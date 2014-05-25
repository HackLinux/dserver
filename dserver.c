#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#define MAX_CLIENTS 10
#define MAX_IP 64
#define MAX_INT_CH 11
#define MAX_LINE 2048

char RWMODE[8];
struct Number *list = NULL;

int waitingReads = 0, completedReads = 0;
int waitingWrites = 0, completedWrites = 0;

// counter semaphores
sem_t *wr, *cr, *ww, *cw;

// readers mode semaphores and variables
sem_t *wrt;
sem_t *mutex;
int readcount = 0;

// writers mode semaphores and variables
int readc = 0, writec = 0;
sem_t *mutex1;
sem_t *mutex2;
sem_t *mutex3;
sem_t *w;
sem_t *r;

// fair mode semaphores and variables
sem_t *noWaiting;
sem_t *noAccessing;
sem_t *cm;
int nreaders = 0;

struct ClientParameter {
	char ip[MAX_IP];
	int clientConnResult;
	int port;
};

struct Number {
	struct Number *next;
	int data;
};

struct QueryParameter {
	char* query;
	int clientConnResult;
};

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, 0); 
    long long microseconds = te.tv_sec*1000000LL + te.tv_usec; 
    return microseconds;
}

void addNumber( struct Number **head, int number) {
	if ( *head == NULL) {
		(*head) = (struct Number *) malloc( sizeof( struct Number) );
		(*head)->data = number;
		(*head)->next = NULL;
	}
	else if ( number < (*head)->data ) {
		struct Number *temp = (struct Number *) malloc( sizeof( struct Number) );
		temp->data = number;
		temp->next = (*head);
		(*head) = temp;
	}
	else {
		struct Number *temp = (*head);
		for ( ; temp->next != NULL && temp->data <= number && temp->next->data <= number; temp = temp->next);
		struct Number *num = (struct Number *) malloc( sizeof( struct Number) );
		struct Number *tmp = temp->next;
		num->data = number;
		temp->next = num;
		num->next = tmp;
	}
}

void display( struct Number *head) {
	for ( struct Number *temp = head; temp != NULL; temp = temp->next) {
		fprintf( stdout, "%d ", temp->data);
	}
	
	fprintf( stdout, "\n");
}

void freeNumbers( struct Number **head) {
	if ( (*head) != NULL) {
		while ( (*head) != NULL) {
			struct Number *temp = (*head)->next;
			free(*head);
			(*head) = temp;
		}
	}
}

int exists( struct Number *head, int number) {
	for ( struct Number *temp = head; temp != NULL; temp = temp->next) {
		if ( temp->data == number) {
			return 1;
		}
	}
	
	return 0;
}

void put( struct Number **head, int lower, int upper) {
	for ( int i = lower; i <= upper; i++) {
		if ( !exists( *head, i) ) {
			addNumber( head, i);
		}
	}
}

void deleteNumber( struct Number **head, int number) {
	if ( exists( *head, number) ) {
		if ( (*head)->data == number) {
			struct Number *newHead = (*head)->next;
			free( *head);
			(*head) = newHead;
		}
		else {
			struct Number *temp = (*head);
			for ( ; temp->next != NULL && temp->next->data != number; temp = temp->next);
			struct Number *current = temp->next;
			temp->next = temp->next->next;
			free( current);
		}
	}
}

void delete( struct Number **head, int lower, int upper) {
	for ( int i = lower; i <= upper; i++) {
		if ( exists( *head, i) ) {
			deleteNumber( head, i);
		}
	}
}

struct Number* get( struct Number *head, int lower, int upper) {
	struct Number *result = NULL;
	
	for ( struct Number *temp = head; temp != NULL; temp = temp->next) {
		if ( temp->data >= lower && temp->data <= upper) {
			addNumber( &result, temp->data);
		}
	}
	
	return result;
}

