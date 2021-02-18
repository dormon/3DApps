#!/bin/bash
BIN=/home/ichlubna/Workspace/dormon/3DApps/build/quiltDetector
#$1 in folder $2 out folder 
for Y in $(seq 0.0 0.05 0.9); do 
    OUTFILE="$2/$Y.csv"
    touch $OUTFILE
    for FOLDER in $1/*; do
        if [[ -f $FOLDER ]]; then
            continue
        fi
        for FILE in $FOLDER/*_frames.txt; do
            FILE=$(realpath $FILE)
                OUTPUT=$($BIN -f $FILE -o "out" -y $Y | tr -d "\n")
                printf "$FILE,$OUTPUT\n" >> $OUTFILE
        done
    done
done

