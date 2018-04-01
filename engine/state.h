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

class State {
public:
	float volume_left, volume_right, volume_midi;
	unsigned int framecount, trackcount;
	bool playing, recording, longrecording, stagerecord, 
		rec_left, rec_right, rec_midi, use_click, bpm_mode, coupled;
	short int sections, current_section, bpm; 
	short int bank, program, 
		audio_L_level_in, audio_R_level_in, audio_L_level_out, audio_R_level_out, 
		midi_level_in, midi_level_out;

	State();
	~State();
	void fill(struct Statebuf *);
	void forward(), back(), record(), long_record(bool);
	void tempo(int n);
	void settempo(char *p);
	void adj_divisions(int n);
	void adj_sections(int n);
	void adj_beats(int n);
	
};
