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
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <stdio.h>
#include <GLFW/glfw3.h>

#define INPORT   "4952"		// the default port 
#define OUTPORT  "4953"		// the default port 
#define PORTBUFSIZE  2048	// buffer size for port read and writes
#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../../common/network.h"
#include "../../common/udpstruct.h"

Network network;

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error %d: %s\n", error, description);
}

bool long_recording = false;
bool click_track = false;
bool couple_sections = true;
bool show_test_window = false;
char trackname[TRACK_NAME_MAX+1];
bool shHints = true;

void show_transport(bool *p_open) {

	char buf[TRACK_NAME_MAX+8];
	
	ImGui::Begin("Transport", p_open);

	if( ImGui::Button("<<") ) network.talk("w"); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Rewind");

	if( ImGui::Button("<") ) network.talk("<"); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Move Back one Section");

	if( ImGui::Button(" . ") ) network.talk("."); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Stop Sequencer");

	if( ImGui::Button(">") ) network.talk(">"); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Move Forward one Section");

	if( ImGui::Button(">>") ) network.talk(","); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Begin Playing");

	ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.6f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.0f, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.0f, 0.8f, 0.8f));

	if( ImGui::Button("Rec") ) {
		if( trackname[0] ) {
			sprintf(buf, "n%s", trackname);
			network.talk(buf);
		}
		network.talk("r");
	} ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Record");

	bool recording_long = long_recording;
	ImGui::Checkbox("REC", &long_recording); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Long Recording");
	if( long_recording != recording_long ) {
		if( long_recording ) {
			if( trackname[0] ) {
				sprintf(buf, "n%s", trackname);
				network.talk(buf);
			}
			network.talk("R1");
		}
		else network.talk("R0");
	}

	ImGui::PopStyleColor(3);

	bool track_click = click_track;
	ImGui::Checkbox("Click", &click_track);	ImGui::SameLine();
	if( click_track != track_click ) {
		if( click_track ) network.talk("c1");
		else			  network.talk("c0");
	}
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Click Track");

	bool sessions_couple = couple_sections;
	ImGui::Checkbox("Couple", &couple_sections);	ImGui::SameLine();
	if( couple_sections != sessions_couple ) {
		if( couple_sections ) network.talk("C1");
		else				  network.talk("C0");
	}
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Section Coupling");

	ImGui::Checkbox("Demo", &show_test_window);	ImGui::SameLine();

	ImGui::End();
}

void show_program(bool *p_open) {

	const char *program_list[] = {
		"Acoustic Grand", "Bright Acoustic", "Electric Grand", "Honky-Tonk", 
		"Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clav", 
		"Celesta", "Glockenspiel", "Music Box", "Vibraphone", 
		"Marimba", "Xylophone", "Tubular Bells", "Dulcimer", 
		"Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", 
		"Reed Organ", "Accoridan", "Harmonica", "Tango Accordian", 
		"Acoustic Guitar(nylon)", "Acoustic Guitar(steel)", "Electric Guitar(jazz)", "Electric Guitar(clean)", 
		"Electric Guitar(muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics", 
		"Acoustic Bass", "Electric Bass(finger)", "Electric Bass(pick)", "Fretless Bass", 
		"Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2", 
		"Violin", "Viola", "Cello", "Contrabass", 
		"Tremolo Strings", "Pizzicato Strings", "Orchestral Strings", "Timpani", 
		"String Ensemble 1", "String Ensemble 2", "SynthStrings 1", "SynthStrings 2", 
		"Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit", 
		"Trumpet", "Trombone", "Tuba", "Muted Trumpet", 
		"French Horn", "Brass Section", "SynthBrass 1", "SynthBrass 2", 
		"Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax", 
		"Oboe", "English Horn", "Bassoon", "Clarinet", 
		"Piccolo", "Flute", "Recorder", "Pan Flute", 
		"Blown Bottle", "Skakuhachi", "Whistle", "Ocarina", 
		"Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)", 
		"Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass+lead)", 
		"Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)", 
		"Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)", 
		"FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)", 
		"FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)", 
		"Sitar", "Banjo", "Shamisen", "Koto", 
		"Kalimba", "Bagpipe", "Fiddle", "Shanai", 
		"Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", 
		"Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal", 
		"Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet", 
		"Telephone Ring", "Helicopter", "Applause", "Gunshot" };

	static int program_item  = 0;
	char buf[64];

	ImGui::Begin("Program", p_open);

	ImGui::Text("Choose Midi Program");
	ImGui::PushItemWidth(200);
	ImGui::ListBox("", &program_item, program_list, IM_ARRAYSIZE(program_list), 20);
	ImGui::PopItemWidth();
	if( ImGui::Button("Accept") ) {
		sprintf(buf, "g%d,%d", 0, program_item);
		network.talk(buf);
		*p_open = false;
	}
	ImGui::SameLine();
	if( ImGui::Button("Abort") )
		*p_open = false; 
	
	ImGui::End();

}

