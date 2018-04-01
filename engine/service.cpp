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
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
//#include <sys/stat.h> 
#include <fcntl.h>
#include <zlib.h>

#include <jack/jack.h>
#include <jack/midiport.h>
#include "../common/network.h"
#include "../common/udpstruct.h"
#include "../common/request.h"
#include "framecollection.h"
#include "internalclick.h"
#include "state.h"
#include "section.h"
#include "track.h"
#include "config.h"
#include "core.h"
#include "service.h"

#define BUFFER_SIZE 256

unsigned char buffer[BUFFER_SIZE];
char stemplate[64];

bool download_state_request = true;

int service_upload(Track *track) {
	// upload the indicated track to server
	int left_fd, right_fd, midi_fd, stream_fd;
	int readcnt;
	// first thing in the output stream is the track data from class instance
	long cnt, streamlen = sizeof(Track);
	// reset these for our count
	track->left_complen = track->right_complen = 0;
	// transfer the output channels to the temp files	
	if( track->use_left ) {
		strcpy(stemplate, "loopR-XXXXXX");
		left_fd = mkstemp(stemplate);
		track->write_left( left_fd );
		cnt = lseek( left_fd, 0, SEEK_CUR );
		streamlen += cnt;
		track->left_complen = cnt;
	}

	if( track->use_right ) {
		strcpy(stemplate, "loopR-XXXXXX");
		right_fd = mkstemp(stemplate);
		track->write_right( right_fd );
		cnt = lseek( right_fd, 0, SEEK_CUR );
		streamlen += cnt;
		track->right_complen = cnt;
	}

	if( track->use_midi ) {
		strcpy(stemplate, "loopR-XXXXXX");
		midi_fd = mkstemp(stemplate);
		track->write_midi( midi_fd );
		streamlen += lseek( midi_fd, 0, SEEK_CUR );
	}

	// now that we know the total length, set up for the put transfer
	network.putter_connect();
	stream_fd = network.start_put( streamlen );
	
	// write the track data first
	write( stream_fd, track, sizeof( Track ) );
	
	if( track->use_left ) {
		lseek( left_fd, 0, SEEK_SET );
		while( (readcnt = read( left_fd, buffer, BUFFER_SIZE )) > 0 )
			write( stream_fd, buffer, readcnt );
		close( left_fd );
	}
	
	if( track->use_right ) {
		lseek( right_fd, 0, SEEK_SET );
		while( (readcnt = read( right_fd, buffer, BUFFER_SIZE )) > 0 )
			write( stream_fd, buffer, readcnt );
		close( right_fd );
	}
	
	if( track->use_midi ) {
		lseek( midi_fd, 0, SEEK_SET );
		while( (readcnt = read( midi_fd, buffer, BUFFER_SIZE )) > 0 )
			write( stream_fd, buffer, readcnt );
		close( midi_fd );
	}
	
	// finish the put by getting the id back and updating track
	network.end_put( &track->unique_ident );
	network.putter_disconnect();
	
	fprintf(stderr, "service_upload: sent track %s with %d bytes, unique_id=%d\n", 
		track->name, (int)streamlen, track->unique_ident );
	return 0;
}

int service_download(unsigned int id, Track *track) {
	// dowload the indicated track from server
	int left_fd, right_fd, midi_fd, stream_fd;
	int readreq, readcnt, reqcnt;
	long streamlen, streamcnt;
	
	// set up for the get transfer and receive the stream socket fd
	stream_fd = network.start_get( id, &streamlen );

	// read the track data first
	read( stream_fd, track, sizeof( Track ) );
	streamcnt = streamlen - sizeof( Track );
	
	// update the ident value in the track itself and
	// clear the loadcount
	track->unique_ident = id;
	track->loadcount = 0;
	
	// now that we have the track data we know some things
	if( track->use_left ) {
		strcpy(stemplate, "loopR-XXXXXX");
		left_fd = mkstemp(stemplate);
		if( (reqcnt = track->left_complen) > streamcnt ) {
			// error condition
		}
		while( reqcnt ) {
			readreq = ( reqcnt > BUFFER_SIZE) ? BUFFER_SIZE : reqcnt;
			if( (readcnt = read( stream_fd, buffer, readreq )) <= 0 ) {
				// error condition
				}
			write( left_fd, buffer, readcnt );
			reqcnt -= readcnt;
			streamcnt -= readcnt;
		}
	}
	
	if( track->use_right ) {
		strcpy(stemplate, "loopR-XXXXXX");
		right_fd = mkstemp(stemplate);
		if( (reqcnt = track->left_complen) > streamcnt ) {
			// error condition
		}
		while( reqcnt ) {
			readreq = ( reqcnt > BUFFER_SIZE) ? BUFFER_SIZE : reqcnt;
			if( (readcnt = read( stream_fd, buffer, readreq )) <= 0 ) {
				// error condition
				}
			write( right_fd, buffer, readcnt );
			reqcnt -= readcnt;
			streamcnt -= readcnt;
		}
	}
	
	if( track->use_midi ) {
		strcpy(stemplate, "loopR-XXXXXX");
		midi_fd = mkstemp(stemplate);
		reqcnt = streamcnt;		// midi data occupies the rest of data stream
		if( reqcnt <= 0 ) {
			// error condition
		}
		while( reqcnt ) {
			readreq = ( reqcnt > BUFFER_SIZE) ? BUFFER_SIZE : reqcnt;
			if( (readcnt = read( stream_fd, buffer, readreq )) <= 0 ) {
				// error condition
				}
			write( midi_fd, buffer, readcnt );
			reqcnt -= readcnt;
			streamcnt -= readcnt;
		}
	}
	
	// ok, now read these back into the track
	if( track->use_left ) {
		lseek( left_fd, 0, SEEK_SET );
		track->read_left( left_fd, track->use_right );
		close( left_fd );
	}

	if( track->use_right ) {
		lseek( left_fd, 0, SEEK_SET );
		track->read_right( right_fd );
		close( right_fd );
	}

	if( track->use_midi ) {
		lseek( left_fd, 0, SEEK_SET );
		track->read_midi( midi_fd );
		close( midi_fd );
	}
	
	// any last adjustments
	track->playback = track->head;
	
	fprintf(stderr, "service_download: recieved track %s with %d bytes\n", track->name, (int)streamlen );
	return 0;
}

