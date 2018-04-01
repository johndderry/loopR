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

State::State() {

	volume_left = volume_right = volume_midi = 1.0f;
	sections = 4;
	rec_midi = rec_left = rec_right = false;
	use_click = false;
	bpm_mode = true;
	coupled = true;
	trackcount = 0;
	
}
	
State::~State() {
	
}

void State::fill(struct Statebuf *p) {
	
	p->volume_left = volume_left;
	p->volume_right = volume_right;
	p->volume_midi = volume_midi;
	p->framecount = framecount;
	p->trackcount = trackcount;
	p->playing = playing;
	p->recording = recording;
	p->longrecording = longrecording;
	p->stagerecord = stagerecord;
	p->rec_midi = rec_midi;
	p->rec_left = rec_left;
	p->rec_right = rec_right;
	p->use_click = use_click;
	p->bpm_mode = bpm_mode;
	p->sections = sections;
	p->current_section = current_section;
	p->bpm = bpm;
	p->bank = bank;
	p->program = program;
	p->audio_L_level_in = audio_L_level_in;
	p->audio_R_level_in = audio_R_level_in;
	p->audio_L_level_out = audio_L_level_out;
	p->audio_R_level_out = audio_R_level_out;
	p->midi_level_in = midi_level_in;
	p->midi_level_out = midi_level_out;
	
}

void State::forward() {
	if( ++current_section >= sections ) 
		current_section = 0;
}

void State::back() {
	if( --current_section < 0 ) 
		current_section = sections -1 ;
}


void State::adj_sections(int n) {
	if( n ) {
		if( sections < MAX_SECTIONS ) sections++;
	}
	else if( sections > 1 ) sections--;
}


void State::record() {
	if( stagerecord ) stagerecord = false;
	else
		if( playing || framecount > 0L) // stage the recording
			stagerecord = true;
		else { 
			// not playing yet and at zero: did we just hit record?
			if( recording ) recording = false;
			else
				// don't stage, go right to recording if any channel is selected
				if( rec_left || rec_right || rec_midi )	recording = true;
		}

}

void State::long_record(bool longtracks) {
	
	if( playing || framecount > 0L ) // stage the recording
		stagerecord = true;
	else  { // not playing yet: don't stage, go right to recording 
		if( current_section == 0 && longtracks )
			longrecording = true;
		else
			recording = true;
	}

}

