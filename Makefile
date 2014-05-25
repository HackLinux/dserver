all: dserver dclient

dserver: dserver.c
	gcc -g -Wall -o dserver dserver.c -std=c99 -lpthread 
	
dclient: dclient.c
	gcc -g -Wall -lpthread -o dclient dclient.c -std=c99
	
clean:
	rm -fr core dclient dserver *~
