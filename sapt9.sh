#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Mod de utilizare: $0 <c>"
    exit 1
fi

char="$1"
count=0


while IFS= read -r line; do
   
    if [[ $line =~ ^[A-Z].*[A-Za-z0-9\ \,\.\!\?]\ [a-z0-9\ \,\.\!\?]*[\.!\?]$ && ! $line =~ ,\ (și|si) ]]; then
       
        if [[ $line =~ $char ]]; then
            ((count++))
        fi
    fi
done

echo "$count"
