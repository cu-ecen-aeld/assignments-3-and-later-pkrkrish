#!/bin/sh

# Check if both arguments are provided
if [ $# -lt 2 ]; then
    echo "Error: Missing arguments."
    echo "Usage: $0 <filesdir> <searchstr>"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if filesdir is a valid directory
if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a valid directory."
    exit 1
fi

# Count the number of files in the directory and subdirectories
num_files=$(find "$filesdir" -type f | wc -l)

# Count the number of matching lines containing searchstr
num_matches=$(grep -r "$searchstr" "$filesdir" | wc -l)

# Print the result
echo "The number of files are $num_files and the number of matching lines are $num_matches"
