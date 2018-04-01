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
#include "config.h"
#include "framecollection.h"
#include "internalclick.h"
#include "state.h"
#include "section.h"
#include "track.h"
#include "service.h"
#include "core.h"

#define NAMEBUFLEN  64

//
// keep our "globals" here, the ones that don't need to be in Statebuf
//
Config config;

Network network;

InternalClick internal_click;

pthread_mutex_t append_track_mut;

State state;

Track *TrackHead = NULL, *TrackTail = NULL;

//struct Statebuf *statebuffer;

int /*sections[MAX_SECTIONS],*/ last_channel = 1;

Section sections[MAX_SECTIONS];

bool running = true, program_change = false, record_mode = false, track_hosting = false;

char trackname[TRACK_NAME_MAX+1], clientname[TRACK_NAME_MAX+1];

void core_init() {
	//
	// perform any global sequencer initializations here
	//
	trackname[0] = clientname[0] = '\0';
}

int load_sequencer(bool loadtracks) {

	char namebuf[128];

	int fd;
	unsigned int bytes;
	
	// remove any existing tracks
	if( state.trackcount ) {		
		Track *t, *track = TrackTail;
		while( track ) {
			t =  track->prev;
			delete track;
			track = t;
		}
		state.trackcount = 0;
	}

	fd = open("seq-state",  O_RDONLY );
	if( fd < 0 ) {
		perror("failure to open 'seq-state file");
		return 0;
	}

	bytes = read( fd, &state, sizeof( State ) );
	if( bytes < sizeof(State)) return 0;

	bytes = read( fd, sections, sizeof(Section) * state.sections );
	if( bytes < sizeof(int) * state.sections ) return 0;

	int count = state.trackcount;
	Track *track, *ptr;
	while( count-- ) {

		track = new Track();
		bytes = read( fd, track, sizeof(Track) );
		if( bytes < sizeof(Track)) return 0;

		track->loadcount = 0;	// keeps track of whether collections are created yet

		if( TrackHead == NULL ) {
			TrackHead = TrackTail = track;
			track->next = track->prev = NULL;
		}
		else {
			ptr = TrackHead;
			while( ptr->next) ptr = ptr->next;
			ptr->next = track;
			track->prev = ptr;
			track->next = NULL;
			TrackTail = track;
		}
	}

	fprintf(stderr, "read seq-state file\n");
	close( fd );
	
	if( !loadtracks ) return 1;
	
	int sequence = 0;
	track = TrackHead;
	while( track ) {

		if( track->use_left ) {
			sprintf(namebuf, "seq-%d-left.zz", sequence );
			fd = open( namebuf,  O_RDONLY);
			track->read_left( fd, track->use_right );
			close( fd );
		}

		if( track->use_right ) {
			sprintf(namebuf, "seq-%d-right.zz", sequence );
			fd = open( namebuf,  O_RDONLY);
			track->read_right( fd );
			close( fd );
		}

		if( track->use_midi ) {
			sprintf(namebuf, "seq-%d-midi", sequence );
			fd = open( namebuf,  O_RDONLY );
			track->read_midi( fd );
			close( fd );
		}

		// the playback pointers need resetting
		track->playback = track->head;		

		fprintf(stderr, "read track sequence %d name %s\n", sequence, track->name );
		track = track->next;
		sequence++;
	}
	return 1;
}

int flush_sequencer(bool writetracks) {
	//
	// write out sequencer data files
	//
	char namebuf[128];
	int fd;

	fd = open("seq-state",  O_RDWR | O_CREAT | O_TRUNC, 0770 );
	if( fd < 0 ) {
		perror("failure to open 'seq-state file");
		return 0;
	}

	write( fd, &state, sizeof( State ) );
	write( fd, sections, sizeof(Section) * state.sections );

	Track *track = TrackHead;
	while( track ) {

		write( fd, track, sizeof( Track ) );	
		track = track->next;
	}
	fprintf(stderr, "wrote seq-state file\n");
	close( fd );	

	if( !writetracks ) return 1;
	
	int sequence = 0;
	track = TrackHead;
	while( track ) {

		if( track->use_left ) {
			sprintf(namebuf, "seq-%d-left.zz", sequence );
			fd = open( namebuf,  O_RDWR | O_CREAT | O_TRUNC, 0770 );
			track->write_left( fd );
			close( fd );
		}

		if( track->use_right ) {
			sprintf(namebuf, "seq-%d-right.zz", sequence );
			fd = open( namebuf,  O_RDWR | O_CREAT | O_TRUNC, 0770 );
			track->write_right( fd );
			close( fd );
		}

		if( track->use_midi ) {
			sprintf(namebuf, "seq-%d-midi", sequence );
			fd = open( namebuf,  O_RDWR | O_CREAT | O_TRUNC, 0770 );
			track->write_midi( fd );
			close( fd );
		}

		fprintf(stderr, "wrote track seq %d name = %s\n", sequence, track->name );
		track = track->next;
		sequence++;
	}
	return 1;

}

