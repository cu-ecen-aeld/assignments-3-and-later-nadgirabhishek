#!/bin/bash

# Check if the correct number of arguments are provided
if [ "$#" -ne 2 ]; then
  echo "Error: Two arguments are required."
  echo "Usage: $0 <directory-path> <search-string>"
  exit 1
  
fi

filesdir=$1
searchstr=$2

# Check if filesdir is a valid directory
if [ ! -d "$filesdir" ]; then
  echo "Error: The specified path '$filesdir' is not a directory."
  exit 1
fi

# Count the number of files in the directory and its subdirectories
file_count=$(find "$filesdir" -type f | wc -l)

# Count the number of matching lines in all files
matching_lines_count=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

# Print the result
echo "The number of files are $file_count and the number of matching lines are $matching_lines_count"

