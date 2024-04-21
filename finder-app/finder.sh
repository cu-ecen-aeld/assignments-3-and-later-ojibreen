#!/bin/sh
# Use /bin/sh instead of /bin/bash for assignemtn 3 and later.

if [ $# -lt 2 ]; then
   echo "Usage:"
   echo "  2 arguemts are required."
   echo "  Argumemt 1: The directory to be searched."
   echo "  AArgument 2: The text to be searched within files."
   exit 1
fi

if [ ! -d $1 ]; then
   echo "$1 is not a directory."
   exit 1
fi

NFILES=$(find $1 -type f | wc -l)
NLINES=$(grep -e $2 -r $1 | wc -l)

echo The number of files are $NFILES and the number of matching lines are $NLINES

