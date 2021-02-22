#!/bin/bash

APP=/home/ichlubna/Workspace/dormon/3DApps/build/quiltToNative
TMP=$(mktemp -d)
TMP_RESULT="$TMP/tea.png"
TMP_RESULT_B="$TMP/tea2.png"
TMP_RESULT_DOG="$TMP/teaDog.png"
TMP_RESULT_SUB="$TMP/teaSub.png"
touch $2
: > $2
#$1 input quilt image $2 output csv with focusing evaluation
for F in $(seq -0.4 0.025 0.4); do 
    $APP --input $1 --focus $F --output $TMP_RESULT
    $APP --input $1 --focus $F --tilt 0.05 --pitch 150.0 --output $TMP_RESULT_B
    convert $TMP_RESULT -colorspace Gray -morphology Convolve DoG:10,0,10 -tint 0 $TMP_RESULT_DOG 
    convert $TMP_RESULT -colorspace Gray $TMP_RESULT_B -colorspace Gray -compose minus -composite $TMP_RESULT_SUB
    AVG_DOG=$(convert $TMP_RESULT_DOG -resize 1x1 txt:- | grep -o -P '(?<=\().*?(?=\))' | head -n 1)
    AVG_SUB=$(convert $TMP_RESULT_SUB -resize 1x1 txt:- | grep -o -P '(?<=\().*?(?=\))' | head -n 1)
    printf "%f,%f,%f\n" "$F" "$AVG_DOG" "$AVG_SUB" >> $2
    rm $TMP/*
done
rm -rf $TMP
