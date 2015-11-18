#!/bin/sh
cd tmp
palette="$1.png"

filters="fps=10,scale=iw/2:-1:flags=lanczos"

ffmpeg -v warning -i $1 -vf "$filters,palettegen" -y $palette
ffmpeg -v warning -i $1 -i $palette -lavfi "$filters [x]; [x][1:v] paletteuse" -y $1.gif

cat $1.gif
exit 0
