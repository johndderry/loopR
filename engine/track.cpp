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
#include <unistd.h> 
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <zlib.h>

#include "../common/network.h"
#include "../common/udpstruct.h"
#include "framecollection.h"
#include "internalclick.h"
#include "state.h"
#include "section.h"
#include "track.h"
#include "config.h"
#include "core.h"

Track::Track( ) {
	head = tail = playback = NULL;
	collcount = loadcount = 0;
}

Track::Track(int partnum, int nameseq ) {
	head = tail = playback = NULL;
	collcount = loadcount = 0;
	part = partnum;
	bank = state.bank;
	program = state.program;
	channel = last_channel;
	use_left = state.rec_left;
	use_right = state.rec_right;
	use_midi = state.rec_midi;
	mute = solo = remove = longtrack = start = false;
	volume_left = volume_right = volume_midi = 1.0f;
	peak_left = peak_right = peak_midi = 0;
	next = prev = NULL;
	unique_ident = 0;

	// don't allow blank trackname
	// future - don't allow duplicate trackname
	// for the time being, removing trailing blanks
	int len = strlen(trackname);
	while( len-- )
		if( trackname[len] == ' ' ) trackname[len] = '\0';
		
	if( trackname[0] ) {
		if( track_hosting ) {
			strcpy( name, clientname );
			int len = strlen( name );
			strncat( name, trackname, TRACK_NAME_MAX - len);
		} else
			strcpy( name, trackname );
	}
	else {
		if( track_hosting && clientname[0] ) 
			strcpy( name, clientname );
		else
			sprintf(name, "track%d",  nameseq);
	}
}

Track::~Track() {

	FrameCollection *ptr;

	if( loadcount && tail ) {
		/* delete any collections */
		while( loadcount-- && (ptr=tail) ) {
			tail = ptr->get_prev();
			delete ptr;
		}
	}
}

bool Track::advance( ) {

	playback = playback->get_next();
	if( playback == NULL && !longtrack ) {
		playback = head;
		return true;
	}
	return false;
}

void Track::sum( FrameCollection *f, unsigned count ) {
	sum( f, count, 1.0f, 1.0f, 1.0f );
}

