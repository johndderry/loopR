CXXFLAGS = -g -O0 -Wall

INCLUDES = config.h state.h section.h framecollection.h track.h internalclick.h core.h service.h\
	../common/network.h ../common/udpstruct.h

OBJECTS = config.o state.o section.o framecollection.o track.o internalclick.o core.o service.o\
	../common/network.o

all: loopR editseqfile

loopR: main.cpp $(INCLUDES) $(OBJECTS)
	g++ -g -o loopR main.cpp $(OBJECTS) -ljack -lpthread -lz

editseqfile: editseqfile.cpp $(INCLUDES) $(OBJECTS)
	g++ -g -o editseqfile editseqfile.cpp $(OBJECTS) -ljack -lpthread -lz

clean:
	(rm *.o)
	(rm loopR editseqfile)