void extend_command( int numb, char *buffer ) {

	// commands for track and section with number preceding

	//fprintf(stderr, "track/division command (%d)%s\n", numb, buffer);
	int trackdivnum, n;
	char *p = buffer, digits[128];
	
	// strip any trailing newline
	if( buffer[numb-1] == '\n' ) {
		fprintf(stderr, "<stripping newline>\n");
		if( --numb == 0 ) {
			fprintf(stderr, "--invalid command - no data\n");
			return;
		}
	}
	
	n = 0;
	while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
	if( n == numb ) {
		fprintf(stderr, "--invalid command, too short\n" );
		return;
	}
	digits[n] = '\0';
	trackdivnum = atoi(digits);
	numb = numb - n;
		
	if( *p == 'a' ) {
		
		// section increase command, check range
		if( trackdivnum >= state.sections ) {
			fprintf(stderr, "--invalid section number\n");
			return;
		}
		sections[trackdivnum].part++;
		if( sections[trackdivnum].part > state.sections )
				sections[trackdivnum].part = 0;
		fprintf(stderr, "section: part now %d\n", sections[trackdivnum].part);
	}		
	else if( *p == 'A' ) {
		
		// section set command, check range
		if( trackdivnum >= state.sections ) {
			fprintf(stderr, "--invalid section number\n");
			return;
		}
		numb--;	p++;
		n = 0;
		while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
		digits[n] = '\0';
		sections[trackdivnum].part = atoi( digits );
		fprintf(stderr, "section: part now %d\n", sections[trackdivnum].part);
	}		
	else {
		
		// track command, locate track		
		Track *trk = TrackHead;
		while( trackdivnum-- && trk) trk = trk->next;
		if( trk == NULL ) {
			fprintf(stderr, "--invalid track number\n");
			return;
		}
		// perform function on track
		switch( *p ) {
			case 'm':
				if( numb < 2 ) { 
					fprintf(stderr, "--invalid mute command\n");
					return;
				}
				if( *(p+1) == '0' ) trk->mute = false;
				else trk->mute = true;
				break;

			case 's':
				if( numb < 2 ) { 
					fprintf(stderr, "--invalid solo command\n");
					return;
				}
				if( *(p+1) == '0' ) trk->solo = false;
				else trk->solo = true;
				break;
				
			case 'v':
				int volleft, volright, volmidi;
				numb--; p++;

				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				if( n == numb ) {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
				digits[n] = '\0';
				volleft = atoi(digits);
				numb = numb - n;

				if( numb > 0 ) {
					numb--; p++; 
				}
				else {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
					
				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				if( n == numb ) {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
				digits[n] = '\0';
				volright = atoi(digits);
				numb = numb - n;

				if( numb > 0 ) {
					numb--; p++; 
				}
				else {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
					
				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				digits[n] = '\0';
				volmidi = atoi(digits);
				numb = numb - n;

				trk->volume_left = (float)volleft / 100.0f;
				trk->volume_right = (float)volright / 100.0f;
				trk->volume_midi = (float)volmidi / 100.0f;
				fprintf(stderr, "--volume: %d %d %d | %f %f %f\n", 
					volleft, volright, volmidi,  trk->volume_left, trk->volume_right, trk->volume_midi );
				break;
				
			case 'b':
				int bank, program;
				numb--; p++;

				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				if( n == numb ) {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
				digits[n] = '\0';
				bank = atoi(digits);
				numb = numb - n;

				if( numb > 0 ) {
					numb--; p++; 
				}
				else {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
					
				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				digits[n] = '\0';
				program = atoi(digits);
				numb = numb - n;

				trk->bank = bank;
				trk->program = program;
				fprintf(stderr, "--bank/program %d / %d\n", bank, program );
				break;
				
			case 'n':
				numb--; p++;
				char namebuf[NAMEBUFLEN];
				n = 0;
				while( n < numb && n < NAMEBUFLEN-1 )
					namebuf[n++] = *p++;
				namebuf[n] = '\0';

				fprintf(stderr, "--name: %s\n", namebuf );
				strncpy(trk->name, namebuf, TRACK_NAME_MAX );
				break;
				
			case 'p':
				numb--; p++;
				if( *p == '-' && *(p+1) == '1') {
					trk->part = -1;
					fprintf(stderr, "--part: %d\n", trk->part );
					break;
				}
				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				digits[n] = '\0';
				trk->part = atoi(digits);
				fprintf(stderr, "--part: %d\n", trk->part );
				break;
				
			case 'c':
				numb--; p++;
				n = 0;
				while( n < numb && isdigit( *p ) ) digits[n++] = *p++;
				digits[n] = '\0';
				trk->channel = atoi(digits);
				fprintf(stderr, "--channel: %d\n", trk->channel );
				break;
				
			case 'd':
				// flag track for deletion
				trk->remove = true;
				break;

			case 'r':
				trk->resample();
				break;

			case 'R':
				trk->reverse();
				break;

			case 'u':
				// upload track to server if this hasn't already occurred
				if( track_hosting && trk->unique_ident == 0 ) {
					pthread_t thread_id;	// this is quite wrong being here
					pthread_create( &thread_id, NULL, service_upload_thread, trk );
				}
				break;

			case 'S':
				// start track - for long tracks, flag track for play
				if( trk->longtrack && trk->playback == NULL ) {
					if( !state.playing && state.framecount == 0 && state.current_section == 0 )
						trk->playback = trk->head;
					else
						trk->start = true;
				}
				break;
		}
	}
}

void tempo(int n) {
	
	if( !state.coupled ) {
		sections[state.current_section].tempo(n);
		return;
	}
	
	for(int i = 0; i < state.sections; i++)
		sections[i].tempo(n);
}

void settempo(char *s) {
	
	if( !state.coupled ) {
		sections[state.current_section].settempo(s);
		return;
	}
	
	for(int i = 0; i < state.sections; i++)
		sections[i].settempo(s);
}

void adj_divisions(int n) {
	
	if( !state.coupled ) {
		sections[state.current_section].adj_divisions(n, state.bpm_mode);
		return;
	}
	
	for(int i = 0; i < state.sections; i++)
		sections[i].adj_divisions(n, state.bpm_mode);
}

void adj_beats(int n) {
	
	if( !state.coupled ) {
		sections[state.current_section].adj_beats(n);
		return;
	}
	
	for(int i = 0; i < state.sections; i++)
		sections[i].adj_beats(n);
}

void do_commands() {

	int numbytes, n;
	char *p, buffer[256], digits[128];
	
	while( running ) {
		numbytes = network.listen(buffer,128);
		if( numbytes < 0 ) continue;
		buffer[numbytes] = '\0';
		fprintf(stderr, "command(%d): %s\n", numbytes, buffer);
		// assume at least one byte arrived
		switch( buffer[0] ) {
			case 'q': running = false; break;
			case 'f': flush_sequencer(true); break;
			case 'L': load_sequencer(true); break;
			case 'x': jack_clear_tracks(); break;
			case 'w': 
				if( track_hosting && state.framecount == 0 && state.current_section == 0 ) 
					download_state_request = true;
				jack_rewind();			
				break;
			case '<': state.back(); break;
			case '>': state.forward(); break;
			case '.': state.playing = false;
				jack_startstop(0);
				break;
			case ',': state.playing = true; 
				jack_startstop(1);
				break;
			case 'r': state.record(); break;
			// these need at least two bytes
			case '=': if(buffer[1] == '1') config.cinternal = true;
					  else config.cinternal = false;
					  if(buffer[2] == '1') config.autoupload = true;
					  else config.autoupload = false;
					  if(buffer[3] == '1') config.longtracks = true;
					  else config.longtracks = false;
				break;
			case 'R': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) 							
							record_mode = state.stagerecord = false;
						else if( buffer[1] == '1' && 
								(state.rec_left || state.rec_right || state.rec_midi)) {
							record_mode = true;
							state.long_record(config.longtracks);
						}							
					} break;
			case 'l': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) state.rec_left = false; 
						else if( buffer[1] == '1' ) state.rec_left = true;
					} break;
			case 'a': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) state.rec_right = false; 
						else if( buffer[1] == '1' ) state.rec_right = true;
					} break;
			case 'm': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) state.rec_midi = false; 
						else if( buffer[1] == '1' ) state.rec_midi = true;
					} break;
			case 'c': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) state.use_click = false; 
						else if( buffer[1] == '1' ) state.use_click = true;
					} break;
			case 'C': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) state.coupled = false; 
						else if( buffer[1] == '1' ) state.coupled = true;
					} break;
			case 'M': if( numbytes > 1 ) {
						if( buffer[1] == '0' ) state.bpm_mode = false; 
						else if( buffer[1] == '1' ) state.bpm_mode = true;
					} break;
			case 'e': if( numbytes > 1 && buffer[1] == '+' ) state.adj_sections(1); 
					  else state.adj_sections(0); break;
			case 't': if( numbytes > 1 ) {
						if( buffer[1] == '+' )		tempo(1); 
						else if( buffer[1] == '-' ) tempo(0);
						else if( numbytes > 3 ) 	settempo(&buffer[1]);
					} break;
			case 'd': if( numbytes > 1 && buffer[1] == '+' ) adj_divisions(1); 
					  else adj_divisions(0);
					  break;
			case 'b': if( numbytes > 1 && buffer[1] == '+' ) adj_beats(1);
					  else adj_beats(0); 
					  break;
			case 'g':
				numbytes--; p = &buffer[1];
				n = 0;
				while( n < numbytes && isdigit( *p ) ) digits[n++] = *p++;
				if( n == numbytes ) {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
				digits[n] = '\0';
				state.bank = atoi(digits);
				numbytes = numbytes - n;

				if( numbytes > 0 ) {
					numbytes--; p++; 
				}
				else {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
					
				n = 0;
				while( n < numbytes && isdigit( *p ) ) digits[n++] = *p++;
				digits[n] = '\0';
				state.program = atoi(digits);
				numbytes = numbytes - n;

				fprintf(stderr, "--global bank/program %d / %d\n", state.bank, state.program );
					unsigned char message[2];
	
				// flag our program change so that next midi track wil get a new channel
				program_change = true;

				// schedule a program message on channel 1
				message[0] = 0xc0;
				message[1] = state.program;
				jack_send_midi( 2, message );
				jack_write_midi();
				break;
				
			case 'v':
				int volume;
				numbytes--;
				if( numbytes < 2 ) {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
				numbytes--;
				p = &buffer[2];
				n = 0;
				while( n < numbytes && (*p == ' ' || isdigit( *p )) ) digits[n++] = *p++;
				digits[n] = '\0';
				volume = atoi(digits);
				switch( buffer[1] ) {
					case 'l':	state.volume_left = (float)volume / 100.00;
						break;
					case 'r':	state.volume_right = (float)volume / 100.00;
						break;
					case 'm':	state.volume_midi = (float)volume / 100.00;
						break;
				}
				fprintf(stderr, "volume %c = %d\n", buffer[1], volume );
				break;
				
			case 'n':
				numbytes--;
				if( numbytes < 1 ) {
					fprintf(stderr, "--invalid command, too short\n" );
					return;
				}
				char namebuf[NAMEBUFLEN];
				n = 0;
				while( n < numbytes && n < NAMEBUFLEN-1 ) {
					namebuf[n] = buffer[n+1];
					n++;
				}
				namebuf[n] = '\0';

				fprintf(stderr, "--name: %s\n", namebuf );
				strncpy( trackname, namebuf, TRACK_NAME_MAX );
				break;
				
			default:
				// look for a digit, if so assume this is a track or section command
				if( isdigit(buffer[0]) ) extend_command( numbytes, buffer );
				else
					// don't know what this is
					fprintf(stderr, "unknown command %s\n", buffer);
				break;
		}
	}
}

