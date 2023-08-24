#!/bin/bash

APP=/home/ichlubna/Workspace/dormon/3DApps/build/quiltToNative
TMP=$(mktemp -d)
TMP_INT=$TMP/int.png
TMP_BLEND=$TMP/blend.ppm
TMP_REFOCUSED=$TMP/ref
touch $3
: > $3
#$1 input quilt image, $2 input folder with separate images form the quilt, $3 output csv with focusing evaluation
for F in $(seq -0.4 0.025 0.4); do
    mkdir $TMP_REFOCUSED
    I=0
    for FILE in $(ls $2 | sort -g); do
        I=$((I+1))
        OFFSET=$(bc -l <<< "1920*$F*(1.0-2*($I/45))")
        convert $2/$FILE -page +$OFFSET+0 -background none -flatten $TMP_REFOCUSED/$FILE 
    done
    $APP --input $1 --focus $F --output $TMP_INT
    convert $TMP_REFOCUSED/* -evaluate-sequence Mean -alpha off $TMP_BLEND
    CONT_BLEND=$(magick $TMP_BLEND -colorspace HCL -format "%[fx:standard_deviation.b]" info:)
    CONT_INT=$(magick $TMP_INT -colorspace HCL -format "%[fx:standard_deviation.b]" info:)
    cd ../../LIQE-main/
    RES_INT=$(python demo2.py $TMP_INT) 
    RES_BLEND=$(python demo2.py $TMP_BLEND) 
    cd -
    LIQE_INT=$(grep -oP '(?<=quality of).*?(?=as quantified)' <<< "$RES_INT")
    LIQE_BLEND=$(grep -oP '(?<=quality of).*?(?=as quantified)' <<< "$RES_BLEND")
    #printf "%f,%f,%f,%f\n" "$CONT_BLEND" "$CONT_INT" "$LIQE_BLEND" "$LIQE_INT" >> $3
    echo "$F", "$CONT_BLEND", "$CONT_INT", "$LIQE_BLEND", "$LIQE_INT" >> $3
    rm -rf $TMP/*
done
rm -rf $TMP
