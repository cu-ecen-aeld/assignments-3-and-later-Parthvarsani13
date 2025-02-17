#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo (Modified for Buildroot environment)

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

# Use configuration files from /etc/finder-app/conf/
username=$(cat /etc/finder-app/conf/username.txt)

if [ $# -lt 3 ]
then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]
    then
        echo "Using default value ${NUMFILES} for number of files to write"
    else
        NUMFILES=$1
    fi  
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

# Read assignment number from /etc/finder-app/conf/
assignment=$(cat /etc/finder-app/conf/assignment.txt)

if [ "$assignment" != "assignment1" ]
then
    mkdir -p "$WRITEDIR"
    if [ -d "$WRITEDIR" ]
    then
        echo "$WRITEDIR created"
    else
        exit 1
    fi
fi

echo "Ensuring writer utility is available in PATH"
if ! command -v writer >/dev/null 2>&1; then
    echo "Error: writer not found in PATH"
    exit 1
fi

for i in $(seq 1 $NUMFILES)
do
    writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")
<<<<<<< HEAD

# Write output to /tmp/assignment4-result.txt
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

# Clean up temporary files
=======

# Write output to /tmp/assignment4-result.txt
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt
# remove temporary directories
>>>>>>> f9886c2 (Update finder-test.sh)
rm -rf /tmp/aeld-data

set +e
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
    exit 1
fi
