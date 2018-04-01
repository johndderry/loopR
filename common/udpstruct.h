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
*///
// C++ Interface: statetrack
//
// Description: 
//
//
// Author: john,,, <john@q4os-desktop>, (C) 2016
//
// Copyright: See COPYING file that comes with this distribution
//
//

// here are a couple of project-wide constants first

#define TRACK_NAME_MAX	20
#define FILE_PATH_MAX	40

#define MAX_SECTIONS	64		// server expects fixed sections array
#define MAX_DIVISIONS	64

#define MAX_FRAMES  (400 * 2500)
#define MIN_FRAMES  (400 * 100)


//
// STATEBUF
//

struct Statebuf {

	//short int volume_left, volume_right, volume_midi;
	float volume_left, volume_right, volume_midi;
	unsigned int framecount, maxframes, trackcount;
	bool playing, recording, longrecording, stagerecord, 
		rec_left, rec_right, rec_midi, use_click, bpm_mode;
	short int divisions, beats, sections, current_section, bpm; 
	short int bank, program, 
		audio_L_level_in, audio_R_level_in, audio_L_level_out, audio_R_level_out, 
		midi_level_in, midi_level_out;
	bool cinternal, autoupload, longtracks;

};


//
// TRACKBUF
//
struct Trackbuf {
	char name[TRACK_NAME_MAX+1];
	short int number, part, channel, bank, program;
	short int vol_left, vol_right, vol_midi, peak_left, peak_right, peak_midi;
	bool mute, solo, longtrack, active;
};
