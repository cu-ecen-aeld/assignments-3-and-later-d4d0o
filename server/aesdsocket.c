/**
* @author David Peter
* A bunch of sockets with some threads and a daemon trying to escape a crazy temporary file that logs everything they do
*/
#include "aesdsocket.h"
#include "aesd_ioctl.h"

static void signal_handler ( int signal_number )
{
    int err = errno;

    DEBUG_LOG("SIGNAL! SIGNAL! SIGNAL! %s {%s}", strsignal (signal_number), __func__);
    if ( signal_number == SIGINT || signal_number == SIGTERM) {
        signal_to_get_out = true;
    }

    errno = err;
}

#ifdef DO_TIMESTAMP
void * timestamp_thread (void *handler)
{
    time_t  timenow;
    int rc;
    ssize_t count;

    handlers_t * hdl = (handlers_t *)handler;

    hdl->buffpt = (char *) malloc(TIMEBUFFLEN * sizeof(char));
    if (NULL == hdl->buffpt) {
        ERROR_LOG("buffer memory allocation {%s}", __func__);
        goto end;
    }

    while ( !signal_to_get_out ) {

        sleep(10);

        timenow = time(NULL);
        strftime(hdl->buffpt, (TIMEBUFFLEN * sizeof(char)), TIMEBUFFFORMAT, localtime(&timenow));

        // Locking the file for writing
        rc = pthread_mutex_lock(hdl->pmutex);
        if ( rc != 0 ) {
            ERROR_LOG("pthread_mutex_lock failed with %d {%s}", rc, __func__);
            goto end;
        }

        DEBUG_LOG("%s", hdl->buffpt);
        count = write (*hdl->ptmpfd, hdl->buffpt, strlen(hdl->buffpt));
        if (-1 == count) {
            ERROR_LOG("write problem {%s}", __func__);
            goto end;
        }

        rc = pthread_mutex_unlock(hdl->pmutex);
        if ( rc != 0 ) {
            ERROR_LOG("pthread_mutex_unlock failed with %d {%s}", rc, __func__);
            goto end;
        }
    }

    end:
    if (NULL != hdl->buffpt) {
        DEBUG_LOG("FREE timestamp buffpt {%s}", __func__);
        free(hdl->buffpt);
        hdl->buffpt = NULL;
    }

    return NULL;
}
#endif

void initialize_handler (handlers_t * hdl_table, unsigned int threadnumber, pthread_mutex_t * pmutex, int * ptmpfd)
{
    hdl_table[threadnumber].pmutex = pmutex;
    hdl_table[threadnumber].ptmpfd = ptmpfd;

    hdl_table[threadnumber].pthread = 0;
    hdl_table[threadnumber].buffpt = NULL;
    hdl_table[threadnumber].clientfd = -1;
    hdl_table[threadnumber].clentaddr = NULL;
}


void clean_handlers (handlers_t * hdl_table, unsigned int threadcount)
{
    for (int i = 0; i < threadcount; i++)
    {
        if (0 != hdl_table[i].pthread) {
            DEBUG_LOG("CANCEL thread #%d {%s}", i, __func__);
            pthread_cancel(hdl_table[i].pthread);

            DEBUG_LOG("JOINING thread #%d {%s}", i, __func__);
            pthread_join(hdl_table[i].pthread, NULL);

            hdl_table[i].pthread = 0;
        }

        if (NULL != hdl_table[i].buffpt) {
            DEBUG_LOG("FREE buffpt thread #%d {%s}", i, __func__);
            free(hdl_table[i].buffpt);
            hdl_table[i].buffpt = NULL;
        }

        if (-1 != hdl_table[i].clientfd) {
            DEBUG_LOG("CLOSE clientfd thread #%d {%s}", i, __func__);
            close(hdl_table[i].clientfd);
            hdl_table[i].clientfd = -1;
        }
    }
}


