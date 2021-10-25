#!/bin/sh
DIR=/home/pp21/share/.testcases/hw1

for dir in $DIR/*.*; do
        echo $dir
        echo $(basename "${dir}")
        ln -s $dir $(basename "${dir}")
done
