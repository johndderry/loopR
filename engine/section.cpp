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

Section::Section() {

	part = 0;
	maxframes = (MAX_FRAMES - MIN_FRAMES)/2 + MIN_FRAMES;
	divisions = 16;
	beats = 4;
}

void Section::tempo(int n) {
	//
	// increase/decrease maxframes by 400 to adjust tempo - increasing tempo reduces maxframes!
	//
	if( n ) maxframes -= 400;
	else maxframes += 400;

	if( maxframes > MAX_FRAMES ) 
		maxframes = MAX_FRAMES;
	else if( maxframes < MIN_FRAMES ) 
		maxframes = MIN_FRAMES;
}

void Section::settempo(char *p) {
	//
	// set the maxframes according to the percentage passed - max tempo is minimum maxframes!
	//
	char digits[4];
	digits[0] = *p++;
	digits[1] = *p++;
	digits[2] = *p++;
	digits[3] = '\0';

	int t = atoi(digits);
	
	maxframes = ( ( (MAX_FRAMES-MIN_FRAMES) * (100-t) ) / 100 ) + MIN_FRAMES;
}

void Section::adj_divisions(int n, bool bpm_mode ) {
	if( n ) {
		if( divisions < MAX_DIVISIONS ) {
			if( bpm_mode ) 
				// adjust maxframes so that bpm remains the same
				maxframes += maxframes/divisions;
			divisions++;
		}
	}
	else if( divisions > 1 ) {
		if( bpm_mode ) 
			maxframes -= maxframes/divisions;
		divisions--;
	}
}

void Section::adj_beats(int n) {
	if( n ) {
		if( beats < divisions ) beats++;
	}
	else if( beats > 1 ) beats--;
}