Statebuf statebuffer;

bool running = true,
	bpm_mode = true,
	shProgram = false;

bool shTransport = true,
	shControls = true, 
	shMains = true,
	shPanel = true,
	shPlaying = true,
	shMixer = true,
	shChat = true;

bool cinternal, autoupload, longtracks;

int tempo;
char tempo_str[128];

void show_controls(bool *p_open) {

	char buf[64];
	
	ImGui::Begin("Controls", p_open, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("View")) {
			ImGui::MenuItem("Controls", NULL, &shControls);
			ImGui::MenuItem("Transport", NULL, &shTransport);
			ImGui::MenuItem("Panel", NULL, &shPanel);
			ImGui::MenuItem("Mains", NULL, &shMains);
			ImGui::MenuItem("Playing", NULL, &shPlaying);
			ImGui::MenuItem("Mixer", NULL, &shMixer);
			ImGui::MenuItem("Chat", NULL, &shChat);
			ImGui::MenuItem("Hints", NULL, &shHints);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	bool mode_bpm = bpm_mode;
	bool cinternal_old = cinternal, autoupload_old = autoupload, longtracks_old = longtracks;
	
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Config")) {
			ImGui::MenuItem("BPM Mode", NULL, &bpm_mode);
			ImGui::MenuItem("Internal Click", NULL, &cinternal );
			ImGui::MenuItem("Auto Upload", NULL, &autoupload);
			ImGui::MenuItem("Long Tracks", NULL, &longtracks);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	
	if( bpm_mode != mode_bpm ) {
		if( bpm_mode ) network.talk("M1");
		else		   network.talk("M0");
	}

	if( cinternal != cinternal_old || autoupload != autoupload_old || longtracks != longtracks_old ) {
		char com[8];
		com[0] = '=';
		if( cinternal ) com[1] = '1';
		else com[1] = '0';
		if( autoupload ) com[2] = '1';
		else com[2] = '0';
		if( longtracks ) com[3] = '1';
		else com[3] = '0';
		com[4] = '\0';
		network.talk(com);
	}
	
	ImGui::BeginGroup();
	if( ImGui::Button("-##e") ) network.talk("e-"); ImGui::SameLine();
	sprintf(buf, "%d", statebuffer.sections );
	ImGui::Text(buf); ImGui::SameLine();
	if( ImGui::Button("+##e") ) network.talk("e+"); ImGui::SameLine();
	ImGui::Text("Sections"); //ImGui::SameLine();
	ImGui::EndGroup(); ImGui::SameLine(0,20);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Change Section Count");
	

	ImGui::BeginGroup();
	if( ImGui::Button("-##d") ) network.talk("d-"); ImGui::SameLine();
	sprintf(buf, "%d", statebuffer.divisions );
	ImGui::Text(buf); ImGui::SameLine();
	if( ImGui::Button("+##d") ) network.talk("d+"); ImGui::SameLine();
	ImGui::Text("Divisions"); //ImGui::SameLine();
	ImGui::EndGroup(); ImGui::SameLine(0,20);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Change Division Count");

	ImGui::BeginGroup();
	if( ImGui::Button("-##b") ) network.talk("b-"); ImGui::SameLine();
	sprintf(buf, "%d", statebuffer.beats );
	ImGui::Text(buf); ImGui::SameLine();
	if( ImGui::Button("+##b") ) network.talk("b+"); ImGui::SameLine();
	ImGui::Text("Beats"); //ImGui::SameLine();
	ImGui::EndGroup(); ImGui::SameLine(0,20);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Change Beat Count");

	ImGui::BeginGroup();
	int prev_tempo = tempo;
	ImGui::PushItemWidth(100);
	ImGui::SliderInt("Tempo", &tempo, 0, 100, ""); ImGui::SameLine();
	ImGui::PopItemWidth();
	if( tempo != prev_tempo ) {
		sprintf(buf, "t%3d", tempo);
		network.talk(buf);
	}
	if( ImGui::Button("-##t") ) network.talk("t-"); ImGui::SameLine();
	ImGui::PushItemWidth(60);
	ImGui::InputText("##t", tempo_str, IM_ARRAYSIZE(tempo_str) ); ImGui::SameLine();
	ImGui::PopItemWidth();
	if( ImGui::Button("+##t") ) network.talk("t+"); //ImGui::SameLine();
	ImGui::EndGroup();  ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Adjust the Tempo");
	
#if 0
	bool mode_bpm = bpm_mode ;
	ImGui::Checkbox("BPM", &bpm_mode ); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle between BPM and Framecount Modes");
	if( bpm_mode != mode_bpm ) {
		if( bpm_mode ) network.talk("M1");
		else		   network.talk("M0");
	}
#endif
	
	if( ImGui::Button("Program") ) { 
		shProgram = !shProgram; 
	} ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Popup a window to choose General Midi Program");

 	if( ImGui::Button("Flush") ) network.talk("f"); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Flush out sequencer files to disk and continue");

 	if( ImGui::Button("Clear") ) network.talk("x"); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Clear all sequencer data: WARNING! can not be undone");

 	if( ImGui::Button("Reload") ) network.talk("L"); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Reload sequencer data from disk");

 	if( ImGui::Button("Exit") ) {
		network.talk("q");
		running = false;
	}  ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Exit the program and save sequencer files");
	
	ImGui::End();
}

int left_in_meter, right_in_meter, midi_in_meter;
int left_out_meter, right_out_meter, midi_out_meter;
int left_vol, right_vol, midi_vol;
bool left_enable, right_enable, midi_enable;

void show_mains(bool *p_open) {

	char buf[64];

	ImGui::Begin("Mains", p_open);

	ImGui::PushItemWidth(105);
	ImGui::InputText("", trackname, IM_ARRAYSIZE(trackname));
	ImGui::PopItemWidth();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("The Trackname can be set here");
	
	ImGui::Text("Trackname");

	ImGui::VSliderInt("##leftoutmeter", ImVec2(29,100), &left_out_meter, 0, 120); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Left Channel Output Meter");
	ImGui::VSliderInt("##rightoutmeter", ImVec2(29,100), &right_out_meter, 0, 120); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Right Channel Output Meter");
	ImGui::VSliderInt("##midioutmeter", ImVec2(29,100), &midi_out_meter, 0, 120);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Midi Channel Output Meter");

	ImGui::Text("Output");

	int vol_left = left_vol;
	int vol_right = right_vol;
	int vol_midi = midi_vol;
	
	ImGui::VSliderInt("##leftvol", ImVec2(29,100), &left_vol, 0, 120); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Left Channel Volume Control");
	ImGui::VSliderInt("##rightvol", ImVec2(29,100), &right_vol, 0, 120); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Right Channel Volume Control");
	ImGui::VSliderInt("##midivol", ImVec2(29,100), &midi_vol, 0, 120);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Midi Channel Volume Control");

	if( left_vol != vol_left ) {
		sprintf(buf,"vl%3d", left_vol);
		network.talk(buf);
	}

	if( right_vol != vol_right ) {
		sprintf(buf,"vr%3d", right_vol);
		network.talk(buf);
	}

	if( midi_vol != vol_midi ) {
		sprintf(buf,"vm%3d", midi_vol);
		network.talk(buf);
	}

	bool enable_left = left_enable;
	bool enable_right = right_enable;
	bool enable_midi = midi_enable;
	
	ImGui::Checkbox("L", &left_enable);	ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Left Channel Enable");
	if( left_enable != enable_left ) {
		if( left_enable ) network.talk("l1");
		else			  network.talk("l0");
	}

	ImGui::Checkbox("R", &right_enable); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Right Channel Enable");
	if( right_enable != enable_right ) {
		if( right_enable ) network.talk("a1");
		else			  network.talk("a0");
	}

	ImGui::Checkbox("M", &midi_enable);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Toggle Midi Channel Enable");
	if( midi_enable != enable_midi ) {
		if( midi_enable ) network.talk("m1");
		else			  network.talk("m0");
	}

	ImGui::Text("Control");

	ImGui::VSliderInt("##leftinmeter", ImVec2(29,100), &left_in_meter, 0, 120); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Left Channel Input Meter");
	ImGui::VSliderInt("##rightinmeter", ImVec2(29,100), &right_in_meter, 0, 120); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Left Channel Input Meter");
	ImGui::VSliderInt("##midiinmeter", ImVec2(29,100), &midi_in_meter, 0, 120);
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Left Channel Intput Meter");

	ImGui::Text("Input");

	ImGui::End();
}

int sections[256];
int current_div;

const char* partnames[] = {
	"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
	"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z" };

const char* partnames2[] = { "_", 
	"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
	"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z" };

void show_panel(bool *p_open) {
	
	ImGui::Begin("Panel", p_open);

	char buf[64];
	bool checked = true, not_checked = false;
	const char* selectnames[] = { 
"select0","select1","select2","select3","select4","select5","select6","select7","select8","select9",
"select10","select11","select12","select13","select14","select15","select16","select17","select18","select19",
"select20","select21","select22","select23","select24","select25","select26","select27","select28","select29",
"select30","select31","select32","select33","select34","select35","select36","select37","select38","select39", 
"select40","select41","select42","select43","select44","select45","select46","select47","select48","select49",
"select50","select51" };

	ImGui::BeginGroup();
	for( int i = 0; i < statebuffer.sections; i++ ) {

		// for each section place a checkbox and pop-down menu
				
		if( i == statebuffer.current_section )
			ImGui::Checkbox("", &checked);
		else
			ImGui::Checkbox("", &not_checked);
			
		/*if( i < statebuffer.sections -1 )*/ ImGui::SameLine();

		sprintf(buf, "%s##%d", partnames[sections[i]], i );
		if (ImGui::Button(buf))
			ImGui::OpenPopup(selectnames[i]);
			
		if (ImGui::BeginPopup(selectnames[i]))
		{
			ImGui::Text("Part");
			ImGui::Separator();
			for (int j = 0; j < statebuffer.sections && j < IM_ARRAYSIZE(partnames); j++)
				if (ImGui::Selectable(partnames[j])) {
					//sections[i] = j;
					sprintf(buf, "%dA%d", i, j );
					network.talk( buf );
				}
			ImGui::EndPopup();
		} ImGui::SameLine(); ImGui::Text(" "); 
		if (i < statebuffer.sections -1 ) ImGui::SameLine();
		
	}
	ImGui::EndGroup();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("These are the sections which retain division, beat, tempo and part info. Click on letter to change part.");
	
	//ImGui::Text("Division");
	ImGui::BeginGroup();
	for( int i = 0; i < statebuffer.divisions; i++) {
		
		if( i == current_div )
			ImGui::Checkbox("", &checked);
		else
			ImGui::Checkbox("", &not_checked);
		if( i < statebuffer.divisions -1 )
			ImGui::SameLine();
	} ImGui::EndGroup();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("These are the divisions which divide up the sample period. An indicator moves through these while playing.");
	
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f,0.0f,0.0f,1.0f));
	if( statebuffer.stagerecord ) ImGui::BulletText("Staging");
	if( statebuffer.recording ) ImGui::BulletText("Recording");
	if( statebuffer.longrecording ) ImGui::BulletText("Long Recording");
	ImGui::PopStyleColor();
	
	ImGui::End();
}

