all:
	(cd common;	make)
	(cd engine;	make)
	(cd frontend;	make)
	(cd server;	make)

clean:
	(cd common;	make clean)
	(cd engine;	make clean)
	(cd frontend;	make clean)
	(cd server;	make clean)

