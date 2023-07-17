/**
* @author David Peter
* That's the H of the C obviously
*/

#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <syslog.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define SOCKPORT    "9000"
#define BUFFLEN     1024
#define TMPFILE     "/var/tmp/aesdsocketdata"
#define BACKLOG     10

#define TIMEBUFFLEN 64
#define TIMEBUFFFORMAT "timestamp:%a, %d %b %Y %T %z\n"


//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf(msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("ERROR: " msg "\n" , ##__VA_ARGS__)
#define END(state) success=state ; goto end


bool signal_to_get_out = false;


struct handlers {
    pthread_mutex_t * pmutex;   //shared by every thread (NULL if non existant)
    int * ptmpfd;               //shared as well (NULL if non existant)

    pthread_t pthread;          //should be initialized to 0 if non existant
    char * buffpt;              //should be initialized to NULL if non existant
    int clientfd;               //should be initialized to -1 if non existant

    char * clentaddr;           //return of inet_ntoa (no need to be freed !don't know why!)
} typedef handlers_t;

static void signal_handler ( int signal_number );
void * timestamp_thread (void * handler);
void initialize_handler (handlers_t * hdl_table, unsigned int threadnumber, pthread_mutex_t * pmutex, int * ptmpfd);
void clean_handlers (handlers_t * hdl_table, unsigned int threadcount);
void *  server_client_app (void * handler);

#endif
