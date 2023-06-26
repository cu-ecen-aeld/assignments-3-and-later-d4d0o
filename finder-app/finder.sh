#!/bin/sh
# Assignment 1: finder shell script
# Author: David Peter

# Exit immediately if a command exits with a non-zero status
set -e
# Treat unset variables as an error when substituting
set -u
# Commands for text style
BOLD=$(tput bold)
NORMAL=$(tput sgr0)
ITALIC=$(tput sitm)
UNDERLINE=$(tput smul)


if [ $# -ne 2 ]
then
    echo -e -n "\nusage: finder ${BOLD}FILESDIR SEARCHSTR${NORMAL}\n"
    echo -e -n "\t${UNDERLINE}FILESDIR${NORMAL}:\tpath to a directory on the filesystem\n"
    echo -e    "\t${UNDERLINE}SEARCHSTR${NORMAL}:\ttext string which will be searched within these files\n"
    exit 1
else
    FILESDIR=$1
    SEARCHSTR=$2
fi


if [ ! -d $FILESDIR ]
then
    echo -e "\n${FILESDIR} is not a directory\n"
    exit 1
else
    # find --> display 1 result per line
    # -type f --> filter only the files
    # -maxdepth 1 limit the search to the pointed directory only, no recursivity following down the tree
    # wc - l --> count the nukber of lines
    NUMFILES=$(find ${FILESDIR} -maxdepth 1 -type f | wc -l)

    # grep one line line per matching pattern going through all the directory files
    # wc - l --> count the number of lines
    NUMLINES=$(grep "${SEARCHSTR}" ${FILESDIR}/* | wc -l)

    echo "The number of files are ${NUMFILES} and the number of matching lines are ${NUMLINES}"
fi
