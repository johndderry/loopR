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
//	loopR.cpp
//
//	the main() program is here and some stuff done during startup 
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

void interupt_handler( int signal ) {

	if( signal == SIGQUIT ) {
		// signal quit to our system
		running = false;
		// force a failure on the input socket
		network.listener_close();
		return;
	}
	
	if( signal == SIGUSR1 ) {
		// upload the statebuffer
		pthread_t thread_id;	// this is quite wrong being here
		pthread_create( &thread_id, NULL, service_upload_thread, NULL );
		return;
	}
}

pthread_t download_thread_id, upload_thread_id;

bool init_server_request = false;

int handleargs(int argc, char *argv[]) {

	argv++;
	
	while( --argc ) {
		
		if( (*argv)[0] == '-' && (*argv)[1] == 'h' ) {

			fprintf( stdout, 
			"Usage:\n  loopR [-h][-l][-Hclientname][-f]\n  -h	help\n  -l	load files\n  -H	use hosting\n  -i	initialize server\n");
			return 1;
		}

		if( (*argv)[0] == '-' && (*argv)[1] == 'l' )

			load_sequencer(true);

		if( (*argv)[0] == '-' && (*argv)[1] == 'i' )

			init_server_request = true;

		if( (*argv)[0] == '-' && (*argv)[1] == 'H' ) {

			track_hosting = true;
			strncpy( clientname, (*argv)+2, TRACK_NAME_MAX );
			fprintf(stderr, "using host, our name='%s'\n", clientname );
		}
		
		argv++;
	}
	return 0;
}


int start_client() {

	char client_path[256];
	
	if( config.client[0] == '~' ) {
		strcpy( client_path, getenv("HOME") );
		strcat( client_path, config.client + 1 );
	}
	else
		strcpy( client_path, config.client );
		
	fprintf(stderr, "starting client %s\n", client_path );
	if( fork() == 0 ) {
		execlp( client_path, "client", config.outport, config.inport, (char *)NULL );
		return 1;
	}
	return 0;
}
	
	
void increment( char *str ) {
	int len = strlen(str);
	switch( str[len-1] ) {
		case '0': str[len-1] = '1'; break;
		case '1': str[len-1] = '2'; break;
		case '2': str[len-1] = '3'; break;
		case '3': str[len-1] = '4'; break;
		case '4': str[len-1] = '5'; break;
		case '5': str[len-1] = '6'; break;
		case '6': str[len-1] = '7'; break;
		case '7': str[len-1] = '8'; break;
		case '8': str[len-1] = '9'; break;
		case '9': str[len-1] = '0'; 
			switch( str[len-2] ) {
				case '0': str[len-2] = '1'; break;
				case '1': str[len-2] = '2'; break;
				case '2': str[len-2] = '3'; break;
				case '3': str[len-2] = '4'; break;
				case '4': str[len-2] = '5'; break;
				case '5': str[len-2] = '6'; break;
				case '6': str[len-2] = '7'; break;
				case '7': str[len-2] = '8'; break;
				case '8': str[len-2] = '9'; break;
				case '9': str[len-2] = '0'; break;
			}
			break;
	}
				
}

int main (int argc, char *argv[]) {
	
 	core_init();
	
	if( handleargs( argc, argv ) ) return 0;

	if( track_hosting ) {
		
		if( init_server_request ) {
			
			network.putter_init(config.trackput, config.trackhost);
			service_upload_thread( NULL );
			network.putter_close();
			return 0;
		}
		
		network.putter_init(config.trackput, config.trackhost);
		network.getter_init(config.trackget, config.trackhost);
		pthread_mutex_init( &append_track_mut, NULL );
		pthread_create( &download_thread_id, NULL, service_download_thread, &init_server_request );
	}
	
	network.talker_init(config.outhost, config.outport);
	int ret, count = 30;
	do {
		ret = network.listener_init(config.inport);
		if( ret > 0 ) {
			increment( config.inport );
			count--;
		}
	} while( ret > 0 && count );
	if( count == 0 ) 
		fprintf(stderr, "failed to find port to listen on. Aborting"); 
	
	if( count && jack_init() ) {

		fprintf(stderr, "sequencer talking on port %s, listening on port %s\n", 
			config.outport, config.inport );

		// catch our signals for keyboard interupt and usr1 
		signal(SIGQUIT, interupt_handler);
		if( track_hosting )
			signal(SIGUSR1, interupt_handler);

		// possibly start a client interface 
		if( strlen( config.client) > 0 ) start_client();

		/* keep running until stopped by the user or interupt */
		do_commands();

		/* close up shop */
		jack_close();

		fprintf(stderr, "sequencer shutting down\n");
		flush_sequencer(true);
	}
		else
			fprintf(stderr, "jack initialization failure\n");

	if( track_hosting ) {
		// precautionary
		running = false;		
		network.getter_close();
		network.putter_close();
		pthread_mutex_destroy( &append_track_mut );
	}

	network.listener_close();
	network.talker_close();

	// remove any tracks present
	if( state.trackcount ) {		
		Track *t, *track = TrackTail;
		while( track ) {
			t =  track->prev;
			delete track;
			track = t;
		}
	}

	return 0;
}
