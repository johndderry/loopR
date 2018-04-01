/* MIT License

Copyright (c) 2018 John D. Derry

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h> 

#include <jack/jack.h>
#include <jack/midiport.h>
#include <zlib.h>

#include "framecollection.h"

//
//	FRAMECOLLECTION
//
//	A collection of frames as received from JACK in process().
//	A collection consists of preallocated left and right audio sample buffers,
//	and space for midi events.
//
//	Used as an element of a linked list created during the recording process,
//	which will later become attached to a track.
//

FrameCollection::FrameCollection(jack_nframes_t	nf, bool createleft, bool createright) {
	//
	// possibly allocate space for sample buffers,  but don't initialize
	//
	frames_left = frames_right = NULL;
	if( createleft ) 
		frames_left = new jack_default_audio_sample_t[nf];
	if( createright )
		frames_right = new jack_default_audio_sample_t[nf];
	nframes = nf;
	next = prev = NULL;
	events = NULL;
	nevents = 0;
}

FrameCollection::~FrameCollection() {

	if( nframes ) {
		if( frames_left ) delete[] frames_left;
		if( frames_right) delete[] frames_right;
	}

	if( nevents ) {
		jack_midi_event_t *e = events;
		while( nevents-- ) {
			if(e->size && e->buffer) 
				delete[] e->buffer;
			e++;
		}
		delete[] events;
	}
}

void FrameCollection::zero() {
	//
	// zero out collection by clear audio buffers and delete any midi events
	//
#if 0
	float *framef_left = (float *) frames_left;
	float *framef_right = (float *) frames_right;
	unsigned count = nframes;
	while( count-- ) 
		*framef_left++ = *framef_right++ = 0.0f;
#else
	if( frames_left ) 	memset(frames_left, 0, sizeof(jack_default_audio_sample_t) * nframes );
	if( frames_right ) 	memset(frames_right, 0, sizeof(jack_default_audio_sample_t) * nframes );
#endif

	if( nevents ) {
		jack_midi_event_t *e = events;
		while( nevents-- && e ) {
			if(e->size && e->buffer) 
				delete[] e->buffer;
			e++;
		}
		delete[] events;
		nevents = 0;
		events = NULL;
	}
}

//
// these next set of functions copy audio and midi data in and out of the collection
// they are passed the jack buffer pointers and number of frames in the sample
//

int FrameCollection::copyin_left(jack_default_audio_sample_t *in, jack_nframes_t nframes)
{
	memcpy (frames_left, in, sizeof (jack_default_audio_sample_t) * nframes);
	return 0;
} 

int FrameCollection::copyin_right(jack_default_audio_sample_t *in, jack_nframes_t nframes)
{
	memcpy (frames_right, in, sizeof (jack_default_audio_sample_t) * nframes);
	return 0;
} 

int FrameCollection::copyout_left(jack_default_audio_sample_t *out, jack_nframes_t nframes)
{
	memcpy (out, frames_left, sizeof (jack_default_audio_sample_t) * nframes);
	return 0;
}

int FrameCollection::copyout_right(jack_default_audio_sample_t *out, jack_nframes_t nframes)
{
	memcpy (out, frames_right, sizeof (jack_default_audio_sample_t) * nframes);
	return 0;
}

int FrameCollection::copyin_midi(void *in, jack_nframes_t nframes)
{
	int count = jack_midi_get_event_count( in );
	if( count == 0 ) return 0;

	//fprintf(stderr, "copyin: %d midi events: ", nevents);

	jack_midi_event_t event;

	// determine the peak volume for all midi events processed
	int volume = 0;

	nevents = 0;
	events = new jack_midi_event_t[count];
	while( count-- ) {

		jack_midi_event_get( &event, in, nevents );

		events[nevents].time = event.time;
		events[nevents].size = event.size;
		events[nevents].buffer = new jack_midi_data_t[event.size];

		for(int unsigned i=0;i<event.size;i++) 
			events[nevents].buffer[i] = event.buffer[i];

		nevents++;

		// look for note on, check pressure for peak value
		if( event.size >= 3 && ((event.buffer[0] & 0xf0) == 0x90 ))
			if( event.buffer[2] > volume ) volume = event.buffer[2];

		//	fprintf(stderr, "t=%x(%d) %x %x %x ", event.time, event.size, 
		//		event.buffer[0], event.buffer[1], event.buffer[2] );
	}
	return (volume * 100) / 128;	// return the peak volume as percentage
} 

int FrameCollection::copyout_midi(void *out, jack_nframes_t nframes)
{
	jack_midi_event_t *event;
	if( nevents == 0 ) return 0;
	int volume = 0;
	unsigned index = 0;
	//fprintf(stderr, "copyout %d midi events: ", nevents);
	while( index < nevents ) {
		event = &events[index++];
		jack_midi_event_write( out, event->time, event->buffer, event->size);

		// look for note on, check pressure for peak value
		if( event->size >= 3 && ((event->buffer[0] & 0xf0) == 0x90 ))
			if( event->buffer[2] > volume ) volume = event->buffer[2];
		//fprintf(stderr, "t=%x(%d) %x %x %x ", event->time, event->size, 
		//	event->buffer[0], event->buffer[1], event->buffer[2] );
	}
	return (volume * 100) / 128;
}

void FrameCollection::append(FrameCollection **head, FrameCollection **tail) {
	//
	// append this collection to the linked list passed as references
	//
	if( *tail == NULL )
		*head = *tail = this;
	else {
		prev = *tail;
		(*tail)->next = this;
		*tail = this;
	}
}

//
// some helper methods
//
FrameCollection *FrameCollection::get_next() {
	return next;
}

FrameCollection *FrameCollection::get_prev() {
	return prev;
}

jack_default_audio_sample_t *FrameCollection::get_frames_left() {
	return frames_left;
}

jack_default_audio_sample_t *FrameCollection::get_frames_right() {
	return frames_right;
}

jack_midi_event_t *FrameCollection::get_events() {
	return events;
}

jack_nframes_t FrameCollection::get_nevents() {
	return nevents;
}

void FrameCollection::insert_midi_event( jack_midi_event_t *e, int channel, float volume ) {
	//
	// insert a midi event into the midi event buffer of this collection.
	// the buffer must be order by event offset for jack's sake.
	//
	// while we are at it, OR in the channel passed  to the midi event and apply the volume
	//
	jack_midi_event_t *new_events;
	jack_nframes_t n;
	bool inserted = false;
	
	//fprintf(stderr, "insert_midi_event with channel %d\n", channel);

	// our new event array will have to be one larger
	new_events = new jack_midi_event_t[nevents+1];

	if( nevents == 0 ) {
		// first event
		new_events[0].time = e->time;
		new_events[0].size = e->size;
		new_events[0].buffer = new jack_midi_data_t[ e->size ];
		for(unsigned int i = 0; i < e->size; i++ ) 
			new_events[0].buffer[i] = e->buffer[i];
		new_events[0].buffer[0] |= channel;
		if( (new_events[0].buffer[0] & 0xf0) == 0x90 )
			new_events[0].buffer[2] *= volume;
		nevents++;
		inserted = true;
	} else {
		// look for place to insert
		for( n=0; n<nevents; n++ ) 
			if( events[n].time < e->time )
				// copy the old event over
				new_events[n] = events[n];
			else {
				// this is the place for new event
				new_events[n].time = e->time;
				new_events[n].size = e->size;
				new_events[n].buffer = new jack_midi_data_t[ e->size ];
				for( unsigned int i = 0; i < e->size; i++ ) 
					new_events[n].buffer[i] = e->buffer[i];

				new_events[n].buffer[0] |= channel;
				if( (new_events[n].buffer[0] & 0xf0) == 0x90 )
					new_events[n].buffer[2] *= volume;

				n++; nevents++;
				// copy over the rest of the old events
				while( n < nevents ) {
					new_events[n] = events[n-1];
					n++;
				}
				inserted = true;
				break;
			}
		if( !inserted ) {	// must be appended
			new_events[n].time = e->time;
			new_events[n].size = e->size;
			new_events[n].buffer = new jack_midi_data_t[ e->size ];
			for( unsigned int i = 0; i < e->size; i++ ) 
				new_events[n].buffer[i] = e->buffer[i];

			new_events[n].buffer[0] |= channel;
			if( (new_events[n].buffer[0] & 0xf0) == 0x90 )
				new_events[n].buffer[2] *= volume;

			nevents++;
		}
	}
	delete[] events;
	events = new_events;
}

// input - output methods

int FrameCollection::write_left( z_stream *strm, bool continue_collection ) {

	if( !continue_collection ) {			
		strm->next_in = (unsigned char *)frames_left;
		strm->avail_in = sizeof( jack_default_audio_sample_t ) * nframes;
	}
	//fprintf(stderr, "0: avail_in=%d avail_out=%d ", strm->avail_in, strm->avail_out );
	if( deflate( strm, Z_NO_FLUSH ) == Z_STREAM_ERROR ) {
		fprintf(stderr, "write_left: stream error\n" );
		return 0;
	}
	//fprintf(stderr, "1: avail_in=%d avail_out=%d ", strm->avail_in, strm->avail_out );
	if( strm->avail_in > 0 ) {
		// call for a continuation of this collection
		return -1;
	}
	return 1;
}

int FrameCollection::write_right( z_stream *strm, bool continue_collection ) {

	if( !continue_collection ) {			
		strm->next_in = (unsigned char *)frames_right;
		strm->avail_in = sizeof( jack_default_audio_sample_t ) * nframes;
	}
	if( deflate( strm, Z_NO_FLUSH ) == Z_STREAM_ERROR ) {
		fprintf(stderr, "write_right: stream error\n" );
		return 0;
	}
	if( strm->avail_in > 0 ) {
		// call for a continuation of this collection
		return -1;
	}
	return 1;
}

int FrameCollection::write_left( int fd ) {

	write( fd, frames_left, sizeof( jack_default_audio_sample_t ) * nframes );

	//fprintf(stderr, "wrote %d frames from collection" , nframes );
	return 1;
}

int FrameCollection::write_right( int fd ) {

	write( fd, frames_right, sizeof( jack_default_audio_sample_t ) * nframes );

	//fprintf(stderr, "wrote %d frames from collection" , nframes );
	return 1;
}

int FrameCollection::read_left( z_stream *strm, unsigned *count ) {

	strm->next_out = (unsigned char *)frames_left;
	strm->avail_out = sizeof(jack_default_audio_sample_t) * nframes;
	
	switch( inflate( strm, Z_NO_FLUSH ) ) {
		case Z_NEED_DICT: case Z_DATA_ERROR:
			fprintf(stderr, "read_left: z_stream data error\n");
			return 0;
		case Z_MEM_ERROR:
			fprintf(stderr, "read_left: z_stream memory error\n");
			return 0;
		case Z_STREAM_END:
			fprintf(stderr, "read_left: z_stream reached end\n");
			if( strm->avail_out > 0 ) 
				fprintf(stderr, "read_left: failure to read complete collection\n");
			if( count ) 
				*count = sizeof(jack_default_audio_sample_t) * nframes - strm->avail_out;
			return 2;
	}

	if( strm->avail_out > 0 ) 
		fprintf(stderr, "read_left: failure to read complete collection\n");
		
	if( count ) 
		*count = sizeof(jack_default_audio_sample_t) * nframes - strm->avail_out;
	
	return 1;
}

int FrameCollection::read_right( z_stream *strm, unsigned *count ) {

	strm->next_out = (unsigned char *)frames_right;
	strm->avail_out = sizeof(jack_default_audio_sample_t) * nframes;
	
	switch( inflate( strm, Z_NO_FLUSH ) ) {
		case Z_NEED_DICT: case Z_DATA_ERROR:
			fprintf(stderr, "read_right: z_stream data error\n");
			return 0;
		case Z_MEM_ERROR:
			fprintf(stderr, "read_right: z_stream memory error\n");
			return 0;
		case Z_STREAM_END:
			fprintf(stderr, "read_right: z_stream reached end\n");
			if( strm->avail_out > 0 ) 
				fprintf(stderr, "read_right: failure to read complete collection\n");
			if( count ) 
				*count = sizeof(jack_default_audio_sample_t) * nframes - strm->avail_out;
			return 2;
	}

	if( strm->avail_out > 0 ) 
		fprintf(stderr, "read_right: failure to read complete collection\n");

	if( count ) 
		*count = sizeof(jack_default_audio_sample_t) * nframes - strm->avail_out;
			
	return 1;
}

int FrameCollection::read_left( int fd, unsigned *count ) {

	int readcnt = read( fd, frames_left, sizeof( jack_default_audio_sample_t ) * nframes );
	if( count ) *count = readcnt;
	
	if( (unsigned)readcnt != sizeof( jack_default_audio_sample_t )*nframes )
		return 0;

	//fprintf(stderr, "wrote %d frames from collection" , nframes );
	return 1;
}

int FrameCollection::read_right( int fd, unsigned *count ) {

	int readcnt = read( fd, frames_right, sizeof( jack_default_audio_sample_t ) * nframes );
	if( count ) *count = readcnt;

	if( (unsigned)readcnt != sizeof( jack_default_audio_sample_t )*nframes )
		return 0;

	//fprintf(stderr, "wrote %d frames from collection" , nframes );
	return 1;
}

int FrameCollection::write_midi( int fd, int collection ) {

	if( nevents == 0 ) return 0;

	disk_midi_event 	devent;
	jack_midi_event_t *e = events;
	int i, count = nevents;

	while( count-- ) {

		// fill out the disk event
		devent.time = e->time;
		devent.collection = collection;
		devent.size = e->size;
		for( i = 0; i < devent.size; i++ )
			devent.buffer[i] = e->buffer[i];
		while( i < 7 ) devent.buffer[i++] = 0;

		write( fd, &devent, sizeof(devent) );
		e++;
	}

	//fprintf(stderr, "wrote %d events from collection %d\n", nevents, collection );
	return 0;
}

int FrameCollection::read_midi( struct disk_midi_event *devent  ) {

	jack_midi_event_t *new_events;
	unsigned int i, c = 0;

	new_events = new jack_midi_event_t [nevents+1];
	while( c < nevents ) {
		new_events[c] = events[c];
		c++;
	}
	if( nevents )
		delete[] events;

	new_events[c].time = devent->time;
	new_events[c].size = devent->size;
	new_events[c].buffer = new jack_midi_data_t[devent->size];
	for( i = 0; i < devent->size; i++ )
		new_events[c].buffer[i] = devent->buffer[i];

	nevents++;
	events = new_events;
	
	return 0;
}

void FrameCollection::adjustframes( jack_nframes_t ) {
	
}
