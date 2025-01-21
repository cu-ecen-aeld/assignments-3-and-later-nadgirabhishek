#!/bin/bash

# Check if the correct number of arguments are provided
if [ "$#" -ne 2 ]; then
  echo "Error: Two arguments are required."
  echo "Usage: $0 <file-path> <text-string>"
  exit 1
fi

writefile=$1
writestr=$2

# Create the directory path if it does not exist
mkdir -p "$(dirname "$writefile")"

# Attempt to write to the file
if echo "$writestr" > "$writefile"; then
  echo "File created successfully with the specified content."
else
  echo "Error: Failed to create or write to the file '$writefile'."
  exit 1
fi

