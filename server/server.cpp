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
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../common/udpstruct.h"
#include "../common/request.h"

#define UPLOAD_PORT		"4952"
#define DOWNLOAD_PORT	"4953"

#define BUFFER_SIZE	1024

unsigned char buffer[BUFFER_SIZE];

int init_socket(const char *portname, int *sock) {
	
	struct addrinfo hints, *servinfo, *p;
    int sockfd, rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, portname, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
		
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("init_socket: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("init_socket: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "init_socket: failed to bind socket\n");
		freeaddrinfo(servinfo);
        return 2;
    }

    freeaddrinfo(servinfo);
	*sock = sockfd;
	return 0;
}

int writefromident(int sockfd, unsigned id, int type ) {
	
	int fd, readcount;
	long int len;
	char filename[64];
	if( type ) sprintf(filename, "%d.done", id);
	else		sprintf(filename, "%d.chat", id);
	fd = open(filename, O_RDONLY );
	if( fd < 0 ) {
		// not found, send length 0
		len = 0;
		write( sockfd, &len, sizeof( len ) );
		return -1;
	}
	// determine file length and send first
	len = lseek( fd, 0, SEEK_END );
	lseek( fd, 0, SEEK_SET );
	write( sockfd, &len, sizeof( len ) );
	// now send the file contents
	while( len > 0 ) {
		if( (readcount = read( fd, buffer, BUFFER_SIZE )) <= 0 )
			return -2;
		len -= readcount;
		write( sockfd, buffer, readcount );
	}
	return 0;
}

void maxids(unsigned int *track, unsigned int *chat) {
	
	DIR	*d;
	int cmp;
	char *p, name[64];
	unsigned int m, trackmax = 0, chatmax = 0;
	struct dirent *dir;
	if( !(d = opendir("."))) {
		*track = *chat = 0;
		return;
	}

	while( (dir = readdir(d)) != NULL ) {
		strncpy( name, dir->d_name, 63);
		p = strchr( name, '.');
		if( p == NULL || *(p+1) == '\0') continue;
		
		cmp = strcmp("done", p+1);
		if( cmp == 0 ) {
			*p = '\0';
			m = atoi( name );
			//fprintf(stderr,"%s = (%s)%d\n", dir->d_name, name, m );
			if( m > trackmax ) trackmax = m;			
		}

		cmp = strcmp("chat", p+1);
		if( cmp == 0 ) {
			*p = '\0';
			m = atoi( name );
			//fprintf(stderr,"%s = (%s)%d\n", dir->d_name, name, m );
			if( m > chatmax ) chatmax = m;			
		}
	}
	closedir( d );
	*track = trackmax;
	*chat = chatmax;
}

int process_download_connect(int sockfd) {
	
	int readcount;
	long int len;
	struct download_request request;
	unsigned int maxident[2];
	unsigned char *buffer;
	
	request.close = false;
	while( !request.close ) {
		readcount = read( sockfd, &request, sizeof( request ) );
		if( readcount < (int) sizeof( request )) {
			fprintf(stderr, "(download) invalid read size of %d\n", readcount );
			return -1;
		}
		// decode the address in the request
		fprintf(stderr, "(download) request: close=%d type=%d ident=%x\n", 
			request.close, request.type, request.ident );
		if( request.close ) continue;
		
		if( request.type == REQ_MAX ) {
			maxids(&maxident[0], &maxident[1] );
			write( sockfd, &maxident, sizeof( maxident ) );
		}
		else if( request.type == REQ_STATE ) {
			int fd = open("STATE", O_RDONLY );
			if( fd >= 0 ) {
				len = lseek(fd, 0, SEEK_END);
				lseek(fd, 0, SEEK_SET);
				buffer = new unsigned char[len];
				readcount = read( fd, buffer, len );
				close( fd );
				write( sockfd, buffer, len );
				delete buffer;
			} else
				fprintf(stderr, "(download) failure to read STATE\n");			
		}
		else if( request.type == REQ_SECTIONS ) { 
			int fd = open("SECTIONS", O_RDONLY );
			if( fd >= 0 ) {
				len = lseek(fd, 0, SEEK_END);
				lseek(fd, 0, SEEK_SET);
				buffer = new unsigned char[len];
				readcount = read( fd, buffer, len );
				close( fd );
				write( sockfd, buffer, len );
				delete buffer;
			} else
				fprintf(stderr, "(download) failure to read SECTIONS\n");			
		}
		else if( request.type == REQ_TRACK )
			writefromident( sockfd, request.ident, 0 );
		else if( request.type == REQ_CHAT )
			writefromident( sockfd, request.ident, 1 );
			
	}
	return 0;
}

