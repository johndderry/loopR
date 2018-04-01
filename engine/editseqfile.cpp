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
//
//	editseqfile
//
//	utility program for editing a seq-state file 
//
#include <stdio.h>
#include <unistd.h> 
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <zlib.h>

#include <signal.h>
#include <sys/stat.h> 
#include <fcntl.h> 

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
#include "service.h"
#include "core.h"

void display_state() {
	
	printf("framecount=%d, trackcount=%d\n", state.framecount, state.trackcount);
	printf("playing=%d, recording=%d, longrecording=%d, stagerecord=%d\n", 
		state.playing, state.recording, state.longrecording, state.stagerecord );
	printf("rec_left=%d, rec_right=%d, rec_midi=%d, use_click=%d, bpm_mode=%d\n", 
		state.rec_left, state.rec_right, state.rec_midi, state.use_click, state.bpm_mode );
	printf("sections=%d, current_section=%d, bpm=%d, bank=%d, program=%d\n", 
		state.sections, state.current_section, state.bpm, state.bank, state.program );
 
}

void display_track(int n, Track *t) {
	
	printf("%d:	name=%s, unique_ident=%d, left_complen=%d, right_complen=%d\n",
		n, t->name, t->unique_ident, t->left_complen, t->right_complen );
	printf("	collcount=%d, loadcount=%d, part=%d, bank=%d, program=%d, channel=%d\n", 
		t->collcount, t->loadcount, t->part, t->bank, t->program, t->channel );
	printf("	mute=%d, solo=%d, use_left=%d, use_right=%d, use_midi=%d, remove=%d, longtrack=%d, start=%d\n\n",
		t->mute, t->solo, t->use_left, t->use_right, t->use_midi, t->remove, t->longtrack, t->start );

}

void display_tracks() {
	int n = 0;
	Track *track = TrackHead;

	while( track ) {
		display_track( n++, track );
		track = track->next;
	}

}		

int main (int argc, char *argv[]) {
	
	int tracknum = -1;
	Track *track;
	
	load_sequencer(false);
	display_state();
	display_tracks();
	
	char input[256];
	input[0] = ' ';
	
	while( input[0] != 'q' ) {
		fputs("q(uit) v(iew) c(lean) C(lear) f(lush) T(racks)n t(rack)n Ln Rn Mn>", stderr);
		fgets(input, 256, stdin);
		input[255] = 0;		
		//fprintf(stderr, "input=%s\n", input );
		switch( input[0] ) {
			
			case 'v': display_state(); display_tracks(); 
				printf("Track = %d\n", tracknum); break;
			case 'C':
				memset( &state, 0, sizeof(state) );
				state.volume_left = state.volume_right = state.volume_midi = 1.0f;
				state.sections = 4;
				state.coupled = true;
			case 'c':
				state.playing = state.recording = state.longrecording = state.stagerecord = false;
				state.rec_left = state.rec_right = state.rec_midi = state.use_click = false;
				state.bpm_mode = true; break;			
			case 'q': break;
			case 'f': flush_sequencer(false); break;
			case 't': int i;
				sscanf(&input[1], "%d", &tracknum);
				fprintf(stderr, "track=%d\n", tracknum );
				for( i = 0, track = TrackHead; track && i < tracknum; i++, track = track->next);
				if( !track ) tracknum = -1;
				break;
			case 'T': 
				sscanf(&input[1], "%d", &state.trackcount);
				break;
			case 'L': if( tracknum < 0 ) break;
				if( input[1] == '0' ) track->use_left = 0; 
				if( input[1] == '1' ) track->use_left = 1; 
				break;
			case 'R': if( tracknum < 0 ) break;
				if( input[1] == '0' ) track->use_right = 0; 
				if( input[1] == '1' ) track->use_right = 1; 
				break;
			case 'M': if( tracknum < 0 ) break;
				if( input[1] == '0' ) track->use_midi = 0; 
				if( input[1] == '1' ) track->use_midi = 1; 
				break;
			
			default: printf("unknown command sequence: '%s'\n", input); 
		}
	}
	
	return 0;
}
