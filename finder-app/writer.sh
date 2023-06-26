#!/bin/sh
# Assignment 1: writer shell script
# Author: David Peter


# Commands for text style
BOLD=$(tput bold)
NORMAL=$(tput sgr0)
ITALIC=$(tput sitm)
UNDERLINE=$(tput smul)


if [ $# -ne 2 ]
then
    echo -e -n "\nusage: writer ${BOLD}WRITEFILE WRITESTR${NORMAL}\n"
    echo -e -n "\t${UNDERLINE}WRITEFILE${NORMAL}:\tfilename with absolute path\n"
    echo -e    "\t${UNDERLINE}WRITESTR${NORMAL}:\tstring to be written in the file\n"
    exit 1
else
    WRITEFILE=$1
    WRITESTR=$2
fi

# creation of the absolute path if it doesn't exist
if [ ! -d $(dirname $WRITEFILE) ]
then
    mkdir -p $(dirname ${WRITEFILE})

    if [ $? -ne 0 ]
    then
        echo "cannot crate $(dirname ${WRITEFILE})"
    fi
fi

# creation of a file with its content
echo ${WRITESTR} > ${WRITEFILE}

if [ $? -ne 0 ]
then
    echo "cannot create ${WRITEFILE}"
    exit 1
fi