/////////////////////////////////////////////////////////////////
//
// JACK STUFF
//
// these globals are used locally to this source
// and have no extern's in the .h file
//

// jack related ports and jack client, sample rate

jack_port_t *input_port_left, *input_port_right, *input_port_midi;
jack_port_t *output_port_left, *output_port_right, *output_port_midi;
jack_client_t *client;
jack_nframes_t sample_rate;

// during recording we create collections of frames
// and string them together in a link list

int collcount = 0;
FrameCollection	*RecHead = NULL, *RecTail = NULL;

// various functions requested from control
// must be done in process() function

bool rewind_request = false, clear_collections = false, clear_tracks = false;

// keeping track of metronome beat

int current_div = -1, beat_count = 0;

// keep track number for appending to new name

int new_track_num = 0;

// count frames processed before sending udp packet

#define MAX_FRAME_COUNT 1024
int frame_count = 0;

// keep a midi buffer for midi events originating from control events

#define MIDI_BUFFER_LEN 128

struct midi_event {
	int len;
	unsigned char *buf;
}  midi_event_buffer[MIDI_BUFFER_LEN];

int midi_buffer_len = 0;
bool midi_buffer_flush = false;

/**************************************************************
 * find_peak_midi() 
 */

int find_peak_midi(void *in, jack_nframes_t nframes)
{
	int count = jack_midi_get_event_count( in );
	if( count == 0 ) return 0;

	jack_midi_event_t event;
	int volume = 0, index = 0;
	//fprintf(stderr, "copyin: %d midi events: ", nevents);
	while( count-- ) {
		jack_midi_event_get( &event, in, index );
		if( event.size >= 3 && ((event.buffer[0] & 0xf0) == 0x90 ))
			if( event.buffer[2] > volume ) volume = event.buffer[2];
	//	fprintf(stderr, "t=%x(%d) %x %x %x ", event.time, event.size, 
	//		event.buffer[0], event.buffer[1], event.buffer[2] );
		index++;
	}
	return (volume * 100) / 128;
} 

