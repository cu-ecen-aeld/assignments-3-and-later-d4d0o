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
//#include <netinet/in.h>
#include <arpa/inet.h>


#define SOCKPORT    "9000"
#define BUFFLEN     1024*1024
#define TMPFILE     "/var/tmp/aesdsocketdata"


bool signal_to_get_out = false;

static void signal_handler ( int signal_number )
{
    int err = errno;

    printf("Got signal: %s\n", strsignal (signal_number));
    if ( signal_number == SIGINT || signal_number == SIGTERM)
        signal_to_get_out = true;

    errno = err;
}

struct handlers {
    bool logopen;
    int tmpfd;
    struct addrinfo *servinfo;
    int sockfd;
    int friendfd;
    char *buffpt;
};

void clean_all (struct handlers *hdl)
{
    if (NULL != hdl->buffpt) {
        free (hdl->buffpt);
        hdl->buffpt = NULL;
    }
    if (-1 != hdl->friendfd) {
        close (hdl->friendfd);
        hdl->friendfd = -1;
    }
    if (-1 != hdl->sockfd) {
        close (hdl->sockfd);
        hdl->sockfd = -1;
    }
    if (NULL != hdl->servinfo) {
        freeaddrinfo (hdl->servinfo);
        hdl->servinfo = NULL;
    }
    if (-1 != hdl->tmpfd) {
        close (hdl->tmpfd);
        hdl->tmpfd = -1;
    }
    if (false != hdl->logopen) {
        closelog ();
        hdl->logopen = false;
    }
}