void queryProcessor( void *arg) {
	char query[2048];
	struct QueryParameter *prm = (struct QueryParameter *) arg;
	strcpy( query, prm->query);
	fprintf( stderr, "Query waiting: %s\n", query);
	fflush( stdout);
	
	char readers[8];
	strcpy( readers, "readers");
	char writers[8];
	strcpy( writers, "writers");
	char fair[8];
	strcpy( fair, "fair");
	
	if ( strcmp( RWMODE, readers) == 0) {
		char *token = strtok( query, " ");
		int lowerBound, upperBound;
		
		lowerBound = atoi( strtok( NULL, " ") );
		upperBound = atoi( strtok( NULL, " ") );
		
		if ( strcmp( token, "get") == 0 ) {
			sem_wait( wr);
			long long startTime = current_timestamp();
			waitingReads++;
			sem_post( wr);
			
			sem_wait( mutex);
			readcount++;
			
			if ( readcount == 1) {
				sem_wait( wrt);
			}
			
			sem_post( mutex);
			
			sem_wait( wr);
			waitingReads--;
			sem_post( wr);
			
			// ----------------------------------------------------------------------
			long long endTime = current_timestamp();
			fprintf( stderr, "%s %d %d is processed!\n", token, lowerBound, upperBound);
			fprintf( stdout, "read %d %d %d %d %d\n", waitingReads, completedReads, waitingWrites, completedWrites, (endTime - startTime) );
			fflush( stdout);
			
			struct Number *results = get( list, lowerBound, upperBound);
			// display( results);
			for ( struct Number *tmp = results; tmp != NULL; tmp = tmp->next) {
				char num[MAX_INT_CH];
				sprintf( num, "%d", tmp->data);
				send( prm->clientConnResult, num, MAX_INT_CH, 0);
			}
			
			char ENDRES[MAX_INT_CH];
			strcpy( ENDRES, "ENDRES");
			send( prm->clientConnResult, ENDRES, MAX_INT_CH, 0);
			//-----------------------------------------------------------------------------------------------------
			
			sem_wait( cr);
			completedReads++;
			sem_post( cr);
			
			sem_wait( mutex);
			readcount--;
			
			if ( readcount == 0) {
				sem_post( wrt);
			}
			
			sem_post( mutex);
		}
		else if ( strcmp( token, "put") == 0 || strcmp( token, "delete") == 0) {
			sem_wait( ww);
			long long startTime = current_timestamp();
			waitingWrites++;
			sem_post( ww);
			
			sem_wait( wrt);
			
			sem_wait( ww);
			waitingWrites--;
			sem_post( ww);
			
			//---------------------------------------------------------------------------------------------------
			//fprintf( stdout,"critical section!");
			long long endTime = current_timestamp();
			fprintf( stderr, "%s %d %d is processed!\n", token, lowerBound, upperBound);
			fprintf( stdout, "write %d %d %d %d %d\n", waitingReads, completedReads, waitingWrites, completedWrites, (endTime - startTime) );
			fflush( stdout);
			
			if ( strcmp( token, "put") == 0 ) {
				put( &list, lowerBound, upperBound);
			}
			else {
				delete( &list, lowerBound, upperBound);
			}
			//-----------------------------------------------------------------------------------------------------
			
			sem_wait( cw);
			completedWrites++;
			sem_post( cw);
			
			sem_post( wrt);
		}
	}
	else if ( strcmp( RWMODE, writers) == 0 ) {
		char *token = strtok( query, " ");
		int lowerBound, upperBound;
		
		lowerBound = atoi( strtok( NULL, " ") );
		upperBound = atoi( strtok( NULL, " ") );
		
		if ( strcmp( token, "get") == 0 ) {
			sem_wait( wr);
			long long startTime = current_timestamp();
			waitingReads++;
			sem_post( wr);
			
			sem_wait( mutex3);
			sem_wait( r);
			sem_wait( mutex1);
			
			readc++;
			if ( readc == 1) {
				sem_wait( w);
			}
			
			sem_post( mutex1);
			sem_post( r);
			sem_post( mutex3);
			
			sem_wait( wr);
			waitingReads--;
			sem_post( wr);
			
			//---------------------------------------------------------------------------------------------------------
			long long endTime = current_timestamp();
			fprintf( stderr, "%s %d %d is processed!\n", token, lowerBound, upperBound);
			fprintf( stdout, "read %d %d %d %d %d\n", waitingReads, completedReads, waitingWrites, completedWrites, (endTime - startTime) );
			fflush( stdout);
			
			struct Number *results = get( list, lowerBound, upperBound);
			for ( struct Number *tmp = results; tmp != NULL; tmp = tmp->next) {
				char num[MAX_INT_CH];
				sprintf( num, "%d", tmp->data);
				send( prm->clientConnResult, num, MAX_INT_CH, 0);
			}
			
			char ENDRES[MAX_INT_CH];
			strcpy( ENDRES, "ENDRES");
			send( prm->clientConnResult, ENDRES, MAX_INT_CH, 0);
			//-----------------------------------------------------------------------------------------------------------
			
			sem_wait( cr);
			completedReads++;
			sem_post( cr);
			
			sem_wait( mutex1);
			
			readc--;
			if ( readc == 0) {
				sem_post( w);
			}
			
			sem_post( mutex1);
		}
		else if ( strcmp( token, "put") == 0 || strcmp( token, "delete") == 0) {
			sem_wait( ww);
			long long startTime = current_timestamp();
			waitingWrites++;
			sem_post( ww);
			
			sem_wait( mutex2);
			
			writec++;
			if ( writec == 1) {
				sem_wait( r);
			}
			
			sem_post( mutex2);
			
			sem_wait( w);
			
			sem_wait( ww);
			waitingWrites--;
			sem_post( ww);
			
			//-------------------------------------------------------------------------------------------------------------------
			long long endTime = current_timestamp();
			fprintf( stderr, "%s %d %d is processed!\n", token, lowerBound, upperBound);
			fprintf( stdout, "write %d %d %d %d %d\n", waitingReads, completedReads, waitingWrites, completedWrites, (endTime - startTime) );
			fflush( stdout);
			
			if ( strcmp( token, "put") == 0 ) {
				put( &list, lowerBound, upperBound);
			}
			else {
				delete( &list, lowerBound, upperBound);
			}
			//------------------------------------------------------------------------------------------------------------------
			
			sem_wait( cw);
			completedWrites++;
			sem_post( cw);
			
			sem_post( w);
			
			sem_wait( mutex2);
			
			writec--;
			if ( writec == 0) {
				sem_post( r);
			}
			
			sem_post( mutex2);
		}
	}
	else if ( strcmp( RWMODE, fair) == 0 ) {
		char *token = strtok( query, " ");
		int lowerBound, upperBound;
		int prev, cur;
		
		lowerBound = atoi( strtok( NULL, " ") );
		upperBound = atoi( strtok( NULL, " ") );
		
		if ( strcmp( token, "get") == 0 ) {
			sem_wait( wr);
			long long startTime = current_timestamp();
			waitingReads++;
			sem_post( wr);
			
			sem_wait( noWaiting);
			sem_wait( cm);
			
			prev = nreaders;
			nreaders++;
			
			sem_post( cm);
			
			if ( prev == 0) {
				sem_wait( noAccessing);
			}
			
			sem_post( noWaiting);
			
			sem_wait( wr);
			waitingReads--;
			sem_post( wr);
			
			//---------------------------------------------------------------------------------------------------------
			long long endTime = current_timestamp();
			fprintf( stderr, "%s %d %d is processed!\n", token, lowerBound, upperBound);
			fprintf( stdout, "read %d %d %d %d %d\n", waitingReads, completedReads, waitingWrites, completedWrites, (endTime - startTime) );
			fflush( stdout);
			
			struct Number *results = get( list, lowerBound, upperBound);
			for ( struct Number *tmp = results; tmp != NULL; tmp = tmp->next) {
				char num[MAX_INT_CH];
				sprintf( num, "%d", tmp->data);
				send( prm->clientConnResult, num, MAX_INT_CH, 0);
			}
			
			char ENDRES[MAX_INT_CH];
			strcpy( ENDRES, "ENDRES");
			send( prm->clientConnResult, ENDRES, MAX_INT_CH, 0);
			//-----------------------------------------------------------------------------------------------------------
			
			sem_wait( cr);
			completedReads++;
			sem_post( cr);
			
			sem_wait( cm);
			
			nreaders--;
			cur = nreaders;
			
			sem_post( cm);
			
			if ( cur == 0) {
				sem_post( noAccessing);
			}
		}
		else if ( strcmp( token, "put") == 0 || strcmp( token, "delete") == 0) {
			sem_wait( ww);
			long long startTime = current_timestamp();
			waitingWrites++;
			sem_post( ww);
			
			sem_wait( noWaiting);
			sem_wait( noAccessing);
			sem_post( noWaiting);
			
			sem_wait( ww);
			waitingWrites--;
			sem_post( ww);
			
			//--------------------------------------------------------------------------------------------------------------------
			long long endTime = current_timestamp();
			fprintf( stderr, "%s %d %d is processed!\n", token, lowerBound, upperBound);
			fprintf( stdout, "write %d %d %d %d %d\n", waitingReads, completedReads, waitingWrites, completedWrites, (endTime - startTime) );
			fflush( stdout);
			
			if ( strcmp( token, "put") == 0 ) {
				put( &list, lowerBound, upperBound);
			}
			else {
				delete( &list, lowerBound, upperBound);
			}
			//-------------------------------------------------------------------------------------------------------------------
			
			sem_wait( cw);
			completedWrites++;
			sem_post( cw);
			
			sem_post( noAccessing);
		}
	}
	
	fflush( stdout);
	
	pthread_exit(0);
}

