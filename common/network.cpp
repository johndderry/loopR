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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "network.h"
#include "request.h"

//
//	NETWORK.CPP
//

#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

Network::Network() {

}

Network::~Network() {

}

int Network::listener_init(const char *port)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "listener_init: getaddrinfo %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
		
        if ((in_sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener_init: socket");
            continue;
        }

        if (bind(in_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(in_sockfd);
            perror("listener_init: bind");
            continue;
        }
		fprintf(stderr, "listener_init: successful bind\n");
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener_init: failed to bind socket\n");
        return 2;
    }

	listener_closed = false;

    freeaddrinfo(servinfo);
	return 0;
}

int Network::listen(char *buf, int buflen) {

    int numbytes;
    //char buf[MAXBUFLEN];
    socklen_t addr_len;
    //char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage their_addr;

    //printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(in_sockfd, buf, buflen-1 , 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("Network::listen recvfrom");
        return -1;
    }

    //printf("listener: got packet from %s\n",
    //    inet_ntop(their_addr.ss_family,
    //        get_in_addr((struct sockaddr *)&their_addr),
    //        s, sizeof s));
    //fprintf(stderr, "listener: packet is %d bytes long\n", numbytes);
    //buf[numbytes] = '\0';
    //fprintf(stderr, "listener: packet contains \"%s\"\n", buf);

    return numbytes;
}

int Network::listen(void *p, int bytes) {

    int numbytes;
    //char buf[MAXBUFLEN];
    socklen_t addr_len;
    //char s[INET6_ADDRSTRLEN];
	struct sockaddr_storage their_addr;

    //printf("listener: waiting to recvfrom...\n");

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(in_sockfd, p, bytes, 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("Network::listen recvfrom");
        return -1;
    }

    //printf("listener: got packet from %s\n",
    //    inet_ntop(their_addr.ss_family,
    //        get_in_addr((struct sockaddr *)&their_addr),
    //        s, sizeof s));
    //fprintf(stderr, "listener: packet is %d bytes long\n", numbytes);
    //char *ptr = (char *)p;
	//ptr[numbytes] = '\0';
    //printf("listener: packet contains \"%s\"\n", ptr);

    return numbytes;
}

int Network::listener_close() {

    if( listener_closed ) return 0;

	close(in_sockfd);
	listener_closed = true;

	return 0;
}

//
////////////////////////////////////////////////////////////////////////////
//

