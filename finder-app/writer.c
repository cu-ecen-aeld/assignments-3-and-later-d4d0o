/**
* Assignment 2: writer script C version
* Author: David Peter
*/
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main (int argc, char **argv)
{
	int fd = -1;
	const char* file = NULL;
	const char* buf = NULL;
	size_t count = 0;
	ssize_t nr = -1;

	if (3 != argc) {
		printf ("Usage : %s WRITEFILE WRITESTR\n", argv[0]);
		printf ("\tWRITEFILE:\tfilename with absolute path\n");
		printf ("\tWRITESTR:\tstring to be written in the file\n");
		return 1;
	}

	file = argv[1];
	buf = argv[2];
	count = strlen (buf);

	openlog (NULL, 0, LOG_USER);

	/* file open or creation RWX for owner RX for everybody else*/
	fd = open (file, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if (fd == -1) {
		// printf ("Cannot open or create the file %s\n", file);
		syslog (LOG_ERR, "Cannot open or create the file %s\n", file);

		closelog ();
		return 1;
	}

	/* creation of a file with its content */
	// printf ("Writting %s to %s\n", buf, file);
	syslog (LOG_DEBUG, "Writting %s to %s\n", buf, file);

	nr = write (fd, buf, count);
	if (-1 == nr) {
		// printf ("Error: %s\n", strerror (errno));
		syslog (LOG_ERR, "Error: %s\n", strerror (errno));

		close (fd);
		closelog ();
		return 1;
	}
	else if (nr != count) {
		// printf ("Partial write: only %zu bytes written out of %zu\n", nr, count);
		syslog (LOG_ERR, "Partial write: only %zu bytes written out of %zu\n", nr, count);

		close (fd);
		closelog ();
		return 1;
	}

	close (fd);
	closelog ();
	return 0;
}
