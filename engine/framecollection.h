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
//	FRAMECOLLECTION
//
//	A collection of frames as received from a JACK port during one callback.
//  Created and filled as an element of a linked list during the recording process,
//  this linked list is maintained in class Track.
//
struct disk_midi_event {
	unsigned int collection, time;
	unsigned char size;
	unsigned char buffer[7];
};

class FrameCollection {

	FrameCollection	*next, *prev; 
	jack_nframes_t nframes, nevents;
	jack_default_audio_sample_t *frames_left, *frames_right;
	jack_midi_event_t *events;

public:

	FrameCollection(jack_nframes_t, bool, bool);
	~FrameCollection();
	FrameCollection *get_next(), *get_prev();
	jack_default_audio_sample_t  *get_frames_left(), *get_frames_right();
	jack_midi_event_t *get_events();
	jack_nframes_t get_nevents();
	int copyin_left(jack_default_audio_sample_t*, jack_nframes_t),
		copyin_right(jack_default_audio_sample_t*, jack_nframes_t);
	int copyout_left(jack_default_audio_sample_t* , jack_nframes_t),
		copyout_right(jack_default_audio_sample_t* , jack_nframes_t);
	int copyin_midi(void*, jack_nframes_t),
		copyout_midi(void*, jack_nframes_t);
	void append(FrameCollection **, FrameCollection **);
	void insert_midi_event(jack_midi_event_t *, int, float);
	void zero();
	int write_left(int), write_right(int), 
		write_left(z_stream*, bool), write_right(z_stream*, bool), write_midi(int,int), 
		read_left(int,unsigned*), read_right(int,unsigned*), 
		read_left(z_stream*,unsigned*), read_right(z_stream*,unsigned*), read_midi( struct disk_midi_event *);
	void adjustframes( jack_nframes_t );
};
