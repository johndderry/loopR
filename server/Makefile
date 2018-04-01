CXXFLAGS = -g -O0 -Wall

server: server.o ../common/network.o
	g++  -g -O0 -o server server.o ../common/network.o

clean:
	(rm *.o)
	(rm server)
