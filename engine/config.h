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
// CLASS CONFIG
// this class for handling configuration tasks
//
#define TRACKPUT	"4950"
#define TRACKGET	"4951"
#define OUTPORT 	"4952"
#define OUTHOST		"127.0.0.1"
#define INPORT  	"4953"
#define TRACKHOST	"127.0.0.1"
#define CLIENT		"~/loopR/frontend/imgui/client"
#define CINTERNAL 	true
#define AUTOUPLOAD	true
#define LONGTRACKS	true
#define DOWNBEAT	36
#define FILL		46
#define CLICK		42
#define VELOCITY	111

class Config {
public:
	char *inport, *outport, *outhost, *trackput, *trackget, *trackhost;
	char *client;
	bool cinternal, autoupload, longtracks;
	unsigned char downbeat, fill, click, velocity;

	Config();
	~Config();
};
