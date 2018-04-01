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
// class NETWORK
//
class Network {

	int in_sockfd, out_sockfd, put_sockfd, get_sockfd;

	//struct sockaddr_storage their_addr;

	struct addrinfo *talker_servinfo, *putter_servinfo, *getter_servinfo, 
		*talker_addr, *putter_addr, *getter_addr;

public:

	bool listener_closed;

	Network();
	~Network();

	int listener_init(const char *), listen(char *, int), listen(void *, int), listener_close();

	int talker_init(const char *, const char *), talk(char *), talk(const char *), talk(void *, int), talker_close();
	
	int putter_init(const char *, const char *), putter_connect(), put_complete(void *src, bool, bool, bool, int), 
		start_put(long int), end_put(unsigned int *), putter_disconnect(), putter_close();
		
	int getter_init(const char *, const char *), get_complete(void *dst, bool, bool, bool, long int), 
		start_get(unsigned int, long int *), get_maxids(unsigned int *, unsigned int *), getter_close();
	
};