Trackbuf tracks[256];

void show_playing(bool *p_open) {
	
	unsigned indx = 0;
	int peak_left, peak_right, peak_midi;
	bool mute, solo;
	char buf[64];
	
	ImGui::Begin("Playing", p_open);
	
	while( indx < statebuffer.trackcount ) {
		if( tracks[indx].longtrack || tracks[indx].part == -1 || tracks[indx].part == sections[statebuffer.current_section] ) {
			// track is to be displayed
			sprintf(buf, "%d", indx);
			ImGui::Text(buf); ImGui::SameLine(); 			
			ImGui::Text(tracks[indx].name); ImGui::SameLine(); 
			
			mute = tracks[indx].mute;
			sprintf(buf, "Mute##%d", indx );
			ImGui::Checkbox(buf, &mute ); ImGui::SameLine();
			if (shHints && ImGui::IsItemHovered())
				ImGui::SetTooltip("Mute this track");
			if( mute != tracks[indx].mute ) {
				sprintf(buf, "%dm%d", indx, mute );
				network.talk(buf);
			}
			
			solo = tracks[indx].solo;
			sprintf(buf, "Solo##%d", indx );
			ImGui::Checkbox(buf, &solo ); ImGui::SameLine();
			if (shHints && ImGui::IsItemHovered())
				ImGui::SetTooltip("Solo this track");
			if( solo != tracks[indx].solo ) {
				sprintf(buf, "%ds%d", indx, solo );
				network.talk(buf);
			}
			
			peak_left = tracks[indx].peak_left;
			peak_right = tracks[indx].peak_right;
			peak_midi = tracks[indx].peak_midi;
			
			ImGui::PushItemWidth(100);
			
			sprintf(buf, "##VolLeft%d", indx );
			ImGui::SliderInt(buf, &peak_left, 0, 100 ); ImGui::SameLine();
			if (shHints && ImGui::IsItemHovered())
				ImGui::SetTooltip("Peak volume level for left channel");

			sprintf(buf, "##VolRight%d", indx );
			ImGui::SliderInt(buf, &peak_right, 0, 100 ); ImGui::SameLine();
			if (shHints && ImGui::IsItemHovered())
				ImGui::SetTooltip("Peak volume level for right channel");

			sprintf(buf, "##VolMidi%d", indx );
			ImGui::SliderInt(buf, &peak_midi, 0, 100 );
			if (shHints && ImGui::IsItemHovered())
				ImGui::SetTooltip("Peak volume level for midi channel");
						
			ImGui::PopItemWidth();
		}
		indx++;
	}
	
	ImGui::End();
}