/**************************************************************
 * find_peak_audio() 
 */

int find_peak_audio(jack_default_audio_sample_t *in, jack_nframes_t nframes)
{
	float volume = 0.0f;
	float *p = (float *)in;

	while( nframes-- ) { 
		if( *p > volume ) volume = *p;
		p++;
	}
	
	return (int) ( (volume * 100) );
} 


/**************************************************************
 * save_recording()
 * 
 * After creating a list of FrameCollections during the
 * recording process, call this procedure when the sequencer
 * returns to frame zero after the sample period is over, to
 * save off the list into a proper Track.
 */

void save_recording( bool longtrack )
{ 
	//
	// create a new track out of the collection list and 
	// add it to the link list of tracks
	//
	Track *track;

	if( collcount && RecTail ) {

		fprintf(stderr, "new track in part %d\n", sections[state.current_section].part );
		track = new Track( sections[state.current_section].part, new_track_num++ );
		track->head = RecHead;
		track->tail = RecTail;
		track->collcount = track->loadcount = collcount;
		track->longtrack = longtrack;
		
		// lock before append track to list
		if( track_hosting ) pthread_mutex_lock( &append_track_mut );
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
		if( track_hosting ) pthread_mutex_unlock( &append_track_mut );

		//
		// watch for a program change that should occur when using midi
		//
		if( program_change && track->use_midi ) {
			// last_channel found in control.h
			last_channel++;
			if( last_channel == 9 ) last_channel++;
			track->channel = last_channel;
			// must prep this channel for the correct program voice
			track->send_channel_midi();
			midi_buffer_flush = true;
			program_change = false;
		}
		
		// when using track hosting, maybe start a upload thread for this track
		if( track_hosting && config.autoupload ) {
			pthread_t thread_id;	// this is quite wrong being here
			pthread_create( &thread_id, NULL, service_upload_thread, track );
		}
	}

	//
	// zero out the collection list
	//
	RecTail = RecHead = NULL;
	collcount = 0;
}

