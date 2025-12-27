#!/bin/sh
# Tester script for assignment 4
# Modified to work inside Buildroot/QEMU environment

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

# Requirement 4b: Use the absolute path for config files
# We check if the Buildroot path exists, otherwise fallback to local for testing
CONF_DIR=/etc/finder-app/conf
if [ ! -d "$CONF_DIR" ]; then
    CONF_DIR="../conf" # Fallback for local development
fi

username=$(cat ${CONF_DIR}/username.txt)
assignment=$(cat ${CONF_DIR}/assignment.txt)

# Parse arguments (Keep your existing logic)
if [ $# -lt 3 ]; then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]; then
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

if [ "$assignment" != "assignment1" ]; then
    mkdir -p "$WRITEDIR"
fi

# Requirement 4b: Use 'writer' from PATH (no ./ prefix)
for i in $(seq 1 $NUMFILES)
do
    writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

# Requirement 4b: Run 'finder.sh' from PATH (no ./ prefix)
OUTPUTSTRING=$(finder.sh "$WRITEDIR" "$WRITESTR")

# Requirement 4c: Write output of the finder command to /tmp/assignment4-result.txt
RESULT_FILE=/tmp/assignment4-result.txt
echo "${OUTPUTSTRING}" > "${RESULT_FILE}"

rm -rf /tmp/aeld-data

# Check result
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING}"
    exit 1
fi
