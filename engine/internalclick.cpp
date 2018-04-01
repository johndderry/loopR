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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include "../common/network.h"
#include "../common/udpstruct.h"
#include "framecollection.h"
#include "internalclick.h"
#include "state.h"
#include "section.h"
#include "track.h"
#include "config.h"
#include "core.h"

#define BUFFER_LEN 256

InternalClick::InternalClick() {

	beathead = downhead = fillhead = beattail = downtail = filltail = playback = NULL;
	beatcollcount = downcollcount = fillcollcount = playcount = 0;
	
	if( config.cinternal == false ) return;
	
	volume_left = volume_right = 1.0f;
	
	FrameCollection  *fc;
	char filename[BUFFER_LEN];
	bool stream_error = false;
	int fd, cmpcount, uncmpcount = 0, readcnt, loadcount = 0;
	unsigned cnt, count;

	unsigned char *p, *q, in[ NFRAMES*sizeof( jack_default_audio_sample_t ) ];
	z_stream s;
	
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;
	
	fprintf(stderr, "InternalClick() looking for compressed data files... " ); 

	// read the beat compressed samples
	s.avail_in = 0;
	s.next_in = Z_NULL;
	
	if( inflateInit( &s ) != Z_OK ) return;
	
	strcpy( filename, getenv("HOME")) ;
	strcat( filename, "/.loopR/beatclick.zz" );
	fd = open( filename,  O_RDONLY);
	if( fd > 0 ) {
		// preload the input for inflate
		fprintf(stderr, "found beatclick.zz\n" ); 
		readcnt = read( fd, in, NFRAMES*sizeof( jack_default_audio_sample_t ));
		s.next_in = in;
		s.avail_in = readcnt;
		cmpcount = readcnt;

		while( readcnt && !stream_error ) { 
			fc = new FrameCollection(NFRAMES, true, false);
			fc->append( &beathead, &beattail );			
			switch( fc->read_left( &s, &count )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					loadcount++; 
					uncmpcount += count /* NFRAMES * sizeof( jack_default_audio_sample_t ) */;
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
		// make an adjustment to the last collection
		if( count < NFRAMES * sizeof( jack_default_audio_sample_t )) {
			fc->adjustframes( count/sizeof( jack_default_audio_sample_t ) );
			fprintf(stderr, "adjusting last collection nframes to equal %d\n", 
				(int) (count/sizeof( jack_default_audio_sample_t )) );
		} 
			
		close( fd );
		beatcollcount = loadcount;
	}

	// read the downbeat compressed samples
	s.avail_in = 0;
	s.next_in = Z_NULL;
	
	if( inflateInit( &s ) != Z_OK ) return;
	
	strcpy( filename, getenv("HOME")) ;
	strcat( filename, "/.loopR/downclick.zz" );
	loadcount = 0;
	stream_error = false;
	fd = open( filename,  O_RDONLY);
	if( fd > 0 ) {
		// preload the input for inflate
		fprintf(stderr, "found downclick.zz\n" ); 
		readcnt = read( fd, in, NFRAMES*sizeof( jack_default_audio_sample_t ));
		s.next_in = in;
		s.avail_in = readcnt;
		cmpcount = readcnt;

		while( readcnt && !stream_error ) { 
			fc = new FrameCollection(NFRAMES, true, false);
			fc->append( &downhead, &downtail );			
			switch( fc->read_left( &s, &count )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					loadcount++; 
					uncmpcount += count /* NFRAMES * sizeof( jack_default_audio_sample_t ) */;
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
		// make an adjustment to the last collection
		if( count < NFRAMES * sizeof( jack_default_audio_sample_t )) {
			fc->adjustframes( count/sizeof( jack_default_audio_sample_t ) );
			fprintf(stderr, "adjusting last collection nframes to equal %d\n", 
				(int)(count/sizeof( jack_default_audio_sample_t )) );
		} 
			
		close( fd );
		downcollcount = loadcount;
	}

	strcpy( filename, getenv("HOME")) ;
	strcat( filename, "/.loopR/fillclick.zz" );
	loadcount = 0;
	stream_error = false;
	fd = open( filename,  O_RDONLY);
	if( fd > 0 ) {
		// preload the input for inflate
		fprintf(stderr, "found fillclick.zz\n" ); 
		readcnt = read( fd, in, NFRAMES*sizeof( jack_default_audio_sample_t ));
		s.next_in = in;
		s.avail_in = readcnt;
		cmpcount = readcnt;

		while( readcnt && !stream_error ) { 
			fc = new FrameCollection(NFRAMES, true, false);
			fc->append( &fillhead, &filltail );			
			switch( fc->read_left( &s, &count )) {
				case  0: stream_error = true; break;
				case  1: case 2:
					loadcount++; 
					uncmpcount += count /* NFRAMES * sizeof( jack_default_audio_sample_t ) */;
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
		// make an adjustment to the last collection
		if( count < NFRAMES * sizeof( jack_default_audio_sample_t )) {
			fc->adjustframes( count/sizeof( jack_default_audio_sample_t ) );
			fprintf(stderr, "adjusting last collection nframes to equal %d\n", 
				(int)(count/sizeof( jack_default_audio_sample_t )) );
		} 
			
		close( fd );
		fillcollcount = loadcount;
	}
	
	fprintf(stderr, "InternalClick() beatcount=%d downcount=%d fillcount=%d\n", 
		beatcollcount, downcollcount, fillcollcount ); 
}

InternalClick::~InternalClick() {
	
	FrameCollection *fc, *nextfc;
	
	fc = beathead;
	while( fc && beatcollcount-- ) {
		nextfc = fc->get_next();
		delete fc;
		fc = nextfc;
	}
	
	fc = downhead;
	while( fc && downcollcount-- ) {
		nextfc = fc->get_next();
		delete fc;
		fc = nextfc;
	}
}

void InternalClick::begin_beat() {
	if( !beatcollcount ) return;
	playback = beathead;
	playcount = beatcollcount;
}

void InternalClick::begin_down() {
	if( !downcollcount ) {
		begin_beat();
		return;
	}
	playback = downhead;
	playcount = downcollcount;
}

void InternalClick::begin_fill() {
	if( !fillcollcount ) {
		begin_beat();
		return;
	}
	playback = fillhead;
	playcount = fillcollcount;
}

void InternalClick::sum(FrameCollection *f, unsigned count) {
	
	// place channel in middle
	float *sumf_left, *framef_left;
	float *sumf_right;
	int cnt = count;

	// cast the frame buffers into floats so we can sum them
	sumf_left = (float *) f->get_frames_left();
	sumf_right = (float *) f->get_frames_right();
	framef_left = (float *) playback->get_frames_left();

	// sum all audio frames available while obtaining peak values
	while( cnt-- && framef_left ) {
		*sumf_left = *sumf_left + (*framef_left) * volume_left;
		*sumf_right = *sumf_right + (*framef_left++) * volume_right;

		sumf_left++; sumf_right++;
	}
}

void InternalClick::advance() {
	playback = playback->get_next();
	playcount--;
}
