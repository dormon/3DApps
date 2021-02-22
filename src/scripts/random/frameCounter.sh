#!/bin/bash
#$1 in folder 
IN=$(realpath $1)
TOTAL=0
for FOLDER in $IN/*; do
    FOLDER_NAME=$(basename $FOLDER)
    for FILE in $IN/$FOLDER_NAME/*; do
        OUTPUT=$(ffmpeg -i $FILE -map 0:v:0 -c copy -f null - 2>&1 )
        NUMBER=$(echo $OUTPUT | grep -o -P '(?<=frame=).*(?=fps)' | xargs)
        TOTAL=$(($TOTAL+$NUMBER))
    done
done
echo $TOTAL
