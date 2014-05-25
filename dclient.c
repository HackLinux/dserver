#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define MAX_INT_CH 11
#define MAX_LINE 2048

int main( int argc, char** argv) {
	setbuf( stdout, NULL);
	int socketConnection;
	struct sockaddr_in socketAddr;
	int ret;
	int index;
	int portNum = atoi( argv[2]);
	char line[MAX_LINE];
	
	char res[MAX_INT_CH];
	char ENDRES[MAX_INT_CH];
	char ENDCLIENT[MAX_INT_CH];
	strcpy( ENDCLIENT, "ENDCLIENT");
	strcpy( ENDRES, "ENDRES");
	
	char END[2048];
	strcpy( END,"END");
	
	socketConnection = socket( AF_INET, SOCK_STREAM, 0);
	
	bzero( &socketAddr, sizeof( socketAddr) );
	
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons( portNum);
	
	inet_pton( AF_INET, argv[1], &(socketAddr.sin_addr) );
	
	fprintf( stderr, "Connecting to the server %s:%d...\n", argv[1], portNum);
	ret = connect( socketConnection, (const struct sockaddr *) &socketAddr, sizeof( socketAddr) );
	
	if ( ret != 0) {
		fprintf( stderr, "Connection failed\n");
		exit(0);
	}
	
	FILE *inFile = fopen( argv[3], "r");
	fprintf( stderr, "Posting commands to the server line by line from the file %s...\n", argv[3]);
	
	while ( fgets( line, MAX_LINE, inFile) != NULL ) {
		fprintf( stderr, "%s", line);
		if ( atof( line) == 0 ) {
			send( socketConnection, line, MAX_LINE, 0);
		}
		else {
			sleep( atof( line) );
		}
	}
	
	send( socketConnection, END, MAX_LINE, 0);
	
	fprintf( stderr, "\nLines were sent! The result coming from the server is printed.\n");
	
	fclose( inFile);
	
	recv( socketConnection, &res, MAX_INT_CH, 0);
	while ( strcmp( res, ENDCLIENT) != 0 ) {
		while ( strcmp( res, ENDRES) != 0 ) {
			fprintf( stdout, "%s ", res);
			
			recv( socketConnection, &res, MAX_INT_CH, 0);
		}
		fprintf( stdout, "\n");
		recv( socketConnection, &res, MAX_INT_CH, 0);
	}
	
	close( socketConnection);
	
	return 0;
}