int Network::talker_init(const char *hostname, const char *port)
{
    struct addrinfo hints, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &talker_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = talker_servinfo; p != NULL; p = p->ai_next) {
        if ((out_sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker_init: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker_init: failed to create socket\n");
        return 2;
    }

    talker_addr = p;

	return 0;
}

int Network::talk( char *message ) {

    int numbytes;

    if ((numbytes = sendto(out_sockfd, message, strlen(message), 0,
             talker_addr->ai_addr, talker_addr->ai_addrlen)) == -1) {
        perror("Network::talk sendto");
        return -1;
    }

    //fprintf(stderr, "talker: sent %d bytes: %s\n", numbytes, message);

    return numbytes;
}

int Network::talk(const char *message ) {

    int numbytes;

    if ((numbytes = sendto(out_sockfd, message, strlen(message), 0,
             talker_addr->ai_addr, talker_addr->ai_addrlen)) == -1) {
        perror("Network::talk sendto");
        return -1;
    }

    //fprintf(stderr, "talker: sent %d bytes: %s\n", numbytes, message);

    return numbytes;
}

int Network::talk(void *p, int bytes ) {

    int numbytes;

    if ((numbytes = sendto(out_sockfd, p, bytes, 0,
             talker_addr->ai_addr, talker_addr->ai_addrlen)) == -1) {
        perror("Network::talk sendto");
        return -1;
    }

    //printf("talker: sent %d bytes to %s\n", numbytes, message);

    return numbytes;
}

int Network::talker_close() {

    close(out_sockfd);
    freeaddrinfo(talker_servinfo);

	return 0;
}

//
////////////////////////////////////////////////////////////////////////////
//

int Network::putter_init(const char *port, const char *hostname) {

    struct addrinfo hints;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &putter_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
	
	return 0;
}

int Network::putter_connect() {
	
    struct addrinfo *p;
 
   // loop through all the results and make a socket
    for(p = putter_servinfo; p != NULL; p = p->ai_next) {
        if ((put_sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Network::putter_init socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Network putter_init failed to create socket\n");
        return 2;
    }

	putter_addr = p;

    if( connect(put_sockfd, (struct sockaddr *)putter_addr->ai_addr, putter_addr->ai_addrlen) < 0)
    {
		perror("Network::putter_init connect");
		return 1;
    } 

	return 0;
}

int Network::putter_disconnect() {
	
	struct upload_request request;

	memset( &request, 0, sizeof request );
	request.close = true;
	write( put_sockfd, &request, sizeof( request));
	
	close( put_sockfd );
	//freeaddrinfo(putter_servinfo);
	
	return 0;
}


int Network::getter_init(const char *port, const char *hostname) {
	
    struct addrinfo hints, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &getter_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = getter_servinfo; p != NULL; p = p->ai_next) {
        if ((get_sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Network::getter_init socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Network::talker_init failed to create socket\n");
        return 2;
    }

	getter_addr = p;
	//
	// the getter can connect now and stay connected
	//
	if( connect(get_sockfd, (struct sockaddr *)getter_addr->ai_addr, getter_addr->ai_addrlen) < 0)
    {
		perror("Network::getter_init connect");
		return 1;
    } 

	return 0;
}

int Network::put_complete(void *src, bool state, bool sections, bool chat, int bytes) {
	
	struct upload_request request;

	memset( &request, 0, sizeof request );
	request.bytecount = bytes;
	if( state ) request.type = REQ_STATE;
	else if( sections ) request.type = REQ_SECTIONS;
	else if( chat ) request.type = REQ_CHAT;
	else return 1;
	// write request which includes length of data
	write( put_sockfd, &request, sizeof( request));
	// then send the data
	write( put_sockfd, src, bytes );
	
	return 0;
}

int Network::get_complete(void *dst, bool state, bool sections, bool chat, long int size) {
	
	struct download_request request;
	
	memset( &request, 0, sizeof request );
	if( state ) request.type = REQ_STATE;
	else if( sections ) request.type = REQ_SECTIONS;
	else if( chat ) request.type = REQ_CHAT;
	else return 1;
	write( get_sockfd, &request, sizeof( request));
	read( get_sockfd, dst, size );
	return 0;
}

int Network::get_maxids( unsigned int *trackid, unsigned int *chatid ) {
	
	struct download_request request;
	unsigned int ids[2];
	
	memset( &request, 0, sizeof request );
	request.type = REQ_MAX;
	write( get_sockfd, &request, sizeof( request));
	// read the two unsigned ints 
	read( get_sockfd, ids, sizeof( unsigned int ) * 2);
	*trackid = ids[0];
	*chatid = ids[1];
	
	return 0;
}

int Network::start_put(long int size) {

	struct upload_request request;

	memset( &request, 0, sizeof request );
	request.type = REQ_TRACK;
	request.bytecount = size;
	// write request which includes length of data
	write( put_sockfd, &request, sizeof( request));
	
	return put_sockfd;
}

int Network::start_get(unsigned id, long int *size) {

	struct download_request request;
	long int length;
	
	memset( &request, 0, sizeof request );
	request.type = REQ_TRACK;
	request.ident = id;
	// write request which includes length of data
	write( get_sockfd, &request, sizeof( request));
	// read the send length from the first bytes
	read( get_sockfd, &length, sizeof length);
	*size = length;
	
	return get_sockfd;	
}

int Network::end_put( unsigned int *id ) {

	read( put_sockfd, id, sizeof( unsigned int) );
	return 0;
}

int Network::putter_close() {
	
#if 0
	struct upload_request request;

	memset( &request, 0, sizeof request );
	request.close = true;
	write( put_sockfd, &request, sizeof( request));
	
	close( put_sockfd );
#endif

	freeaddrinfo(putter_servinfo);
	
	return 0;
}

int Network::getter_close() {
	
	struct download_request request;

	memset( &request, 0, sizeof request );
	request.close = true;
	write( get_sockfd, &request, sizeof( request));
	
	close( get_sockfd );
	freeaddrinfo(getter_servinfo);

	return 0;
}