/***********************************************************************
 * process()
 * 
 * The process callback for JACK. This is called from a real time thread
 * to do the work on each sample set JACK has provided.
 */
 
int process (jack_nframes_t nframes, void *arg)
{
	// create local pointers used for sample buffers for both audio and midi
	// that we will receive from JACK
	jack_default_audio_sample_t *in_left, *in_right, *out_left, *out_right;
	void *in_midi, *out_midi;

	// get our four pointers to the jack audio port buffers
	in_left = (jack_default_audio_sample_t*) jack_port_get_buffer (input_port_left, nframes);
	in_right = (jack_default_audio_sample_t*) jack_port_get_buffer (input_port_right, nframes);
	out_left = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port_left, nframes);
	out_right = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port_right, nframes);

	// get our two pointers to the jack midi port buffers
	in_midi = (void*) jack_port_get_buffer (input_port_midi, nframes);
	out_midi = (void*) jack_port_get_buffer (output_port_midi, nframes);

	// keep track of soloing and reseting of long tracks
	bool soloing = false, resettracks = false;

	// first look for a request to clear all tracks
	if( clear_tracks ) {
		
		Track *a, *p = TrackTail;
		while( p ) {
			a = p->prev;
			delete p;
			p = a;
		}

		TrackHead = TrackTail = NULL;
		state.trackcount = 0;
		// force rewind to rewind sections as well
		state.framecount = 0;
		rewind_request = true;
		clear_tracks = false;
	}

	// next look for rewind request
	if( rewind_request ) {
		
		if( state.framecount == 0 ) {
			state.current_section = 0;
			resettracks = true;
		}
		else
			state.framecount = 0;
		if( !record_mode ) 
			state.recording = state.longrecording = false;
		state.stagerecord = false;
		
		current_div = -1;
		beat_count = 0;
		clear_collections = true;
		rewind_request = false;
	}

	// next look for request to clear collections
	if( clear_collections ) {
		if( collcount ) {
			FrameCollection *q, *p = RecTail;
			while( p ) {
				q = p->get_prev();
				delete p;
				p = q;
			}
			collcount = 0;
			RecHead = RecTail = NULL;
		}
		clear_collections = false;
	}
	
	// ok - on with the main business at hand
	
	if( state.playing ) {
		//
		// Playing. 
		// 1. Check for maxframes reached, which means the sample period is over
		// 2. Check for recording, where we save off the port buffer data
		// 3. Sum track samples from recorded tracks for these frame periods
		// 4. Check for metronome activity, then send out summed samples
		//
		if( state.framecount > sections[state.current_section].maxframes ) {
			//
			// reached max frames, reset sequencer to beginning
			//
			state.framecount = 0;

			// don't increment current_section yet because we need to save tracks with correct value
			// but we need to know what the next section will be
			int nextsection = state.current_section + 1;
			if( nextsection >= state.sections )
				nextsection = 0;

			if( state.recording ) {
				//
				// if recording, save the recorded frames into a new track
				//
				save_recording( false );
				// if not in record mode, stop recording now
				if( !record_mode )
					state.recording = false;
			}

			if( state.stagerecord ) {
				//
				// we were staged, now go to record mode if any channel is set to record
				//
				state.stagerecord = false;

				if (state.rec_left || state.rec_right || state.rec_midi ) {
					if( record_mode && config.longtracks && nextsection == 0 )
						state.longrecording = true;
					else
						state.recording = true;
					
				}
			}
			//
			// reset playback pointers and send note of messages in all active tracks 
			//
			Track *track = TrackHead;
			while( track ) {
				if( !track->longtrack || (nextsection == 0 && track->start) ) {
					track->playback = track->head;
					if( track->use_midi )
						track->send_notesoff_midi();
					if( track->longtrack ) track->start = false;
				}
				track = track->next;
			}
			midi_buffer_flush = true;

			// increment section counter now
			state.current_section++;
			if( state.current_section >= state.sections )
				state.current_section = 0;
				
		}
		
		// look for any long tracks to save off
		if( state.longrecording && !record_mode ) {
			// 
			// stop long recording. Save the collections to a track
			//
			save_recording( true );
			state.longrecording = false;			
		}
		
		// now see about recording anything from this call to process
		if( state.recording || state.longrecording ) {
			//
			// record this set of input frames into a collection
			//
			FrameCollection *fc;
			fc = new FrameCollection(nframes, state.rec_left, state.rec_right);
			
			if( state.rec_left ) 
				fc->copyin_left( in_left, nframes);
			if( state.rec_right ) 
				fc->copyin_right( in_right, nframes);

			if( state.rec_midi )
				state.midi_level_in = fc->copyin_midi( in_midi, nframes);
			else 
				state.midi_level_in = find_peak_midi( in_midi, nframes);
				
			fc->append(&RecHead, &RecTail);
			collcount++;
		} else
			state.midi_level_in = find_peak_midi( in_midi, nframes);

		//
		// playback all active tracks in this section by summing the frames
		// at the playback pointer
		//
		FrameCollection *sum = new FrameCollection(nframes, true, true ) ;
		sum->zero();
	
		Track *track, *next, *prev;
		int tracknum = 0;
		
		//
		// first pass: process any track remove request and look for track soloing
		//
		track = TrackHead;
		while( track ) {	// for all tracks

			if( track->remove ) {
				if( track == TrackHead ) {
					// we are the head
					if( track == TrackTail ) {
						// we are the only track
						delete track;
						track = NULL;
						TrackHead = TrackTail = NULL;
						state.trackcount--;
					} else { // are not the only track		
						next = track->next;
						if( next ) next->prev = NULL;
						delete track;
						TrackHead = next;
						state.trackcount--;
						track = next;
					}
				} else if( track == TrackTail ) {
					// we are the tail
					prev = track->prev;
					if( prev ) prev->next = NULL;
					delete track;
					TrackTail = prev;
					state.trackcount--;
					track = NULL;
				} else {
					next = track->next;
					prev = track->prev;
					delete track;
					if( next ) next->prev = prev;
					if( prev ) prev->next = next;
					state.trackcount--;
					track = next;
				}
				fprintf(stderr, "track %d removed\n", tracknum );

			} else {
				if( track->solo ) soloing = true;
				track = track->next;
				tracknum++;
			}
		}
				
		//
		// second pass: go thru the track list and get any samples to play back for this part
		//
		track = TrackHead;
		while( track ) {
		
			if( track->playback && 
				( track->longtrack || track->part == -1 || track->part == sections[state.current_section].part )) {
				//
				// there is something at the playback pointer and in our part as defined by section	
				//
				// sum the frame collection at the playback pointer, 
				// depending on whether we are soloing and the track is muted
				//
				if( soloing ) {
					if( track->solo ) 
						track->sum( sum, nframes, 1.0f, 1.0f, 1.0f );
				} else { 
					if( !track->mute ) 
						track->sum( sum, nframes, state.volume_left, state.volume_right, state.volume_midi ); 
				}
				//
				// advance the playback pointer for this track
				//	
				track->advance();
			}
			track = track->next;
		}

		//
		// Clear jack midi buffer before possibly outputting any midi events
		//
		jack_midi_clear_buffer(out_midi);
		
		//
		// check for beat activity and any metronome activity to output
		//
		
		// we are looking ahead to the next call to process, so we can begin the ticks
		// ahead of time to compensate for real events starting ahead of the beat
 
		int lookdiv = ( sections[state.current_section].divisions * (state.framecount + nframes) ) / sections[state.current_section].maxframes;
		int newdiv = ( sections[state.current_section].divisions * (state.framecount ) ) / sections[state.current_section].maxframes;

		if( lookdiv != current_div ) {
			//
			// process any click output 
			//
			if( state.use_click ) {
				if( config.cinternal ) {
					// use the internal click for metronome
					if( beat_count == 0 ) internal_click.begin_down();
					else if( beat_count == sections[state.current_section].beats - 1 )
						internal_click.begin_fill();
					else internal_click.begin_beat();
				} else {
					// use the midi output to create metronome click
					// this is presently done in one process() callback
					// and we do it here
					unsigned char buf[3];			
					if( beat_count == 0 ) {
						// output the downbeat
						buf[0] = 0x99;
						buf[1] = config.downbeat;
						buf[2] = config.velocity;
						jack_midi_event_write( out_midi, 0L, buf, 3);
						buf[0] = 0x89;
						jack_midi_event_write( out_midi, 0L, buf, 3);
					} else if( beat_count == sections[state.current_section].beats - 1 ) {
						// output the fill
						buf[0] = 0x99;
						buf[1] = config.fill;
						buf[2] = config.velocity;
						jack_midi_event_write( out_midi, 0L, buf, 3);
						buf[0] = 0x89;
						jack_midi_event_write( out_midi, 0L, buf, 3);				
					} else {
						// output a beat click
						buf[0] = 0x99;
						buf[1] = config.click;
						buf[2] = config.velocity;
						jack_midi_event_write( out_midi, 0L, buf, 3);
						buf[0] = 0x89;
						jack_midi_event_write( out_midi, 0L, buf, 3);
					}
				}
			}
		}
		
		if( newdiv != current_div ) {
			//
			// flag the beat and process any click output 
			//
			current_div = newdiv;

			// increment the beat counter and possible reset to zero
			if( ++beat_count >= sections[state.current_section].beats ) beat_count = 0;
		}
		
		//
		// look for any midi events buffered from control functions ready to be flushed
		//
		if( midi_buffer_flush ) {

			for( int i = 0; i < midi_buffer_len; i++ ) {

				jack_midi_event_write( out_midi, 0L, midi_event_buffer[i].buf,  midi_event_buffer[i].len );
				delete[] midi_event_buffer[i].buf;

			}
			midi_buffer_len = 0;
			midi_buffer_flush = false;
		}
		
		//
		// check for any samples to play from the internal click
		//
		if( config.cinternal && internal_click.playcount ) {
			internal_click.sum( sum, nframes );
			internal_click.advance();
		}
		
		//
		// now send out the summed samples to the output buffers
		//
		sum->copyout_left( out_left, nframes);
		sum->copyout_right( out_right, nframes);
		state.midi_level_out  = sum->copyout_midi( out_midi, nframes);
		
		// get the peak values for audio output
		state.audio_L_level_out = find_peak_audio( out_left, nframes);
		state.audio_R_level_out = find_peak_audio( out_right, nframes);

		// delete the sample summer 
		delete sum;
	}	
	else {
		//
		// not playing, but we may need to do something about a rewind request
		//
		//  clear output sample, flush midi output events, read midi input for peak
		//
		memset (out_left, 0, sizeof (jack_default_audio_sample_t) * nframes);
		memset (out_right, 0, sizeof (jack_default_audio_sample_t) * nframes);
		
		if( resettracks ) {
			// resetting everyback to zero, start normal tracks and
			// shut down long tracks
			Track *track = TrackHead;
			while( track ) {
				if( track->longtrack ) {
					track->playback = NULL;
					track->start = false;
				} else {
					track->playback = track->head;
					if( track->use_midi ) {
						track->send_notesoff_midi();
						midi_buffer_flush = true;
					}						
				}
				track = track->next;
			}		
		}
		//
		// look for any midi events buffered from control functions ready to be flushed
		//
		if( midi_buffer_flush ) {

			// clear jack midi buffer before outputting anything
			jack_midi_clear_buffer(out_midi);

			for( int i = 0; i < midi_buffer_len; i++ ) {

				jack_midi_event_write( out_midi, 0L, midi_event_buffer[i].buf,  midi_event_buffer[i].len );
				delete[] midi_event_buffer[i].buf;

			}
			midi_buffer_len = 0;
			midi_buffer_flush = false;
		}
		// read the midi input events for peak value
		state.midi_level_in = find_peak_midi( in_midi, nframes);		
	}

	// update some stuff in program state

	// read the audio input buffers for peak values
	state.audio_L_level_in = find_peak_audio( in_left, nframes);
	state.audio_R_level_in = find_peak_audio( in_right, nframes);

	state.bpm = (int)( (10.0f * 60.0f * sections[state.current_section].divisions)  / ( (float)sections[state.current_section].maxframes / (float)sample_rate) );
	//
	// Playing or not, see if it's time to send the udp packet containing
	//
	// 1. program state
	// 2. section list ( of part numbers )
	// 3. data for all tracks
	//
	// out the network port 
	//
	frame_count += nframes;
	if( frame_count >= MAX_FRAME_COUNT ) {
		frame_count = 0;
		
		unsigned char * bptr, *buffer;
		struct Statebuf statebuf;
		int sectionparts[MAX_SECTIONS];
		
		// create a buffer large enough to hold statebuf, sections and tracks 
		bptr = buffer = new unsigned char[ sizeof(Statebuf) + sizeof(int)*state.sections 
			+ sizeof(Trackbuf)*state.trackcount ];
			
		// copy state buffer any other data to buffer
		state.fill( &statebuf );
		statebuf.maxframes = sections[state.current_section].maxframes;
		statebuf.divisions = sections[state.current_section].divisions;
		statebuf.beats = sections[state.current_section].beats;
		statebuf.cinternal = config.cinternal;
		statebuf.autoupload = config.autoupload;
		statebuf.longtracks = config.longtracks;

		memcpy( bptr, &statebuf, sizeof(Statebuf) );
		bptr = bptr + sizeof(Statebuf);
		
		// copy the sections part numbers into an array
		// then copy this array to buffer
		for(int i = 0; i < state.sections; i++) 
			sectionparts[i] = sections[i].part;
		memcpy( bptr, sectionparts, sizeof(int) * state.sections );
		bptr = bptr + sizeof(int) * state.sections;
		
		//
		// copy all tracks  with simplified information 
		//
		Trackbuf trackbuf;
		int count = 0, tc = state.trackcount;
		Track *tptr = TrackHead;
		while(tptr && tc--) {
			
			//
			// fill out the Trackbuf
			//
			trackbuf.number = count++;
			strcpy(trackbuf.name, tptr->name );
			trackbuf.part = tptr->part;
			trackbuf.channel = tptr->channel;
			trackbuf.bank = tptr->bank;
			trackbuf.program = tptr->program;
			trackbuf.vol_left = (int) ( 100 * tptr->volume_left );
			trackbuf.vol_right = (int) ( 100 * tptr->volume_right );
			trackbuf.vol_midi = (int) ( 100 * tptr->volume_midi );
			trackbuf.peak_left = tptr->peak_left;
			trackbuf.peak_right = tptr->peak_right;
			trackbuf.peak_midi = tptr->peak_midi;
			trackbuf.mute = tptr->mute;
			trackbuf.solo = tptr->solo;
			trackbuf.longtrack = tptr->longtrack;
			if( tptr->longtrack )
				if( tptr->playback ) trackbuf.active = true;
				else 				 trackbuf.active = false;
			else 
				if( tptr->part == sections[state.current_section].part) trackbuf.active = true;
				else trackbuf.active = false;
			//
			// copy to buffer
			//
			memcpy( bptr, &trackbuf, sizeof(Trackbuf) );
			bptr = bptr + sizeof(Trackbuf);
			tptr = tptr->next; 
			
		}

		//
		// send out all data out network port
		//
		network.talk(buffer, sizeof(Statebuf) + sizeof(int)*state.sections + 
			sizeof(Trackbuf)*state.trackcount);
		
		delete[] buffer;
		
	}
		
	//
	// if we are playing then increase the frame counter
	//
	if( state.playing )
		state.framecount = state.framecount + nframes;

	// finally, check the transport for any changes 
	jack_transport_state_t transport_state = jack_transport_query( client, NULL );
	if( transport_state == JackTransportStopped && state.playing )
		state.playing = false;
	if( transport_state == JackTransportRolling && !state.playing )
		state.playing = true;

	return 0;
}

