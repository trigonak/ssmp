#!/bin/sh

if [ $# -lt 2 ];
then
    echo "Usage: ./$0 APPLICATION NUMBER_OF_MESSAGES";
    echo " where APPLICATION is the application to be used for measurements (e.g., one2one)";
    echo " and NUMBER_OF_MESSAGES is the number of messages to be exchanged";
    exit;
fi;

CORES=$(cat /proc/cpuinfo | grep processor | tail -n1 | awk '// {print $3}');
echo "Found $(($CORES+1)) cores";

printf "     ";

for ccol in $(seq 0 1 $CORES); do
    printf "%5d" $ccol
done

echo "";
printf "%s" "------";

for ccol in $(seq 0 1 $((5*$(($CORES+1))))); do
    printf "-"
done

echo "";

for crow in $(seq 0 1 $CORES); do
    printf "%5d |" $crow
    for ccol in $(seq 0 1 $CORES); do
	if [ "$crow" -ne "$ccol" ]
	then
	    res=$(./$1 2 $2 $crow $ccol | grep "ticks" | awk '// {sum+=$NF}; END {printf "%d", sum/NR}')
	    printf "%5d" $res
	else
	    printf "    0"
	fi
    done
    echo "";
done


