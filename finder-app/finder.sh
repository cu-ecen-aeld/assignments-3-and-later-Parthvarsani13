#!/bin/bash

if [ $# -ne 2 ]
then 
	echo "Error: 2 arguements are required -> Filesdir and SearchStr"
	exit 1
fi

filesdir=$1
searchdir=$2

if [ ! -d $filesdir ]
then
	echo "Error: $filesdir does not exist"
	exit 1
fi

filecount=$(find $filesdir -type f 2>/dev/null | wc -l)
match_count=$(grep -rin "$searchdir" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $filecount and the number of matching lines are $match_count"

exit 0