void clientHandler( void *arg) {
	char buf[2048];
	struct ClientParameter *param = (struct ClientParameter *) arg;
	recv( param->clientConnResult, &buf, 2048, 0 );
	char END[2048];
	char ENDCLIENT[MAX_INT_CH];
	strcpy( ENDCLIENT, "ENDCLIENT");
	strcpy( END,"END");
	fprintf( stderr, "Thread created for the client, waiting for the requests from the client...\n");
	fflush( stdout);
	int currentCounter = 5;
	pthread_t* threads = (pthread_t *) malloc( sizeof( pthread_t) * currentCounter );
	int thIndex = 0;
	
	while ( strcmp( buf, END) != 0 ) {
		int lowerBound, upperBound;
		char *token;
		
		pthread_t queryProc;
		struct QueryParameter *queryParam = (struct QueryParameter *) malloc( sizeof( struct QueryParameter) );
		queryParam->query = (char *) malloc( sizeof( char) * strlen( buf) );
		strcpy( queryParam->query, buf);
		queryParam->clientConnResult = param->clientConnResult;
		int queryCreation = pthread_create( &queryProc, NULL, queryProcessor, (void *) queryParam);
		if ( thIndex < currentCounter) {
			threads[thIndex] = queryProc;
			thIndex++;
		}
		else {
			currentCounter += 5;
			threads = (pthread_t *) realloc( threads, sizeof( pthread_t) * currentCounter );
			threads[thIndex] = queryProc;
		}
		
		recv( param->clientConnResult, &buf, 2048, 0 );
	}
	
	for ( int x = 0; x < thIndex; x++) {
		pthread_join( threads[x], NULL);
	}
	
	free( threads);
	
	send( param->clientConnResult, ENDCLIENT, MAX_INT_CH, 0);
	
	close( param->clientConnResult);
	fprintf( stderr, "A Client connection is closed\n");
	fflush( stdout);
	
	pthread_exit(0);
}

