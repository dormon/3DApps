#!/bin/bash

for FILE in $1/*; do
    NAME=$(basename $FILE | cut -d. -f1)
    mkdir "$2/$NAME"
    ffmpeg -i $FILE -frames:v 45 "$2/$NAME/%04d.png"
    montage "$2/$NAME/*.png" -tile 5x9 -geometry 1920x1080+0+0 "$2/$NAME.jpg"
    convert -flop "$2/$NAME.jpg" "$2/$NAME""_flip.jpg"
done