struct DetailStruct {

	struct DetailStruct *next;
	Trackbuf track;
	int indx;

} *detail_head = NULL;

void show_mixer(bool *p_open) {
	
	char buf[64];
	int pan, lastpan, vol, lastvol;
	unsigned indx = 0;
	
	ImGui::Begin("Mixer", p_open);
	
    if( statebuffer.trackcount == 0 ) {
		ImGui::End();	
		return;
	}
	
	ImGui::Columns(statebuffer.trackcount, "mixed");
	ImGui::Separator();

	while( indx < statebuffer.trackcount ) {
		sprintf(buf, "%d##mixer", indx);
		if( ImGui::Button(buf) ) {
			if( detail_head ) {
				DetailStruct *p = detail_head;
				while( p->next ) p = p->next;
				p->next = new DetailStruct;
				p->next->next = NULL;
				p->next->track = tracks[indx];
				p->next->indx = indx;
			} else {
				detail_head = new DetailStruct;
				detail_head->next = NULL;
				detail_head->track = tracks[indx];
				detail_head->indx = indx;
			}
		}
		if (shHints && ImGui::IsItemHovered()) {
			sprintf(buf, "Click to edit the details for track %d", indx);
			ImGui::SetTooltip(buf);			
		}

		pan = lastpan = tracks[indx].vol_right - tracks[indx].vol_left;
		sprintf(buf, "##pan%d", indx);
		ImGui::PushItemWidth(30);
		ImGui::SliderInt(buf, &pan, -100, 100);
		ImGui::PopItemWidth();
		if (shHints && ImGui::IsItemHovered()) {
			sprintf(buf, "Channel Panning for track %d", indx);
			ImGui::SetTooltip(buf);			
		}

		vol = lastvol = (tracks[indx].vol_left + tracks[indx].vol_right)/2;
		sprintf(buf, "##vol%d", indx);		
		ImGui::VSliderInt(buf, ImVec2(30,100), &vol, 0, 100);
		if (shHints && ImGui::IsItemHovered()) {
			sprintf(buf, "Volume for track %d", indx);
			ImGui::SetTooltip(buf);			
		}

		if( pan != lastpan || vol != lastvol ) {
			sprintf(buf, "%dv%d,%d,%d", indx, (2*vol-pan)/2, (2*vol+pan)/2, tracks[indx].vol_midi );
			network.talk(buf);
		}

		ImGui::NextColumn();
		indx++;		
	}
	
	ImGui::Separator();
	ImGui::End();	
}

