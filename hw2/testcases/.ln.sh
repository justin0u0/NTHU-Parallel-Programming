#!/bin/sh
DIR=/home/pp21/share/.testcases/hw2

for dir in $DIR/*.*; do
        echo $dir
        echo $(basename "${dir}")
        ln -s $dir $(basename "${dir}")
done