//
// Upload thread is used in jack.cpp and control.cpp
//
void *service_upload_thread(void *arg) {
	if( arg == NULL ) {
		// upload the state and sections
		fprintf(stderr, "upload thread, uploading state and section data\n" );
		
		// precautionary, a client using hosting can't clear these anymore
		state.framecount = 0;
		state.current_section = 0;
		state.recording = false;
		state.stagerecord = false;		

		// make the transfer
		network.putter_connect();
		network.put_complete(&state, true, false, false, sizeof( State ));
		network.put_complete(sections, false, true, false, sizeof(Section) * MAX_SECTIONS );
		network.putter_disconnect();
		return NULL;
	}
	Track *track = (Track *)arg;
	fprintf(stderr, "upload thread, uploading track %s\n", track->name );
	service_upload( track );
	return NULL;
}

void *service_download_thread( void *arg ) {
	
	unsigned int oldtrackmax, newtrackmax, chatmax;
	int trackcount;
	bool *init_server_req = (bool*) arg;
	
	fprintf(stderr, "download thread, init_server_request=%d\n", *init_server_req );
	// normally we will read the state and sections from server right away and continue
	if( *init_server_req )
		download_state_request = false;
		
	while( running ) {
		// look for state download request and 
		// make sure we are not playing
		if( download_state_request && !state.playing ) {
			download_state_request = false;
			// save the track count - we will update the received statebuffer with it
			trackcount = state.trackcount;
			// presently, we can only get a download state request
			// while not playing - otherwise we would have to more locking here
			network.get_complete(&state, true, false, false, sizeof( State ));
			// update the track count
			state.trackcount = trackcount;
			network.get_complete(sections, false, true, false, sizeof(Section) * MAX_SECTIONS );			
			fprintf(stderr, "(service_download_task) fetched state and section data\n");
		}
		
		// fetch a fresh max ident
		//fprintf(stderr, "(service_download_task) fetching max ident...");
		network.get_maxids( &newtrackmax, &chatmax );
		fprintf(stderr, " max ident(track/chat) = %d/%d\n", newtrackmax, chatmax);

		// look thru the track list and get the max id we have
		oldtrackmax = 0;
		Track *track = TrackHead;
		while( track ) {
			if( track->unique_ident > oldtrackmax ) oldtrackmax = track->unique_ident;
			track = track->next;
		}
		
		// see if there is any work
		if( oldtrackmax < newtrackmax) {
			
			// fetch any tracks we are lacking
			while( oldtrackmax < newtrackmax ) {
				track = new Track;
				fprintf(stderr, "(service_download_task) requesting download\n");
				service_download( ++oldtrackmax, track );
				
				// lock before append track to list
				fprintf(stderr, "(service_download_task) acquiring lock...");
				pthread_mutex_lock( &append_track_mut );
				fprintf(stderr, "continuing\n");
				
				if( TrackTail == NULL ) {
					TrackTail = TrackHead = track;
					track->next = track->prev = NULL;
				}
				else {
					track->prev = TrackTail;
					track->next = NULL;
					TrackTail->next = track;
					TrackTail = track;
				}
				state.trackcount++;
				fprintf(stderr, "(service_download_task) freeing lock\n");
				pthread_mutex_unlock( &append_track_mut );
			}
			
		} else 
			sleep(1);
	}

	fprintf(stderr, "(service_download_task) exiting\n");
	return NULL;
}