int main (int argc, char** argv)
{
    int success = EXIT_SUCCESS;
    int status;

    unsigned int threadnumber = 0;

    // [1] Things to free or close are numbered with '[]'
    openlog (NULL, 0, LOG_USER);

    // Signal management
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        ERROR_LOG("errno %d (%s) registering for SIGTERM {%s}",errno,strerror(errno), __func__);
        END (EXIT_FAILURE);
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        ERROR_LOG("errno %d (%s) registering for SIGINT {%s}",errno,strerror(errno), __func__);
        END (EXIT_FAILURE);
    }


    // [2] Socket preparation
    struct addrinfo *servinfo = NULL;
    int sockfd = -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo (NULL, SOCKPORT, &hints, &servinfo);
    if (0 != status) {
        ERROR_LOG("getaddrinfo(): %s {%s}", gai_strerror(status), __func__);
        END (EXIT_FAILURE);
    }

    // [3]
    sockfd = socket (servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (-1 == sockfd) {
        ERROR_LOG("errno %d (%s) getting a socket {%s}",errno,strerror(errno), __func__);
        END (EXIT_FAILURE);
    }

    status = bind (sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (-1 == status) {
        ERROR_LOG("errno %d (%s) binding to the socket {%s}",errno,strerror(errno), __func__);
        END (EXIT_FAILURE);
    }

    status = listen (sockfd, BACKLOG);
    if (-1 == status) {
        ERROR_LOG("errno %d (%s) listening to the socket {%s}",errno,strerror(errno), __func__);
        END (EXIT_FAILURE);
    }



    // Daemon creation if requested
    pid_t pid;
    if (argc >= 2) {
        if ( 0 == strcmp(argv[1], "-d") ) {
            // Daemon option required
            pid = fork ();
            if (-1 == pid) {
                ERROR_LOG("errno %d (%s) forking for daemon {%s}",errno,strerror(errno), __func__);
                END (EXIT_FAILURE);
            }
            else if (0 != pid) { // Parent has to quit
                // watchout DO NOT cleanup
                exit(EXIT_SUCCESS);
            }

            // Child reset
            // create new session and process group
            if (-1 == setsid ()) {
                ERROR_LOG("errno %d (%s) creating daemon session & process group {%s}",errno,strerror(errno), __func__);
                END (EXIT_FAILURE);
            }

            // set the working directory to the root directory
            if (-1 == chdir ("/")) {
                ERROR_LOG("errno %d (%s) setting daemon root directory {%s}",errno,strerror(errno), __func__);
                END (EXIT_FAILURE);
            }

            // close all open files--NR_OPEN is overkill, but works
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            // redirect fd's 0,1,2 to /dev/null
            open ("/dev/null", O_RDWR);   // stdin
            open ("/dev/null", O_RDWR);   // stdout
            open ("/dev/null", O_RDWR);   // stderr
        }
    }

    // [4] messaging file storage creation
    int tmpfd = -1;
#ifndef USE_AESD_CHAR_DEVICE
    tmpfd = open (TMPFILE, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (-1 == tmpfd) {
        ERROR_LOG("errno %d (%s) opening the tmp file {%s}", errno, strerror(errno), __func__);
        END (EXIT_FAILURE);
    }
#endif

    // file protected by mutex
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


    // [5] Threads, buffers, and descriptors structure table
    // The tmp file and the mutex address will be passed to everybody and can be cleaned or reset by anybody
    // The table will collect memory buffer and socket descriptor for further cleanup
    handlers_t * hdl_table = NULL;
    hdl_table = (handlers_t *) malloc((BACKLOG+1) * sizeof (handlers_t));
    if (NULL == hdl_table) {
        ERROR_LOG("handlers_t table memory allocation {%s}", __func__);
        END (EXIT_FAILURE);
    }


    // the 1st thread created is reserved for the 10 sec timestamp
    threadnumber = 0;
    initialize_handler (hdl_table, threadnumber, &mutex, &tmpfd);
#ifdef DO_TIMESTAMP
    status = pthread_create (&hdl_table[0].pthread, NULL, timestamp_thread, (void *) &hdl_table[0]);
    if (0 != status) {
        ERROR_LOG("creation of timestamp thread code %d {%s}", status, __func__);
        END (EXIT_FAILURE);
    }
#endif

    /*
     * timer_create even after a call to timer_delete was generating a leakage report in valgrind
     *
    timer_t timerid;

    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_thread;
    sev.sigev_value.sival_ptr = &hdl_table[0];

    status = timer_create (CLOCK_REALTIME, &sev, &timerid);
    if (0 != status) {
        ERROR_LOG("errno %d (%s) timer_create {%s}", errno, strerror(errno), __func__);
        END (EXIT_FAILURE);
    }

    struct itimerspec itimerspec;
    memset(&itimerspec, 0, sizeof(struct itimerspec));
    itimerspec.it_value.tv_sec = 10;
    itimerspec.it_interval.tv_sec = 10;

    status = timer_settime(timerid, TIMER_ABSTIME, &itimerspec, NULL );
    if (0 != status) {
        ERROR_LOG("errno %d (%s) timer_settime {%s}", errno, strerror(errno), __func__);
        END (EXIT_FAILURE);
    }
    */


    // [5'] Accept continuously new connections and start for each for one a new server-client applicative thread
    struct sockaddr their_addr;
    socklen_t addr_size = sizeof (their_addr);

    while ( !signal_to_get_out ) {

        // handlers table reallocation to prepare for a new applicative thread launched by accepting a connection
        threadnumber++;
        initialize_handler (hdl_table, threadnumber, &mutex, &tmpfd);


        memset(&their_addr, 0, addr_size);
        status = accept(sockfd, &their_addr, &addr_size);
        if (-1 == status) {
            if(EINTR == errno){
                DEBUG_LOG("errno %d (%s) catch of EINTR in accept() {%s}", errno, strerror(errno), __func__);
                END (EXIT_SUCCESS);
            }
            if(signal_to_get_out){
                DEBUG_LOG("errno %d (%s) catch of EINTR in accept() [I had treated it] {%s}", errno, strerror(errno), __func__);
                END (EXIT_SUCCESS);
            }
            ERROR_LOG("errno %d (%s) accepting socket {%s}", errno, strerror(errno), __func__);
            END (EXIT_FAILURE);
        }

        hdl_table[threadnumber].clientfd = status;
        hdl_table[threadnumber].clentaddr = inet_ntoa( ((struct sockaddr_in *)&their_addr)->sin_addr );

        status = pthread_create (&hdl_table[threadnumber].pthread, NULL, server_client_app, (void *) &hdl_table[threadnumber]);
        if (0 != status) {
            ERROR_LOG("creation of server/client app thread code %d {%s}", status, __func__);
            END (EXIT_FAILURE);
        }
    }


    // Exit cleanup management
    end:
    DEBUG_LOG("GOODBYE :)");

    // [5]
    if (NULL != hdl_table) {
        clean_handlers(hdl_table, threadnumber + 1);

        DEBUG_LOG("FREE hdl_table {%s}", __func__);
        free(hdl_table);
        hdl_table = NULL;
    }

#ifndef USE_AESD_CHAR_DEVICE
    // [4]
    DEBUG_LOG("CLOSE tmp file {%s}", __func__);
    close (tmpfd);
    tmpfd = -1;
#endif

    // [3]
    if (-1 != sockfd) {
        DEBUG_LOG("SHUTDOWN socketfd {%s}", __func__);
        shutdown(sockfd, SHUT_RDWR);
        //close (sockfd);
    }

    // [2]
    if (NULL != servinfo) {
        DEBUG_LOG("FREE service info {%s}", __func__);
        freeaddrinfo (servinfo);
    }

    // [1]
    DEBUG_LOG("CLOSE syslog {%s}", __func__);
    closelog();

    DEBUG_LOG("success = %d {%s}", success, __func__);
    exit (success);
}



void * server_client_app (void * handler /*int friendfd, char * client_addr, int tmpfd, pthread_mutex_t * pmutex*/)
{
    int ret;

    ssize_t towrite;
    ssize_t count;
    int buffercount = 0;

    char *charpt; // will be a pointer on the identified '\n' character position
    char *tmppt; // will be a temporary pointer to safely reallocate the buffer

    off_t filepos;
    ssize_t written;

    handlers_t * hdl = (handlers_t *)handler;


    syslog (LOG_DEBUG, "Accepted connection from %s", hdl->clentaddr);

    // Loop back to receive once response sent
    while ( !signal_to_get_out ) {
        // Loop reception until a '\n' is obtained. Then will write everything to the tmp file

        if (NULL == hdl->buffpt) {
            hdl->buffpt = (char *)malloc(BUFFLEN * sizeof(char));

            if (NULL == hdl->buffpt) {
                ERROR_LOG("buffer dynamic allocation failed {%s}", __func__);
                goto end;
            }

            buffercount = 1;
        }

        towrite = 0;

        // Reception (blocking) as many buffer length as necessary to obtain a \n before processing
        while( (0 == towrite) && !signal_to_get_out) {
            count = recv(hdl->clientfd, (hdl->buffpt + ((buffercount-1)*BUFFLEN)), BUFFLEN, 0);
            if (-1 == count) {
                if(EINTR == errno){
                    DEBUG_LOG("errno %d (%s) catch of EINTR in recv() {%s}", errno, strerror(errno), __func__);
                    goto end;
                }
                if(signal_to_get_out){
                    DEBUG_LOG("errno %d (%s) catch of EINTR in recv() [I treated it] {%s}", errno, strerror(errno), __func__);
                    goto end;
                }
                ERROR_LOG("errno %d (%s) recv() {%s}", errno, strerror(errno), __func__);
                goto end;
            }
            else if (0 == count) {
                // client disconnected, let's terminate
                DEBUG_LOG("client disconnected (descriptor %d) {%s}", hdl->clientfd, __func__);
                goto end;
            }

            charpt = strchr(hdl->buffpt, '\n');
            if(NULL == charpt) {
                // no newline, allocation of another buffer
                buffercount++;
                tmppt = realloc(hdl->buffpt, (buffercount * BUFFLEN) * sizeof(char));
                if (NULL == tmppt) {
                    ERROR_LOG("buffer reallocation failed {%s}", __func__);
                    goto end;
                }
                hdl->buffpt = tmppt;
            }
            else {
                towrite = charpt - hdl->buffpt + 1;
            }
        }

        ret = pthread_mutex_lock(hdl->pmutex);
        if ( ret != 0 ) {
            ERROR_LOG("pthread_mutex_lock failed with %d {%s}", ret, __func__);
            goto end;
        }

#ifndef USE_AESD_CHAR_DEVICE
        // Positioning at the end of the tmp file to append data
        filepos = lseek (*hdl->ptmpfd, 0, SEEK_END);
        if (-1 == filepos) {
            ERROR_LOG("errno %d (%s) lseek() EOF {%s}", errno, strerror(errno), __func__);
            goto end;
        }
#else
        if (NULL == strstr(hdl->buffpt, AESDCHAR_SEEK_CMD)) {
            // if it' s not an ioctl command open char device, write buffer, and close
            DEBUG_LOG("not ioctl open %s", TMPFILE);
	    *hdl->ptmpfd = open (TMPFILE, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
            if (-1 == *hdl->ptmpfd) {
                ERROR_LOG("errno %d (%s) opening the tmp file {%s}", errno, strerror(errno), __func__);
                goto end;
            }
#endif
            DEBUG_LOG("write %s", TMPFILE);
            // Write buffer to the file managing potential incomplete write
            tmppt = hdl->buffpt;
            while(towrite > 0) {
	        DEBUG_LOG("towrite %zu", towrite);
                count = write (*hdl->ptmpfd, tmppt, towrite);
	        DEBUG_LOG("count %zu", count);
                if (-1 == count) {
                    if(EINTR == errno){
                        DEBUG_LOG("errno %d (%s) catch of EINTR in write() {%s}", errno, strerror(errno), __func__);
                        goto end;
                    }
                    if(signal_to_get_out){
                        DEBUG_LOG("errno %d (%s) catch of EINTR in write() [I had treated it] {%s}", errno, strerror(errno), __func__);
                        goto end;
                    }
                    ERROR_LOG("errno %d (%s) write() {%s}", errno, strerror(errno), __func__);
                    goto end;
                }
                tmppt += count;
                towrite -= count;
            }
#ifdef USE_AESD_CHAR_DEVICE

	    DEBUG_LOG("close %s", TMPFILE);
            close (*hdl->ptmpfd);
            *hdl->ptmpfd = -1;
#endif

        // Read and send back the complete file
        //memset(hdl->buffpt, 0, (buffercount * BUFFLEN) * sizeof(char));

#ifndef USE_AESD_CHAR_DEVICE
        // Current position is the end of file, how much do we have to send ? Storing that in {count}
        count = lseek (*hdl->ptmpfd, 0, SEEK_CUR);
        if (-1 == count) {
            ERROR_LOG("errno %d (%s) lseek() CURR {%s}", errno, strerror(errno), __func__);
            goto end;
        }

        // Positioning at the beginning to send back the full content
        filepos = lseek (*hdl->ptmpfd, 0, SEEK_SET);
        if (-1 == filepos) {
            ERROR_LOG("errno %d (%s) lseek() START {%s}", errno, strerror(errno), __func__);
            goto end;
        }
#else
	}

	// open char device, write buffer, and close it
        *hdl->ptmpfd = open (TMPFILE, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
        if (-1 == *hdl->ptmpfd) {
            ERROR_LOG("errno %d (%s) opening the tmp file {%s}", errno, strerror(errno), __func__);
            goto end;
        }

        if (NULL != strstr(hdl->buffpt, AESDCHAR_SEEK_CMD)) {
            struct aesd_seekto seek_ioctl = {0, 0};
            
	    DEBUG_LOG("ioctl string: %s", hdl->buffpt);
            
	    sscanf(hdl->buffpt, AESDCHAR_SEEK_CMD "%i,%i\n", &seek_ioctl.write_cmd, &seek_ioctl.write_cmd_offset);
            
	    DEBUG_LOG("ioctl cmd: %i, offset: %i", seek_ioctl.write_cmd, seek_ioctl.write_cmd_offset);

            ioctl(*hdl->ptmpfd, AESDCHAR_IOCSEEKTO, &seek_ioctl);
	}

	count = BUFFLEN;
#endif

	// Read and send back the complete file
        memset(hdl->buffpt, 0, (buffercount * BUFFLEN) * sizeof(char));

        while(count > 0) {
            filepos = read (*hdl->ptmpfd, hdl->buffpt, (count <= BUFFLEN) ? count : BUFFLEN);
            if (-1 == filepos) {
                perror("Error server client app: read()");
                goto end;
            }
	    else if (0 == filepos) {
                // nothing left to read
                count = 0;
                break;
            }
            
            count -= filepos;

            tmppt = hdl->buffpt;
            while(filepos > 0) {
                written = send(hdl->clientfd, tmppt, filepos, 0);
                if (-1 == written) {
                    perror("Error server client app: Send");
                    goto end;
                }

                tmppt += written;
                filepos -= written;
            }
        }
#ifndef USE_AESD_CHAR_DEVICE
        fsync(*hdl->ptmpfd);
#else
	close (*hdl->ptmpfd);
        *hdl->ptmpfd = -1;
#endif

        ret = pthread_mutex_unlock(hdl->pmutex);
        if ( ret != 0 ) {
            printf("Error server client app: pthread_mutex_unlock failed with %d\n", ret);
            goto end;
        }

        free(hdl->buffpt);
        hdl->buffpt = NULL;
    }

    end:
    if (NULL != hdl->buffpt) {
        DEBUG_LOG("FREE buffpt {%s}", __func__);
        free(hdl->buffpt);
        hdl->buffpt = NULL;
    }

    if (-1 != hdl->clientfd) {
        DEBUG_LOG("CLOSE clientfd {%s}", __func__);
        close(hdl->clientfd);
        hdl->clientfd = -1;
    }

    syslog (LOG_DEBUG, "Closed connection from %s", hdl->clentaddr);
    return NULL;
}
