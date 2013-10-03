#!/bin/bash

echo $# files;
for i in seq 1 $#
do
    if [ -f "$1" ]
    then
	tail -n +4 $1 | cut -d'|' -f 2 > "$1".tmp
	echo "Avg latencies for $1 ---------------------------------------";
	./avg.awk "$1".tmp
	rm -f "$1".tmp >> /dev/null
    else
	echo "$1" does not exist
    fi
    
    shift
done