unsigned int track_uid = 0, chat_uid = 0;
int uniquetrackid() {
	return ++track_uid;
}

int uniquechatid() {
	return ++chat_uid;
}

int process_upload_connect(int sockfd) {
	
	struct upload_request request;
	char filename[64], newname[64];
	int fd, readcount;
	unsigned int id;
	unsigned char *buffer;
	
	request.close = false;
	while( !request.close ) {
		readcount = read( sockfd, &request, sizeof( request ) );
		if( readcount < (int) sizeof( request )) {
			fprintf(stderr, "(upload) invalid read size of %d\n", readcount );
			return -1;
		}
		// decode the the request
		fprintf(stderr, "(upload) request: close=%d type=%d bytecount=%d\n", 
			request.close, request.type, (int)request.bytecount );
		if( request.close ) continue;
		
		if( request.type == REQ_STATE ) {
			buffer = new unsigned char[request.bytecount];
			readcount = read( sockfd, buffer, request.bytecount );
			fd = open("STATE", O_WRONLY|O_CREAT|O_TRUNC, 0777  );
			write( fd, buffer, readcount );
			close( fd );
			delete buffer;
			continue;
		}
		
		if( request.type == REQ_SECTIONS ) {
			buffer = new unsigned char[request.bytecount];
			readcount = read( sockfd, buffer, request.bytecount );
			fd = open("SECTIONS", O_WRONLY|O_CREAT|O_TRUNC, 0777  );
			write( fd, buffer, readcount );
			close( fd );
			delete buffer;
			continue;
		}
		
		// now transfer these bytes from socket to a file with name of id
		// using bytecount given in request
		// determine the unique id at this time
		sprintf(filename, "%d.part", (id = uniquetrackid()) );
		fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0777  );
		if( fd < 0 ) {
			fprintf(stderr, "(upload) failure to create file '%s'\n", filename );
			return -1;
		}
		long int count = request.bytecount;
		while( count > 0 ) {
			readcount = read( sockfd, buffer, BUFFER_SIZE );
			count -= readcount;
			write( fd, buffer, readcount );
		}
		close(fd);
		fprintf(stderr, "(upload) transfer complete, sending ident: %d\n", id );
		
		// send unique id back to client
		write( sockfd, &id, sizeof( id ));
		
		// rename the file to the correct name
		sprintf(newname, "%d.done", id);
		int ret = rename( filename, newname );
		if( ret < 0 ) fprintf(stderr, "rename failure");
	}
	return 0;
}


int main (int argc, char *argv[]) {
	
	const char *port, *pname;
	int tmpfd, socket_fd, conn_fd, pid;
	
	// figure out where to start unique id's at
	maxids(&track_uid, &chat_uid);
	fprintf(stderr, "Last unique trackid = %d, chatid = %d\nDetecting other files: ", track_uid, chat_uid );
	if( (tmpfd = open("STATE", O_RDONLY)) >= 0 ) {
		close( tmpfd );
		fprintf(stderr, "STATE " );
	}
	if( (tmpfd = open("SECTIONS", O_RDONLY)) >= 0 ) {
		close( tmpfd );
		fprintf(stderr, "SECTIONS " );
	}
	fprintf(stderr, "done. Now forking...\n" );
	
	// now fork into two process and set up our port details
	int mainpid = fork();
	if( mainpid ) {
		port = DOWNLOAD_PORT;
		pname = "download";
	} else {
		port = UPLOAD_PORT;
		pname = "upload";
	}
	
	if( init_socket(port, &socket_fd) ) return 1;
	
	listen( socket_fd, 10 );
	fprintf(stderr, "(%s) listening on port %s\n", pname, port );
	
	if( mainpid ) // download port
		while( 1 ) {
			
			conn_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL); 
			fprintf(stderr,"(%s) return from accept, forking...\n", pname);
			if( (pid = fork()) == 0 ) {

				fprintf(stderr,"(%s) child processing connect on %s\n", pname, port);
				if( mainpid ) process_download_connect(conn_fd);
				else		  process_upload_connect(conn_fd);
				
				close( conn_fd );
				fprintf(stderr,"(%s) child complete\n", pname);
				break;
				
			} else {
				fprintf(stderr, "(%s) created child pid %d. Looping to call accept.\n", pname, pid);
				close( conn_fd );
			}
		}
	else	// upload port
		while( 1 ) {
			conn_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL); 
			fprintf(stderr,"(%s) return from accept, processing connect on %s\n", pname, port);
			process_upload_connect(conn_fd);
			close( conn_fd );
			fprintf(stderr,"(%s) connect complete, calling accept\n", pname);
		}
	
	close( socket_fd );
	return 0;
}
