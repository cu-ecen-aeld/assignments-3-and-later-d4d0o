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

#define END(state) success=state ; goto end;


bool signal_to_get_out = false;


struct handlers {
    pthread_mutex_t * pmutex;
    int * ptmpfd;

    int friendfd;
    char *buffpt;
    bool active;
} typedef handlers_t;

static void signal_handler ( int signal_number );
void * timestamp_thread (void * handlers);
void clean_all (handlers_t * hdl);
int server_client_app (int friendfd, char * client_addr, int tmpfd, pthread_mutex_t * pmutex);

#endif

/*
off_t filepos;


struct sockaddr their_addr;
socklen_t addr_size = sizeof (their_addr);

char buffer[BUFFLEN];

ssize_t towrite;
*/