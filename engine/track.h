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
 
#define  NFRAMES 1024

//
//  Defines a track in the global track list
//
class Track {
public:
	FrameCollection *head, *tail, *playback;
	int		collcount, loadcount, part, bank, program, channel;
	bool	mute, solo, use_left, use_right, use_midi, remove, longtrack, start;
	Track	*next, *prev;
	float	volume_left, volume_right, volume_midi;
	int		peak_left, peak_right, peak_midi;
	char	name[TRACK_NAME_MAX+1];
	unsigned int unique_ident, left_complen, right_complen;

	Track();
	Track(int, int);
	~Track();
	bool advance();
	void sum( FrameCollection *s, unsigned );
	void sum( FrameCollection *s, unsigned, float, float, float );
	void send_notesoff_midi(), send_channel_midi();
	int write_left(int), write_right(int), write_midi(int), 
		read_left(int, bool), read_right(int), read_midi(int);
	int nevents();
	void resample();
	void reverse();
};