void Track::sum( FrameCollection *f, unsigned count, float factor_left, float factor_right, float factor_midi ) {
	//
	//	Sum any audio frames and midi events from the framecollection at the playback pointer
	//	into the collection passed to us, applying factors as we go
	//
	//	count represents the desired frame count for summing, but that may not reflect the
	//	actual frames in the audio sample

	if( use_left && use_right ) {
	
		float *sumf_left, *framef_left, fpeak_left = 0.0f;
		float *sumf_right, *framef_right, fpeak_right = 0.0f;
		int cnt = count;

		// cast the frame buffers into floats so we can sum them
		sumf_left = (float *) f->get_frames_left();
		framef_left = (float *) playback->get_frames_left();
		sumf_right = (float *) f->get_frames_right();
		framef_right = (float *) playback->get_frames_right();

		// sum all audio frames available while obtaining peak values
		while( cnt-- && framef_left && framef_right ) {
			if( *framef_left * volume_left > fpeak_left ) fpeak_left = *framef_left * volume_left; 
			*sumf_left = *sumf_left + (*framef_left++) * volume_left * factor_left;

			if( *framef_right * volume_right > fpeak_right ) fpeak_right = *framef_right * volume_right; 
			*sumf_right = *sumf_right + (*framef_right++) * volume_right * factor_right;

			sumf_right++; sumf_left++; 
			}

		peak_left = (int) (fpeak_left * 100.0f);
		peak_right = (int) (fpeak_right * 100.0f);
	} 
	else if( use_left ) {
		// place channel in left/right according to levels
		float *sumf_left, *framef_left, fpeak_left = 0.0f;
		float *sumf_right;
		int cnt = count;

		// cast the frame buffers into floats so we can sum them
		sumf_left = (float *) f->get_frames_left();
		sumf_right = (float *) f->get_frames_right();
		framef_left = (float *) playback->get_frames_left();

		// sum all audio frames available while obtaining peak values
		while( cnt-- && framef_left ) {
			if( *framef_left * volume_left > fpeak_left ) fpeak_left = *framef_left * volume_left; 
			*sumf_left = *sumf_left + (*framef_left) * volume_left * factor_left;
			*sumf_right = *sumf_right + (*framef_left++) * volume_right * factor_right;

			sumf_left++; sumf_right++;
			}

		peak_left = (int) (fpeak_left * 100.0f);
	}
	else if( use_right ) {
	
		float *sumf_right, *framef_right, fpeak_right = 0.0f;
		float *sumf_left;
		int cnt = count;

		// cast the frame buffers into floats so we can sum them
		sumf_left = (float *) f->get_frames_left();
		sumf_right = (float *) f->get_frames_right();
		framef_right = (float *) playback->get_frames_right();

		// sum all audio frames available while obtaining peak values
		while( cnt-- && framef_right ) {
			if( *framef_right * volume_right > fpeak_right ) fpeak_right = *framef_right * volume_right; 
			*sumf_left = *sumf_left + (*framef_right) * volume_left * factor_left;
			*sumf_right = *sumf_right + (*framef_right++) * volume_right * factor_right;

			sumf_left++; sumf_right++; 
			}

		peak_right = (int) (fpeak_right * 100.0f);
	}
	
	if( use_midi ) {

		int ipeak = 0;

		// for all midi events at playback, insert them into passed collection
		jack_nframes_t n, ncount = playback->get_nevents();
		jack_midi_event_t *events = playback->get_events();
		for( n = 0; n < ncount; n++ ) {

			if( events[n].size >= 3 && ((events[n].buffer[0] & 0xf0) == 0x90 ))
				if( events[n].buffer[2] > ipeak ) ipeak = events[n].buffer[2];

			// insert each event into collection, anding the channel and apply volume at that time
			f->insert_midi_event( &events[n], channel, volume_midi * factor_midi );
		}
		peak_midi = ( ipeak * 100 ) / 128;
	}
}

void Track::send_notesoff_midi() {
	// send out channel initialization: all notes off messages
	unsigned char message[3];

	// send out all notes off
	message[0] = 0xb0 | channel;
	message[1] = 0x7b;
	message[2] = 0;
	jack_send_midi( 3, message );
}
	
void Track::send_channel_midi() {
	// send out channel initialization:  bank and program midi messages
	unsigned char message[2];

	// send program message
	message[0] = 0xc0 | channel;
	message[1] = program;
	jack_send_midi( 2, message );
}

int Track::nevents() {
	int total = 0, count = collcount;
	FrameCollection *fc = head;
	while( count-- && fc ) { 
		total+= fc->get_nevents();
		fc = fc->get_next();
	}
	return total;
}

