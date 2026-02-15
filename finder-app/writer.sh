#!/bin/bash

numarg=$#

if [ "$numarg" -ne 2 ]
then
  echo "wrong command" 
  exit 1
fi

file=$1
string=$2
dir=$(dirname "$file")

if mkdir -p "$dir"
then
  echo "$string" > "$file"
  exit 0
else
  exit 1
fi