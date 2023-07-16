/**
* @author David Peter
* A socket with some threads and a daemon trying to escape a crazy temporary file that logs everything they do
*/
#include "aesdsocket.h"

static void signal_handler ( int signal_number )
{
    int err = errno;

    printf("Got signal: %s\n", strsignal (signal_number));
    if ( signal_number == SIGINT || signal_number == SIGTERM) {
        signal_to_get_out = true;
    }

    errno = err;
}



void * timestamp_thread (void *handlers)
{
    int success = EXIT_SUCCESS;

    time_t  timenow;
    int rc = 0;
    ssize_t count = 0;

    handlers_t * hdl = (handlers_t *)handlers;

    hdl->buffpt = (char *) malloc(TIMEBUFFLEN * sizeof(char));
    if (NULL == hdl->buffpt) {
        printf("Error timestamp thread: buffer memory allocation\n");
        END (EXIT_FAILURE);
    }

    while ( !signal_to_get_out ) {

        sleep(10);

        timenow = time(NULL);
        strftime(hdl->buffpt, (TIMEBUFFLEN * sizeof(char)), TIMEBUFFFORMAT, localtime(&timenow));

        // Locking the file for writing
        rc = pthread_mutex_lock(hdl->pmutex);
        if ( rc != 0 ) {
            printf("Error timestamp thread: pthread_mutex_lock failed with %d\n", rc);
            END (EXIT_FAILURE);
        }

        count = write (*hdl->ptmpfd, hdl->buffpt, strlen(hdl->buffpt));
        if (-1 == count) {
            perror(" Error timestamp thread in write()");
            END (EXIT_FAILURE);
        }

        rc = pthread_mutex_unlock(hdl->pmutex);
        if ( rc != 0 ) {
            printf("Error timestamp thread: pthread_mutex_unlock failed with %d\n", rc);
            END (EXIT_FAILURE);
        }
    }

    end:
    if (NULL != hdl->buffpt) {
        printf("FREE timestamp buffpt\n");
        free(hdl->buffpt);
    }

    hdl->active = false;
    exit(success);
}


/*
void clean_all (handlers_t * hdl)
{
    signal_to_get_out = true;
    // tell the threads to exit
    // got through each thread of the table to clean unfree yet buffers and join the thread
    if (-1 != hdl->friendfd) {
        //shutdown(hdl->friendfd, SHUT_RDWR);
        close ( hdl->friendfd);
    }

    if (NULL != hdl->buffpt) {
        free (hdl->buffpt);
    }
    //pthread_join (ptimestamp_thrd, NULL);

    // Clean the main / initializer thread buffers and file descriptors


    if (-1 != hdl->tmpfd) {
        close (hdl->tmpfd);
    }
}
*/