int Track::write_left( int fd ) {
	bool compress_error = false, continue_collection = false;
	int cmpcount = 0, count = collcount;
	FrameCollection *fc = head;
	unsigned char out[ NFRAMES*sizeof( jack_default_audio_sample_t ) ];
	z_stream s;
	
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;

	if( deflateInit( &s, Z_DEFAULT_COMPRESSION) != Z_OK ) return 0;

	s.avail_out = NFRAMES*sizeof( jack_default_audio_sample_t ) ;
	s.next_out = out;
	
	fprintf(stderr, "writing %d collections: %d bytes uncompressed, ", count, 
		(int) (collcount * NFRAMES * sizeof( jack_default_audio_sample_t ) ));

	while( count && fc && !compress_error ) {
		
		//fprintf(stderr, "\ncount %d: ", count ); 
		switch( fc->write_left( &s, continue_collection ) ) {
			case -1: continue_collection = true; 	break;
			case  0: compress_error = true;			break;
			case  1: 
				count--;
				fc = fc->get_next();
				continue_collection = false;
				break;
		}
		
		if( s.avail_out < NFRAMES*sizeof( jack_default_audio_sample_t )) {
			// write out compressed data and reset output buffer

			//fprintf(stderr, "write %d compressed", (int)(NFRAMES*sizeof( jack_default_audio_sample_t ) - s.avail_out)); 
			write( fd, out, (NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out );
			cmpcount += ((NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out );
			s.avail_out = NFRAMES*sizeof( jack_default_audio_sample_t ) ;
			s.next_out = out;
		}
	}

	s.avail_in = 0;
	s.next_in = Z_NULL;
	do { 
		s.avail_out = NFRAMES*sizeof( jack_default_audio_sample_t ) ;
		s.next_out = out;
		
		deflate( &s, Z_FINISH );

		//fprintf(stderr, "\n(flush %d bytes compressed) ", 
		//	(int)(NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out);
		write( fd, out, (NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out );
		cmpcount += (NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out;
		
	} while( s.avail_out < (NFRAMES*sizeof( jack_default_audio_sample_t )));
	
	fprintf(stderr, "%d bytes compressed\n", cmpcount );
	
	deflateEnd( &s );
	
	return 1;
}
	
int Track::write_right( int fd ) {
	bool compress_error = false, continue_collection = false;
	int cmpcount = 0, count = collcount;
	FrameCollection *fc = head;
	unsigned char out[ NFRAMES*sizeof( jack_default_audio_sample_t ) ];
	z_stream s;
	
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;

	if( deflateInit( &s, Z_DEFAULT_COMPRESSION) != Z_OK ) return 0;

	s.avail_out = NFRAMES*sizeof( jack_default_audio_sample_t ) ;
	s.next_out = out;
	
	fprintf(stderr, "writing %d collections: %d bytes uncompressed, ", count, 
		(int) (collcount * NFRAMES * sizeof( jack_default_audio_sample_t ) ));

	while( count && fc && !compress_error ) {
		
		//fprintf(stderr, "\ncount %d: ", count ); 

		switch( fc->write_right( &s, continue_collection ) ) {
			case -1: continue_collection = true; 	break;
			case  0: compress_error = true;			break;
			case  1: 
				count--;
				fc = fc->get_next();
				continue_collection = false;
				break;
		}
		
		if( s.avail_out < NFRAMES*sizeof( jack_default_audio_sample_t )) {
			// write out compressed data and reset output buffer

			//fprintf(stderr, "write %d compressed", (int)(NFRAMES*sizeof( jack_default_audio_sample_t ) - s.avail_out)); 
			write( fd, out, (NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out );
			cmpcount += ((NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out );
			s.avail_out = NFRAMES*sizeof( jack_default_audio_sample_t ) ;
			s.next_out = out;
		}
	}

	s.avail_in = 0;
	s.next_in = Z_NULL;
	do { 
		s.avail_out = NFRAMES*sizeof( jack_default_audio_sample_t ) ;
		s.next_out = out;
		
		deflate( &s, Z_FINISH );

		//fprintf(stderr, "\n(flush %d bytes compressed) ", 
		//	(int)(NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out);
		write( fd, out, (NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out );
		cmpcount += (NFRAMES*sizeof( jack_default_audio_sample_t )) - s.avail_out;
		
	} while( s.avail_out < (NFRAMES*sizeof( jack_default_audio_sample_t )));
	
	fprintf(stderr, "%d bytes compressed\n", cmpcount );
	
	deflateEnd( &s );
	
	return 1;
}
	
int Track::write_midi( int fd ) {
	int count = 0;
	FrameCollection *fc = head;
	while( count < collcount && fc ) { 

		fc->write_midi( fd, count++ );
		fc = fc->get_next();
	}
	return 0;
}
	
int Track::read_left( int fd, bool use_right ) {
	// when reading into track, collcount has the count when we wrote it
	// we will keep a load count while loading, then compare results
	bool stream_error = false;
	int cmpcount, uncmpcount = 0;
	int readcnt, cnt;
	FrameCollection  *fc;
	unsigned char *p, *q, in[ NFRAMES*sizeof( jack_default_audio_sample_t ) ];
	z_stream s;
	
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;
	s.avail_in = 0;
	s.next_in = Z_NULL;
	
	if( inflateInit( &s ) != Z_OK ) return 0;
	
	// preload the input
	readcnt = read( fd, in, NFRAMES*sizeof( jack_default_audio_sample_t ));
	s.next_in = in;
	s.avail_in = readcnt;
	cmpcount = readcnt;
	
	fprintf(stderr, "looking for %d collections, preread %d compressed bytes\n", collcount, readcnt );
	
	if( loadcount == 0 ) {
		// no collections yet, just read our samples until done
		head = tail = NULL;
		while( readcnt && loadcount < collcount && !stream_error ) {
			//fprintf(stderr, "\nCollection %d: ", loadcount );
			fc = new FrameCollection(NFRAMES, true, use_right);
			fc->append( &head, &tail );
			switch( fc->read_left( &s, NULL )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					loadcount++; 
					uncmpcount += NFRAMES * sizeof( jack_default_audio_sample_t );
					break;
			}
			if( s.avail_in < NFRAMES*sizeof( jack_default_audio_sample_t )) {
				// relocate any leftover bytes to the front of buffer
				if( (cnt = s.avail_in) > 0 ) {
					//fprintf(stderr, "relocating %d bytes, ", cnt );
					q = s.next_in;
					p = in;
					while( cnt-- )
						*p++ = *q++;
				}
				s.next_in = in;
				// fill out the input buffer for next pass			
				readcnt = read( fd, in + s.avail_in, NFRAMES*sizeof( jack_default_audio_sample_t ) - s.avail_in );
				//fprintf(stderr, "read %d additional bytes", readcnt );
				s.avail_in += readcnt;
				cmpcount += readcnt;
			}
		}
		if( readcnt != 0 || loadcount != collcount ) 
			fprintf(stderr, "error reading track: readcnt=%d loadcount=%d\n", readcnt, loadcount );
	}
	else if( loadcount == collcount ) {
		// all collections created, go thru the list and read our data
		fc = head;
		while( readcnt && fc && !stream_error ) {
			switch( fc->read_left( &s, NULL )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					fc = fc->get_next();
					loadcount++; 
					uncmpcount += NFRAMES * sizeof( jack_default_audio_sample_t );
					break;
			}
			if( s.avail_in < NFRAMES*sizeof( jack_default_audio_sample_t )) {
				// relocate any leftover bytes to the front of buffer
				if( (cnt = s.avail_in) > 0 ) {
					//fprintf(stderr, "relocating %d bytes, ", cnt );
					q = s.next_in;
					p = in;
					while( cnt-- )
						*p++ = *q++;
				}
				s.next_in = in;
				// fill out the input buffer for next pass			
				readcnt = read( fd, in + s.avail_in, NFRAMES*sizeof( jack_default_audio_sample_t ) - s.avail_in );
				//fprintf(stderr, "read %d additional bytes", readcnt );
				s.avail_in += readcnt;
				cmpcount += readcnt;
			}
		}
		if( readcnt != 0 || fc ) 
			fprintf(stderr, "error reading track: readcnt=%d fc=%x\n", readcnt, (int)((long)fc & 0xffffffff) );

	} else {
		fprintf(stderr, "loadcount != collcount in Track::read_left()\n");
		// some of the collections have been created
	}

	inflateEnd( &s);
	fprintf(stderr, "read %d total compressed bytes, uncompressed = %d\n", cmpcount, uncmpcount );
	
	return 0;
}

int Track::read_right( int fd ) {
	// when reading into track, collcount has the count when we wrote it
	// we will keep a load count while loading, then compare results
	bool stream_error = false;
	int cmpcount, uncmpcount = 0;
	int readcnt, cnt;
	FrameCollection  *fc;
	unsigned char *p, *q, in[ NFRAMES*sizeof( jack_default_audio_sample_t ) ];
	z_stream s;
	
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;
	s.avail_in = 0;
	s.next_in = Z_NULL;
	
	if( inflateInit( &s ) != Z_OK ) return 0;
	
	// preload the input
	readcnt = read( fd, in, NFRAMES*sizeof( jack_default_audio_sample_t ));
	s.next_in = in;
	s.avail_in = readcnt;
	cmpcount = readcnt;
	
	fprintf(stderr, "looking for %d collections, preread %d compressed bytes\n", collcount, readcnt );
	
	if( loadcount == 0 ) {
		// no collections yet, just read our samples until done
		head = tail = NULL;
		while( readcnt && loadcount < collcount && !stream_error ) {
			//fprintf(stderr, "\nCollection %d: ", loadcount );
			fc = new FrameCollection(NFRAMES, false, true);
			fc->append( &head, &tail );
			switch( fc->read_right( &s, NULL )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					loadcount++; 
					uncmpcount += NFRAMES * sizeof( jack_default_audio_sample_t );
					break;
			}
			if( s.avail_in < NFRAMES*sizeof( jack_default_audio_sample_t )) {
				// relocate any rightover bytes to the front of buffer
				if( (cnt = s.avail_in) > 0 ) {
					//fprintf(stderr, "relocating %d bytes, ", cnt );
					q = s.next_in;
					p = in;
					while( cnt-- )
						*p++ = *q++;
				}
				s.next_in = in;
				// fill out the input buffer for next pass			
				readcnt = read( fd, in + s.avail_in, NFRAMES*sizeof( jack_default_audio_sample_t ) - s.avail_in );
				//fprintf(stderr, "read %d additional bytes", readcnt );
				s.avail_in += readcnt;
				cmpcount += readcnt;
			}
		}
		if( readcnt != 0 || loadcount != collcount ) 
			fprintf(stderr, "error reading track: readcnt=%d loadcount=%d\n", readcnt, loadcount );
	}
	else if( loadcount == collcount ) {
		// all collections created, go thru the list and read our data
		fc = head;
		while( readcnt && fc && !stream_error ) {
			switch( fc->read_right( &s, NULL )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					fc = fc->get_next();
					loadcount++; 
					uncmpcount += NFRAMES * sizeof( jack_default_audio_sample_t );
					break;
			}
			if( s.avail_in < NFRAMES*sizeof( jack_default_audio_sample_t )) {
				// relocate any rightover bytes to the front of buffer
				if( (cnt = s.avail_in) > 0 ) {
					//fprintf(stderr, "relocating %d bytes, ", cnt );
					q = s.next_in;
					p = in;
					while( cnt-- )
						*p++ = *q++;
				}
				s.next_in = in;
				// fill out the input buffer for next pass			
				readcnt = read( fd, in + s.avail_in, NFRAMES*sizeof( jack_default_audio_sample_t ) - s.avail_in );
				//fprintf(stderr, "read %d additional bytes", readcnt );
				s.avail_in += readcnt;
				cmpcount += readcnt;
			}
		}
		if( readcnt != 0 || fc ) 
			fprintf(stderr, "error reading track: readcnt=%d fc=%x\n", readcnt, (int)((long)fc & 0xffffffff) );

	} else {
		fprintf(stderr, "loadcount != collcount in Track::read_right()\n");
		// some of the collections have been created
	}

	inflateEnd( &s);
	fprintf(stderr, "read %d total compressed bytes, uncompressed = %d\n", cmpcount, uncmpcount );
	
	return 0;
}

int Track::read_midi( int fd ) {
	struct disk_midi_event ee;

	if( loadcount == 0 ) {
		// no collections yet, create the entire set ahead of time
		head = tail = NULL;
		FrameCollection  *fc;
		while( loadcount < collcount  ) { 
			fc = new FrameCollection(NFRAMES, false, false);
			fc->append( &head, &tail );
			loadcount++;
		}
	}
	else if( loadcount < collcount ) {
		fprintf(stderr,"can't handle this case\n");
		return 0;
	}

	// all collections created, go thru the list and read our data
	// pre-read first event
	FrameCollection *fc = head;
	int res;
	unsigned coll = 0;

	res = read( fd, &ee, sizeof(struct disk_midi_event));
	
	while(  fc && res ) {

		// find the correct collection
		while( coll < ee.collection ) {
			fc = fc->get_next();
			coll++;
		}
		// read this midi event into frame collection
		fc->read_midi( &ee );
		// get next disk event
		res = read( fd, &ee, sizeof(struct disk_midi_event));
	}
		
	return 0;
}

void Track::resample( ) {
}

void Track::reverse() {
}

