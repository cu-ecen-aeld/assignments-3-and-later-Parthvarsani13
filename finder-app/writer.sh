#!/bin/bash

if [ $# -ne 2 ]
then
    echo "Error: Any of the arg is missing -> file_name or string"
    exit 1
fi

writefile=$1
writestr=$2
dirpath=$(dirname "$writefile")

mkdir -p "$dirpath"
if [ $? -ne 0 ]
then
    echo "Error: file could not get created"
    exit 1
fi

echo "$writestr" > "$writefile"
if [ $? -ne 0 ]
then
    echo "Error: couldn't write to the file"
    exit 1
fi

exit 0