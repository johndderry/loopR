/*
MIT License

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
#include <stdlib.h>
#include <string.h>

#include "config.h"

//
// configuration class
//

#define BUFFER_LEN 256

Config::Config() {

	// establish defaults first
	inport = new char[strlen(INPORT)+1];
	strcpy( inport, INPORT );
	outport = new char[strlen(OUTPORT)+1];
	strcpy( outport, OUTPORT );
	outhost = new char[strlen(OUTHOST)+1];
	strcpy( outhost, OUTHOST );
	
	trackget = new char[strlen(TRACKGET)+1];
	strcpy( trackget, TRACKGET );
	trackput = new char[strlen(TRACKPUT)+1];
	strcpy( trackput, TRACKPUT );
	trackhost = new char[strlen(TRACKHOST)+1];
	strcpy( trackhost, TRACKHOST );
	
	client = new char[strlen(CLIENT)+1];
	strcpy( client, CLIENT );

	cinternal = CINTERNAL;
	autoupload = AUTOUPLOAD;
	longtracks = LONGTRACKS;
	downbeat = DOWNBEAT;
	fill = FILL;
	click = CLICK;
	velocity = VELOCITY;

	char filename[BUFFER_LEN], *p, *q;
	strcpy( filename, getenv("HOME")) ;
	strcat( filename, "/.loopR/config" );
	FILE *fd = fopen( filename, "r");
	if( fd == NULL ) {
		perror( filename );
		return;
	}

	char buffer[BUFFER_LEN], parameter[BUFFER_LEN], value[BUFFER_LEN];
	
	while( fgets( buffer, BUFFER_LEN, fd ) ) {

		// copy over parameter and value part

		// allow for no value after the '=' 
		value[0] = '\0';
		
		p = buffer;
		q = parameter;
		while( *p && *p != '=')
			*q++ = *p++;
		*q = '\0';
		
		if( *p != '\0' ) {
			p++;
			q = value;
			while( *p && *p != '\n' )
				*q++ = *p++;
			*q = '\0';
		}
		
		// operate on parameter
		if( strcmp( parameter, "INPORT" ) == 0 ) {
			delete inport;
			inport = new char[strlen(value)+1];
			strcpy(inport, value );
		} else
		if( strcmp( parameter, "OUTPORT" ) == 0 ) {
			delete outport;
			outport = new char[strlen(value)+1];
			strcpy(outport, value );
		} else
		if( strcmp( parameter, "OUTHOST" ) == 0 ) {
			delete outhost;
			outhost = new char[strlen(value)+1];
			strcpy(outhost, value );
		} else
		if( strcmp( parameter, "TRACKPUT" ) == 0 ) {
			delete trackput;
			trackput = new char[strlen(value)+1];
			strcpy(trackput, value );
		} else
		if( strcmp( parameter, "TRACKGET" ) == 0 ) {
			delete trackget;
			trackget = new char[strlen(value)+1];
			strcpy(trackget, value );
		} else
		if( strcmp( parameter, "TRACKHOST" ) == 0 ) {
			delete trackhost;
			trackhost = new char[strlen(value)+1];
			strcpy(trackhost, value );
		} else
		if( strcmp( parameter, "CLIENT" ) == 0 ) {
			delete client;
			client = new char[strlen(value)+1];
			strcpy(client, value );
		} else
		if( strcmp( parameter, "CINTERNAL" ) == 0 ) {
			if( strcmp( value, "true") == 0 )
				cinternal = true;
			if( strcmp( value, "false") == 0 )
				cinternal = false;			
		} else
		if( strcmp( parameter, "AUTOUPLOAD" ) == 0 ) {
			if( strcmp( value, "true") == 0 )
				autoupload = true;
			if( strcmp( value, "false") == 0 )
				autoupload = false;			
		}
		if( strcmp( parameter, "LONGTRACKS" ) == 0 ) {
			if( strcmp( value, "true") == 0 )
				longtracks = true;
			if( strcmp( value, "false") == 0 )
				longtracks = false;			
		}
		else
		if( strcmp( parameter, "DOWNBEAT" ) == 0 ) 
			downbeat = atoi(value);
		else
		if( strcmp( parameter, "FILL" ) == 0 )
			fill = atoi(value);	
		else
		if( strcmp( parameter, "CLICK" ) == 0 )
			click = atoi(value);	
		else
		if( strcmp( parameter, "VELOCITY" ) == 0 )
			velocity = atoi(value);	
	}

	fclose( fd );
}

Config::~Config() {

	char filename[BUFFER_LEN];
	strcpy( filename, getenv("HOME")) ;
	strcat( filename, "/.loopR/config" );

	FILE *fd = fopen( filename, "w");
	if( fd == NULL ) {
		perror( filename );
		return;
	}

	fprintf(fd, "INPORT=%s\nOUTPORT=%s\nOUTHOST=%s\n", inport, outport, outhost );
	fprintf(fd, "TRACKPUT=%s\nTRACKGET=%s\nTRACKHOST=%s\n", trackput, trackget, trackhost );
	fprintf(fd, "CLIENT=%s\n", client );
	if( cinternal ) fprintf(fd, "CINTERNAL=true\n");
	else			fprintf(fd, "CINTERNAL=false\n");
	if( autoupload ) fprintf(fd, "AUTOUPLOAD=true\n");
	else			fprintf(fd, "AUTOUPLOAD=false\n");
	if( longtracks ) fprintf(fd, "LONGTRACKS=true\n");
	else			fprintf(fd, "LONGTRACKS=false\n");
	fprintf(fd, "DOWNBEAT=%d\nFILL=%d\nCLICK=%d\nVELOCITY=%d\n",
		downbeat, fill, click, velocity );

	fclose( fd );
}


		
		