int main (int argc, char** argv)
{
    int success = EXIT_SUCCESS;
    int status = 0;

    /* handlers_t * hdl_table = NULL; */ /*{ .logopen=false,
                       .tmpfd=-1,
                       .servinfo=NULL,
                       .sockfd=-1,
                       .friendfd=-1,
                       .buffpt=NULL,
                       .mutex=PTHREAD_MUTEX_INITIALIZER};*/

    openlog (NULL, 0, LOG_USER);



    // Signal management
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        END (EXIT_FAILURE)
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        END (EXIT_FAILURE)
    }



    // Socket preparation
    struct addrinfo *servinfo = NULL;
    int sockfd = -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo (NULL, SOCKPORT, &hints, &servinfo);
    if (0 != status) {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(status));
        END (EXIT_FAILURE)
    }

    sockfd = socket (servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (-1 == sockfd) {
        perror("Descriptor");
        END (EXIT_FAILURE)
    }

    status = bind (sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (-1 == status) {
        perror("Bind");
        END (EXIT_FAILURE)
    }

    status = listen (sockfd, BACKLOG);
    if (-1 == status) {
        perror("Listen");
        END (EXIT_FAILURE)
    }



    // Daemon creation if requested
    pid_t pid;
    if (argc >= 2) {
        if ( 0 == strcmp(argv[1], "-d") ) {
            // Daemon option required
            pid = fork ();
            if (-1 == pid) {
                perror("Fork");
                END (EXIT_FAILURE)
            }
            else if (0 != pid) { // Parent has to quit
                END (EXIT_SUCCESS)
            }

            // Child reset
            // create new session and process group
            if (-1 == setsid ()) {
                END (EXIT_FAILURE)
            }

            // set the working directory to the root directory
            if (-1 == chdir ("/")) {
                END (EXIT_FAILURE)
            }

            // close all open files--NR_OPEN is overkill, but works
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            // redirect fd's 0,1,2 to /dev/null
            open ("/dev/null", O_RDWR);   // stdin
            open ("/dev/null", O_RDWR);   // stdout
            open ("/dev/null", O_RDWR);   // stderror
        }
    }



    handlers_t * hdl_table = (handlers_t *) malloc(BACKLOG * sizeof (handlers_t));
    if (NULL == hdl_table) {
        printf("Handlers table memory allocation error\n");
        END (EXIT_FAILURE)
    }

    // messaging file storage creation
    int tmpfd = open (TMPFILE, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (-1 == tmpfd) {
        perror("TMPFILE");
        END (EXIT_FAILURE)
    }

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < BACKLOG; i++)
    {
        hdl_table[i].pmutex = &mutex;
        hdl_table[i].ptmpfd = &tmpfd;
        hdl_table[i].buffpt = NULL;
        hdl_table[i].friendfd = -1;
        hdl_table[i].active = true;
    }

    pthread_t ptimestamp_thrd = 0;
    status = pthread_create (&ptimestamp_thrd, NULL, timestamp_thread, (void *) &hdl_table[0]);
    if (0 != status) {
        printf("Error creation of timestamp thread: code %d", status);
        END (EXIT_FAILURE)
    }


    struct sockaddr their_addr;
    socklen_t addr_size = sizeof (their_addr);

    // Accept connections and start the server client applications
    while ( !signal_to_get_out ) {

        // Welcome to my friend
        memset(&their_addr, 0, addr_size);

        status = accept(sockfd, &their_addr, &addr_size);
        if (-1 == status) {
            perror("Accept");
            END (EXIT_FAILURE);
        }

        char * client_addr = inet_ntoa( ((struct sockaddr_in *)&their_addr)->sin_addr );

        server_client_app (status, client_addr, tmpfd, &mutex);
    }


    // Exit cleanup management
    end:
    printf ("Goodbye\n");

    //clean_all (&hdl);

    close (tmpfd);

    free(hdl_table);
    printf("FREE hdl_table");

    if (-1 != sockfd) {
        //shutdown(hdl->sockfd, SHUT_RDWR);
        close (sockfd);
    }

    if (NULL != servinfo) {
        freeaddrinfo (servinfo);
        printf("FREE service info");
    }
    closelog();

    if (0 != ptimestamp_thrd)
        pthread_join(ptimestamp_thrd, NULL);

    exit (success);
}



int server_client_app (int friendfd, char * client_addr, int tmpfd, pthread_mutex_t * pmutex)
{
    int success = EXIT_SUCCESS;
    int ret = 0;

    char * buffpt = NULL;

    ssize_t towrite = 0;
    ssize_t count = 0;
    int buffercount = 0;

    char *charpt; // will be a pointer on the identified '\n' character position
    char *tmppt; // will be a temporary pointer to safely reallocate the buffer

    off_t filepos = 0;
    ssize_t written = 0;


    syslog (LOG_DEBUG, "Accepted connection from %s", client_addr);

    // Loop back to receive once response sent
    while ( !signal_to_get_out ) {
        // Loop reception until a '\n' is obtained. Then will write everything to the tmp file

        if (NULL == buffpt) {
            buffpt = (char *)malloc(BUFFLEN * sizeof(char));

            if (NULL == buffpt) {
                printf("Error server client app : buffer allocation failed\n");
                END (EXIT_FAILURE);
            }

            buffercount = 1;
        }

        towrite = 0;

        // Reception (blocking) as many buffer length as necessary to obtain a \n before processing
        while( (0 == towrite) && !signal_to_get_out) {
            count = recv(friendfd, (buffpt + ((buffercount-1)*BUFFLEN)), BUFFLEN, 0);
            if (-1 == count) {
                printf("Error server client app: receive failed\n");
                END (EXIT_FAILURE);
            }
            else if (0 == count) {
                // client disconnected, let's terminate
                END (EXIT_SUCCESS)
            }

            charpt = strchr(buffpt, '\n');
            if(NULL == charpt) {
                // no newline, allocation of another buffer
                buffercount++;
                tmppt = realloc(buffpt, (buffercount * BUFFLEN) * sizeof(char));
                if (NULL == tmppt) {
                    printf("Error server client app: buffer reallocation failed\n");
                    END (EXIT_FAILURE);
                }
                buffpt = tmppt;
            }
            else {
                towrite = charpt - buffpt + 1;
            }
        }


        // Append the reading to the file
        ret = pthread_mutex_lock(pmutex);
        if ( ret != 0 ) {
            printf("Error server client app: pthread_mutex_lock failed with %d\n", ret);
            END (EXIT_FAILURE);
        }

        printf("DEBUG - %s", buffpt);
        // Positioning at the end of the tmp file to append data
        filepos = lseek (tmpfd, 0, SEEK_END);
        if (-1 == filepos) {
            perror("Error server client app: lseek() EOF");
            END (EXIT_FAILURE);
        }

        // Write buffer to the file managing potential incomplete write
        tmppt = buffpt;
        while(towrite > 0) {
            count = write (tmpfd, tmppt, towrite);
            if (-1 == count) {
                perror("Error server client app: write()");
                END (EXIT_FAILURE);
            }
            tmppt += count;
            towrite -= count;
        }


        // Read and send back the complete file
        memset(buffpt, 0, (buffercount * BUFFLEN) * sizeof(char));

        // Current position is the end of file, how much do we have to send ? Storing that in {count}

        count = lseek (tmpfd, 0, SEEK_CUR);
        if (-1 == count) {
            perror("Error server client app: lseek() CURRENT");
            END (EXIT_FAILURE);
        }

        // Positioning at the beginning to send back the full content
        filepos = lseek (tmpfd, 0, SEEK_SET);
        if (-1 == filepos) {
            perror("Error server client app: lseek() START");
            END (EXIT_FAILURE);
        }

        while(count > 0) {
            filepos = read (tmpfd, buffpt, (count <= BUFFLEN) ? count : BUFFLEN);
            if (-1 == filepos) {
                perror("Error server client app: read()");
                END (EXIT_FAILURE);
            }

            count -= filepos;

            tmppt = buffpt;
            while(filepos > 0) {
                written = send(friendfd, tmppt, filepos, 0);
                if (-1 == written) {
                    perror("Error server client app: Send");
                    END (EXIT_FAILURE);
                }

                tmppt += written;
                filepos -= written;
            }
        }

        fsync(tmpfd);

        ret = pthread_mutex_unlock(pmutex);
        if ( ret != 0 ) {
            printf("Error server client app: pthread_mutex_unlock failed with %d\n", ret);
            END (EXIT_FAILURE);
        }

        free(buffpt);
        buffpt = NULL;
    }


    end:
    printf("FREE server buffpt");
    if (NULL != buffpt) {
        free(buffpt);
    }

    //shutdown(friendfd, SHUT_RDWR);
    close(friendfd);

    syslog (LOG_DEBUG, "Closed connection from %s", client_addr);
    return (success);
}