int main (int argc, char** argv)
{
    int status;
    struct handlers hdl = { .logopen=false, .tmpfd=-1, .servinfo=NULL, .sockfd=-1, .friendfd=-1, .buffpt=NULL};

    // Signal Handler
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler=signal_handler;
    if( sigaction(SIGTERM, &new_action, NULL) != 0 ) {
        printf("Error %d (%s) registering for SIGTERM",errno,strerror(errno));
        return 1;
    }
    if( sigaction(SIGINT, &new_action, NULL) ) {
        printf("Error %d (%s) registering for SIGINT",errno,strerror(errno));
        return 1;
    }

    openlog (NULL, 0, LOG_USER);
    hdl.logopen = true;

    // Let's go socket crazy

    // There is a need to structure some info
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    status = getaddrinfo (NULL, SOCKPORT, &hints, &hdl.servinfo);
    if (0 != status) {
        fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(status));
        clean_all (&hdl);
        exit(EXIT_FAILURE);
    }

    // Get the file descriptor
    hdl.sockfd = socket (hdl.servinfo->ai_family, hdl.servinfo->ai_socktype, hdl.servinfo->ai_protocol);
    if (-1 == hdl.sockfd) {
        perror("Descriptor");
        clean_all (&hdl);
        exit(EXIT_FAILURE);
    }

    // Bonding with a new friend
    status = bind (hdl.sockfd, hdl.servinfo->ai_addr, hdl.servinfo->ai_addrlen);
    if (-1 == status) {
        perror("Bind");
        clean_all (&hdl);
        exit(EXIT_FAILURE);
    }

    // TODO GO IN DAEMON MODE OPTION
    if (argc >= 2) {
        if ( 0 == strcmp(argv[0], "-d") ) {
            // Yuhuuu Daemonization
            printf("Let's go Daemon\n");
            printf("Well... you have to implement it first");
        }
    }

    // Waiting desperately for a friendly call
    status = listen (hdl.sockfd, 0);
    if (-1 == status) {
        perror("Listen");
        clean_all (&hdl);
        exit(EXIT_FAILURE);
    }


    // messaging file storage creation
    hdl.tmpfd = open (TMPFILE, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (-1 == hdl.tmpfd) {
        perror("TMPFILE");
        clean_all (&hdl);
        exit(EXIT_FAILURE);
    }

    off_t filepos;
    ssize_t written;


    struct sockaddr their_addr;
    socklen_t addr_size = sizeof (their_addr);

    char buffer[BUFFLEN];
    char *tmppt;

    ssize_t count; // will count the quantity read or sent

    char *charpt; // will be a pointer on the identified '\n' character position

    int i; // iteration counter of BUFFLEN allocation memory

    ssize_t towrite;

    // Loop back to connection once disconnected
    while ( !signal_to_get_out ) {

        // Welcome to my friend
        memset(&their_addr, 0, addr_size);

        hdl.friendfd = accept(hdl.sockfd, &their_addr, &addr_size);
        if (-1 == hdl.friendfd) {
            perror("Accept");
            clean_all (&hdl);
            exit(EXIT_FAILURE);
        }
        syslog (LOG_DEBUG, "Accepted connection from %s",
                inet_ntoa( ((struct sockaddr_in *)&their_addr)->sin_addr ));

        // Loop back to receive once response sent
        while (-1 != hdl.friendfd) {
            // Loop reception until a '\n' is obtained. Then will write everything to the tmp file

            if (NULL == hdl.buffpt)
                hdl.buffpt = malloc(BUFFLEN);
            if (NULL == hdl.buffpt) {
                fprintf(stderr, "buffer allocation failed\n");
                clean_all (&hdl);
                exit(EXIT_FAILURE);
            }

            towrite = 0;
            i = 1;

            while( 0 == towrite) {
                count = recv(hdl.friendfd, (hdl.buffpt + ((i-1)*BUFFLEN)), BUFFLEN, 0);
                if (-1 == count) {
                    perror("Receive");
                    clean_all (&hdl);
                    exit (EXIT_FAILURE);
                }
                else if (0 == count) {
                    // Friend ran away, let's get out of that loop to accept a new connection
                    close (hdl.friendfd);
                    hdl.friendfd = -1;
                    break;
                }

                charpt = strchr(hdl.buffpt, '\n');
                if(NULL == charpt) {
                    // no newline, we continue
                    i++;
                    tmppt = realloc(hdl.buffpt, i * BUFFLEN); //use charpt here as a temporary pointer for realloc
                    if (NULL == tmppt) {
                        fprintf(stderr, "buffer allocation failed\n");
                        clean_all (&hdl);
                        exit(EXIT_FAILURE);
                    }
                    hdl.buffpt = tmppt;
                }
                else {
                    towrite = charpt - hdl.buffpt + 1;
                }
            }

            // connection lost
            if (0 == towrite)
                break;

            // Positioning at the end of the tmp file to append data
            filepos = lseek (hdl.tmpfd, 0, SEEK_END);
            if (-1 == filepos) {
                perror("lseek() EOF");
                clean_all (&hdl);
                exit (EXIT_FAILURE);
            }

            // Write that buffer to the file
            tmppt = hdl.buffpt;
            while(towrite > 0) {
                count = write (hdl.tmpfd, tmppt, towrite);
                if (-1 == count) {
                    perror("write()");
                    clean_all (&hdl);
                    exit (EXIT_FAILURE);
                }
                tmppt += count;
                towrite -= count;
            }
            fsync(hdl.tmpfd);
            free(hdl.buffpt);
            hdl.buffpt = NULL;

            // Read and send back

            // Current position is the end of file, how much do we have to send ? Storing that in {count}
            count = lseek (hdl.tmpfd, 0, SEEK_CUR);
            if (-1 == count) {
                perror("lseek() CURRENT");
                clean_all (&hdl);
                exit (EXIT_FAILURE);
            }


            // Positioning at the beginning to send back the full content
            filepos = lseek (hdl.tmpfd, 0, SEEK_SET);
            if (-1 == filepos) {
                perror("lseek() START");
                clean_all (&hdl);
                exit (EXIT_FAILURE);
            }

            while(count > 0) {
                filepos = read (hdl.tmpfd, buffer, (count <= BUFFLEN) ? count : BUFFLEN);
                if (-1 == filepos) {
                    perror("read()");
                    clean_all (&hdl);
                    exit (EXIT_FAILURE);
                }

                count -= filepos;

                tmppt = buffer;
                while(filepos > 0) {
                    written = send(hdl.friendfd, tmppt, filepos, 0);
                    if (-1 == written) {
                        perror("Send");
                        clean_all (&hdl);
                        exit (EXIT_FAILURE);
                    }

                    tmppt += written;
                    filepos -= written;
                }
            }
        }

        // Got disconnected
        syslog (LOG_DEBUG, "Closed connection from %s",
                inet_ntoa( ((struct sockaddr_in *)&their_addr)->sin_addr ));
    }

    // SIGINT or SIGTERM handling to gracefully exit
    printf ("Goodbye\n");
    clean_all (&hdl);
    exit (EXIT_SUCCESS);
}