void show_detail(Trackbuf *track, bool *p_open) {
	char buf[64];

	ImGui::Begin("Detail", p_open);
	ImGui::InputText("Name", track->name, IM_ARRAYSIZE(track->name));
	ImGui::Checkbox("Mute", &track->mute); ImGui::SameLine();
	ImGui::Checkbox("Solo", &track->solo);

	ImGui::PushItemWidth(100);
	int item = track->part + 1;
	ImGui::Combo("Part", &item, partnames2, statebuffer.sections + 1);
	track->part = item - 1;
	
	item = track->channel;
	ImGui::InputInt("Channel", &item);
	track->channel = item;
	
	item = track->bank;
	ImGui::InputInt("Bank", &item);
	track->bank = item;
	
	item = track->program;
	ImGui::InputInt("Program", &item);
	track->program = item;	
	ImGui::PopItemWidth();

	item = track->vol_left;
	ImGui::VSliderInt("Left", ImVec2(29,100), &item, 0, 120); ImGui::SameLine();
	track->vol_left = item;

	item = track->vol_right;
	ImGui::VSliderInt("Right", ImVec2(29,100), &item, 0, 120); ImGui::SameLine();
	track->vol_right = item;
	
	item = track->vol_midi;
	ImGui::VSliderInt("Midi", ImVec2(29,100), &item, 0, 120); 
	track->vol_midi = item;
	
	ImGui::Button("Resample");  ImGui::SameLine();
	ImGui::Button("Reverse");  ImGui::SameLine();
	if( ImGui::Button("Start") ) {
		*p_open = false;
		sprintf(buf, "%dS", track->number );
		network.talk(buf);				
	} ImGui::SameLine();
					
	if( ImGui::Button("Upload") ) {
		*p_open = false;
		sprintf(buf, "%du", track->number );
		network.talk(buf);	
	}
		
	if( ImGui::Button("Save") ) {
		*p_open = false;
		sprintf( buf, "%dn%s", track->number, track->name ); 
		network.talk(buf);
		sprintf( buf, "%dv%d,%d,%d", track->number, track->vol_left, track->vol_right, track->vol_midi ); 
		network.talk(buf);
		sprintf( buf, "%dm%d", track->number, track->mute ); 
		network.talk(buf);
		sprintf( buf, "%ds%d", track->number, track->solo ); 
		network.talk(buf);
		if( !track->longtrack ) {
			sprintf( buf, "%dp%d", track->number, track->part );
			network.talk(buf);
		} 
		sprintf( buf, "%dc%d", track->number, track->channel ); 
		network.talk(buf);			
		sprintf( buf, "%db%d,%d", track->number, track->bank, track->program ); 
		network.talk(buf);			
	} ImGui::SameLine();

	if( ImGui::Button("Return") ) *p_open = false;
	ImGui::SameLine();

	if( ImGui::Button("Delete") ) {
		*p_open = false;
		sprintf( buf, "%dd", track->number ); 
		network.talk(buf);
	}
	
	ImGui::End();	
	
}