int main( int argc, char** argv) {
	char readers[8];
	strcpy( readers, "readers");
	char writers[8];
	strcpy( writers, "writers");
	char fair[8];
	strcpy( fair, "fair");
	
	int portNum = atoi( argv[2]);
	
	strcpy( RWMODE, argv[3]);
	
	sem_unlink( "/wr");
	sem_unlink( "/cr");
	sem_unlink( "/ww");
	sem_unlink( "/cw");
	ww = sem_open( "/ww", O_RDWR | O_CREAT, 0660, 1);
	wr = sem_open( "/wr", O_RDWR | O_CREAT, 0660, 1);
	cr = sem_open( "/cr", O_RDWR | O_CREAT, 0660, 1);
	cw = sem_open( "/cw", O_RDWR | O_CREAT, 0660, 1);
	
	if ( strcmp( RWMODE, readers) == 0 ) {
		sem_unlink( "/wrt");
		sem_unlink( "/mutex");
		wrt = sem_open( "/wrt", O_RDWR | O_CREAT, 0660, 1);
		mutex = sem_open( "/mutex", O_RDWR | O_CREAT, 0660, 1);
	}
	else if ( strcmp( RWMODE, writers) == 0 ) {
		sem_unlink( "/mutex1");
		sem_unlink( "/mutex2");
		sem_unlink( "/mutex3");
		sem_unlink( "/w");
		sem_unlink( "/r");
		mutex1 = sem_open( "/mutex1", O_RDWR | O_CREAT, 0660, 1);
		mutex2 = sem_open( "/mutex2", O_RDWR | O_CREAT, 0660, 1);
		mutex3 = sem_open( "/mutex3", O_RDWR | O_CREAT, 0660, 1);
		w = sem_open( "/w", O_RDWR | O_CREAT, 0660, 1);
		r = sem_open( "/r", O_RDWR | O_CREAT, 0660, 1);
	}
	else if ( strcmp( RWMODE, fair) == 0 ) {
		sem_unlink( "/noWaiting");
		sem_unlink( "/noAccessing");
		sem_unlink( "/cm");
		noWaiting = sem_open( "/noWaiting", O_RDWR | O_CREAT, 0660, 1);
		noAccessing = sem_open( "/noAccessing", O_RDWR | O_CREAT, 0660, 1);
		cm = sem_open( "/cm", O_RDWR | O_CREAT, 0660, 1);
	}
	
	int sockResult;
	int clientConnResult;
	struct sockaddr_in socketAddr, clientAddr;
	socklen_t clientAddrLen;
	int i;
	
	sockResult = socket( AF_INET, SOCK_STREAM, 0);
	bzero( &socketAddr, sizeof( socketAddr) );
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_addr.s_addr = htonl( INADDR_ANY);
	socketAddr.sin_port = htons( portNum);
	bind( sockResult, (struct sockaddr *) &socketAddr, sizeof( socketAddr) );
	
	listen( sockResult, MAX_CLIENTS);
	
	fprintf( stderr, "Waiting for requests...\n");
	fflush( stdout);
	
	pthread_t clientThreads[MAX_CLIENTS];
	int curThread = 0;
	
	while (1) {
		bzero( &clientAddr, sizeof( clientAddr) );
		clientAddrLen = sizeof( clientAddr);
		clientConnResult = accept( sockResult, (struct sock_addr *) &clientAddr, &clientAddrLen);
		pthread_t thResult;
		struct ClientParameter *param = (struct ClientParameter *) malloc( sizeof( struct ClientParameter) );
		strcpy( param->ip, inet_ntop( AF_INET, &(clientAddr.sin_addr), param->ip, MAX_IP) );
		param->port = ntohs( clientAddr.sin_port);
		param->clientConnResult = clientConnResult;
		int ret = pthread_create( &thResult, NULL, clientHandler, (void *) param);
		if ( curThread < MAX_CLIENTS) {
			clientThreads[curThread] = thResult;
			curThread++;
		}
		// pthread_join( thResult, NULL);
		
		if ( ret != 0) {
			fflush( stdout);
			fprintf( stderr, "Thread created for a client\n");
			fflush( stdout);
		}
		
		fflush( stdout);
	}
	
	for ( int y = 0; y < curThread; y++) {
		pthread_join( clientThreads[y], NULL);
	}
	
	if ( strcmp( RWMODE, readers) == 0 ) {
		sem_close( wrt);
		sem_close( mutex);
	}
	else if ( strcmp( RWMODE, writers) == 0 ) {
		sem_close( mutex1);
		sem_close( mutex2);
		sem_close( mutex3);
		sem_close( w);
		sem_close( r);
	}
	else if ( strcmp( RWMODE, fair) == 0 ) {
		sem_close( noWaiting);
		sem_close( noAccessing);
		sem_close( cm);
	}
	
	sem_close( ww);
	sem_close( cw);
	sem_close( wr);
	sem_close( cr);
	
	fflush( stdout);
	
	freeNumbers( &list);
	
	return 0;
}
