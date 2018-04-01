# loopR
Live-looping audio application using JACK

Similiar to and inspired by Giada, loopR is a live looping application written in C++ for JACK audio.
Records audio and midi input onto loops and plays them back according to a programmed structure.
Allows for recording on fly, useful in live performance settings. 

Implemented with a server/frontend/server design, this application allows multiple applications 
to run simultaneously on different machines yet work on the same set of loops.