char *chat_buffer;
char chat_input[64];

void show_chat(bool *p_open) {
	
	char *nb;
	
	ImGui::Begin("Chat", p_open);
	
	ImGui::BeginChild("chattext", ImVec2(0,300), true);
	ImGui::Text(chat_buffer);
	ImGui::EndChild();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Text chat from yourself and others will appear here");

	ImGui::InputText("", chat_input, IM_ARRAYSIZE(chat_input)); ImGui::SameLine();
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Enter your text chat here");
	if( ImGui::Button("Send") ) {
		nb = new char[strlen(chat_buffer)+strlen(chat_input)+2];
		strcpy(nb, chat_buffer);
		strcat(nb, chat_input);
		strcat(nb, "\n");
		delete chat_buffer;
		chat_buffer = nb;				
	}
	if (shHints && ImGui::IsItemHovered())
		ImGui::SetTooltip("Send the text chat you entered to the server");

	ImGui::End();
	
}

unsigned char largebuff[PORTBUFSIZE];

int main(int argc, char **argv) {

	Trackbuf trackbuffer;

	chat_buffer = new char[1];
	chat_buffer[0] = '\0';

	// establish connection ports
	if( argc > 1 ) 
		network.listener_init(argv[1]);
	else
		network.listener_init(INPORT);
		
	if( argc > 2 ) 
		network.talker_init("127.0.0.1", argv[2]);
	else
		network.talker_init("127.0.0.1", OUTPORT);

    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1000, 600, "LoopR Client", NULL, NULL);
    glfwMakeContextCurrent(window);

    // Setup ImGui binding
    ImGui_ImplGlfw_Init(window, true);

	unsigned char *lptr;
	int indx, last_div = -1, tc;
    ImVec4 clear_color = ImColor(114, 144, 154);

    // Main loop
    while (running && !glfwWindowShouldClose(window))
    {
		// wait on listen for packet
		network.listen(largebuff, PORTBUFSIZE);

		// decode statebuffer
		lptr = largebuff;
		memcpy( &statebuffer, lptr, sizeof(Statebuf) );
		lptr = lptr + sizeof(Statebuf);
		
		// update config values
		cinternal = statebuffer.cinternal;
		autoupload = statebuffer.autoupload;
		longtracks = statebuffer.longtracks;
	
		// copy section definitionss
		memcpy( sections, lptr, sizeof(int) * statebuffer.sections );
		lptr = lptr + sizeof(int) * statebuffer.sections ;

		// process the tracks.  read each track from the packet
		tc = statebuffer.trackcount;
		indx = 0;
		while(tc--) {
			// copy track data into buffer and track array
			memcpy(&trackbuffer, lptr, sizeof(Trackbuf) );
			tracks[indx++] = trackbuffer;
			lptr = lptr + sizeof(Trackbuf);
		}

		// determine the present division
		current_div = (statebuffer.divisions * statebuffer.framecount) / statebuffer.maxframes;

		// poll for events from glfw
		glfwPollEvents();

		// check keyboard for any cheet codes
		char buf[64];
		ImGuiIO& io = ImGui::GetIO();
		for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++) {
			//if (ImGui::IsKeyPressed(i)) fprintf(stderr, "key=%d\n", i);
			if (ImGui::IsKeyPressed(i)) switch( i ) {
				case '.': if( io.KeyShift ) network.talk(">"); else network.talk("."); break; 
				case ',': if( io.KeyShift ) network.talk("<"); else network.talk(","); break;
				case ' ': case 'r': network.talk("r"); break;
				case 'w': case 'W': network.talk("w"); break;
				case 'x': case 'X':
					sprintf(buf, "%dd", statebuffer.trackcount -1 );
					network.talk(buf);
					break;
				case '/': shTransport = shControls = shMains = shPanel = shPlaying = shMixer = shChat = true; break;
			}
		}
		
		// begin our windowing
        ImGui_ImplGlfw_NewFrame();
		
		//  update the input and output meters
		if( left_in_meter ) left_in_meter--;
		if( right_in_meter ) right_in_meter--;
		if( midi_in_meter ) midi_in_meter--;	
		
		if( statebuffer.audio_L_level_in > left_in_meter ) 
			left_in_meter = statebuffer.audio_L_level_in;
		if( statebuffer.audio_R_level_in > right_in_meter ) 
			right_in_meter = statebuffer.audio_R_level_in;
		if( statebuffer.midi_level_in > midi_in_meter ) 
			midi_in_meter = statebuffer.midi_level_in;

		if( left_out_meter ) left_out_meter--;
		if( right_out_meter ) right_out_meter--;
		if( midi_out_meter ) midi_out_meter--;	
		
		if( statebuffer.audio_L_level_out > left_out_meter ) 
			left_out_meter = statebuffer.audio_L_level_out;
		if( statebuffer.audio_R_level_out > right_out_meter ) 
			right_out_meter = statebuffer.audio_R_level_out;
		if( statebuffer.midi_level_out > midi_out_meter ) 
			midi_out_meter = statebuffer.midi_level_out;

		// update tempo slider and BPM/maxframes display
		tempo = 100 - ( ((statebuffer.maxframes-MIN_FRAMES)*100) / (MAX_FRAMES-MIN_FRAMES) );
		if( bpm_mode )
			sprintf( tempo_str, "%d.%1d", statebuffer.bpm/10, statebuffer.bpm % 10 );
		else
			sprintf( tempo_str, "%d", (unsigned int) statebuffer.maxframes );
	
		// update volume controls
		left_vol = (int)(100.00*statebuffer.volume_left);
		right_vol = (int)(100.00*statebuffer.volume_right);
		midi_vol = (int)(100.00*statebuffer.volume_midi);

		// now display and update main windows
		if( shTransport ) show_transport(&shTransport);
		if( shControls ) show_controls(&shControls);
		if( shPanel ) show_panel(&shPanel);
		if( shMains ) show_mains(&shMains);
		if( shPlaying ) show_playing(&shPlaying);
		if( shMixer ) show_mixer(&shMixer);
		if( shProgram ) show_program(&shProgram);
		if( shChat ) show_chat(&shChat);
		
		// display any detail windows
		DetailStruct *lptr = NULL, *dptr = detail_head;
		bool p_open;
		while( dptr ) {
			p_open = true;
			show_detail( &dptr->track, &p_open );
			if( p_open == false ) {
				if( dptr == detail_head ) {
					detail_head = dptr->next;
					delete dptr;
					if( detail_head ) dptr = detail_head->next;
					else			  dptr = NULL;
				} else {
					lptr->next = dptr->next;
					delete dptr;
					dptr = lptr->next;
				}				
			} else {
				lptr = dptr;
				dptr = dptr->next;
			}
		}
		
      	// Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
        if (show_test_window)
        {
            ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
            ImGui::ShowTestWindow(&show_test_window);
        }

		last_div = current_div;
		
        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplGlfw_Shutdown();
    glfwTerminate();

	network.listener_close();
	network.talker_close();

    return 0;
}
