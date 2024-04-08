#!/bin/bash

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

SUMLINES=0
SUMFILES=0
while read -r line; do
   NUMLINES=$(echo $line | cut -d ":" -f 2)
   SUMLINES=$((SUMLINES+NUMLINES))
   SUMFILES=$((SUMFILES+1))
done <<< "$(grep $2 $1/* -c -s)"
echo "The number of files are $SUMFILES and the number of matching lines are $SUMLINES"
exit 0