/*******************************************************************
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg)
{
	exit (1);
}

/********************************************************************
 * jack_init()
 * 
 * Initialize the jack interface. 
 * Return 1 on success
 */
int jack_init() {

	const char **ports;
	const char *client_name = "loopR";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	/* open a client connection to the JACK server */

	client = jack_client_open (client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, "
			 "status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		return(0);
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, 0);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

	jack_on_shutdown (client, jack_shutdown, 0);

	/* display and save the current sample rate. 
	 */

	fprintf (stderr, "engine sample rate: %" PRIu32 "\n",
		(sample_rate = jack_get_sample_rate (client)) );

	/* create four audio ports */

	input_port_left = jack_port_register (client, "input_left",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
	input_port_right = jack_port_register (client, "input_right",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput, 0);
	output_port_left = jack_port_register (client, "output_left",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);
	output_port_right = jack_port_register (client, "output_right",
					  JACK_DEFAULT_AUDIO_TYPE,
					  JackPortIsOutput, 0);

	if ((input_port_left == NULL) || (input_port_right == NULL) ||
		(output_port_left == NULL) || (output_port_right == NULL)) {
		fprintf(stderr, "no more JACK audio ports available\n");
		return(0);
	}

	/* create two midi ports */

	input_port_midi = jack_port_register (client, "in",
					 JACK_DEFAULT_MIDI_TYPE,
					 JackPortIsInput, 0);
	output_port_midi = jack_port_register (client, "out",
					  JACK_DEFAULT_MIDI_TYPE,
					  JackPortIsOutput, 0);

	if ((input_port_midi == NULL) || (output_port_midi == NULL)) {
		fprintf(stderr, "no more JACK midi ports available\n");
		return(0);
	}

	/* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return(0);
	}

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */

	/* need two capture ports for left and right */
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "no physical capture ports\n");
		return(0);
	}

	if (jack_connect (client, ports[0], jack_port_name (input_port_left))) {
		fprintf (stderr, "cannot connect input_left ports\n");
	}
	if (jack_connect (client, ports[1], jack_port_name (input_port_right))) {
		fprintf (stderr, "cannot connect input_right ports\n");
	}

	free (ports);
	
	/* need two playback ports for left and right */
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no physical playback ports\n");
		return(0);
	}

	if (jack_connect (client, jack_port_name (output_port_left), ports[0])) {
		fprintf (stderr, "cannot connect output_left ports\n");
	}
	if (jack_connect (client, jack_port_name (output_port_right), ports[1])) {
		fprintf (stderr, "cannot connect output_right ports\n");
	}

	free (ports);

