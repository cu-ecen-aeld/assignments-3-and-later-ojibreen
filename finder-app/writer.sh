#!/bin/bash
# Check args.
if [ $# -lt 2 ]; then
   echo "Usage:"
   echo "  2 arguemts are required."
   echo "  Argumemt 1: The path with filename to be used.."
   echo "  AArgument 2: The text to be written."
   exit 1
fi

DIRECTORY=$(dirname $1)

if [ ! -d $DIRECTORY ]; then
   mkdir -p $DIRECTORY
fi

echo $2 > $1
exit 0
