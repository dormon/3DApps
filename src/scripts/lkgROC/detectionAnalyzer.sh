#!/bin/bash
#set -x
BIN=/home/ichlubna/Workspace/dormon/3DApps/build/quiltDetector
#$1 in folder $2 out folder 
OUT=$(realpath $2)
IN=$(realpath $1)

TIMES_FILE="$OUT/times"
TOTAL_TIME=0
touch $TIMES_FILE
for FOLDER in $IN/*; do
    FOLDER_NAME=$(basename "$FOLDER")
    mkdir "$OUT/$FOLDER_NAME"
    for FILE in $IN/$FOLDER_NAME/*; do
        TIME=$(/usr/bin/time -f "A%eB" 2>&1 $BIN -i $FILE -o "$OUT/$FOLDER_NAME/" -g 1 -m 2)
        TIME=$(echo $TIME | grep -o -P '(?<=A).*(?=B)' | tr -d [:upper:])
        TOTAL_TIME=$(echo "$TOTAL_TIME+$TIME" | bc)
        echo "$FILE : $TIME" >> $TIMES_FILE
    done
done
echo "Total: $TOTAL_TIME"
echo "Total: $TOTAL_TIME" >> $TIMES_FILE