#if 0
	/* need one capture ports for midi */
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "no midi capture port\n");
		return(0);
	}

	if (jack_connect (client, jack_port_name (input_port_midi), ports[0])) {
		fprintf (stderr, "cannot connect midi capture port\n");
	}

	free (ports);

	/* need one playback ports for midi */
	ports = jack_get_ports (client, NULL, NULL,
				JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no midi playback port\n");
		return(0);
	}

	if (jack_connect (client, jack_port_name (output_port_midi), ports[0])) {
		fprintf (stderr, "cannot connect midi playback port\n");
	}

	free (ports);
#endif

	return 1;
}

/***********************************************************************
 * jack_close()
 * 
 */
int jack_close() {

	jack_client_close (client);

	return 0;
}

/***********************************************************************
 * jack_send_midi()
 * 
 * Append some midi event to the buffer maintained for jack.
 * These events will be outputted later when the flush flag is set
 */
int jack_send_midi(int len, unsigned char *buf) {
	
	if( midi_buffer_len  >= MIDI_BUFFER_LEN ) return 0;

	unsigned char *buffer = new unsigned char[len];
	midi_event_buffer[midi_buffer_len].buf = buffer;
	midi_event_buffer[midi_buffer_len++].len = len;

	for(int i=0;i<len;i++) buffer[i] = buf[i];
	
	return 0;
}
		
/***********************************************************************
 * jack_write_midi()
 * 
 * Call for a flush of midi events in the output buffer.
 * Set a flag, so that the process() procedure can do the actual output
 */
int jack_write_midi() {
	
	midi_buffer_flush = true;
	return 0;
}

//
// helper rountines called from control
//
void jack_rewind() {
	// flag these to occur in process()
	rewind_request = true; 
	
	//current_div = -1;
	//beat_count = 0;	
}

void jack_clear_tracks() {
	
	clear_tracks = true;
}

void jack_startstop(int n) {
	if(n) jack_transport_start( client );
	else jack_transport_stop( client );
}

