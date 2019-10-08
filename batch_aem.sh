#!/bin/bash

make rebuild

rm -rf log/
if [ ! -d log ]
then
    mkdir log
fi

rm -rf appntk/
if [ ! -d appntk ]
then
    mkdir appntk
fi

for file in data/arith/*
do
    if test -f $file
    then
        name=`basename $file`
        filename="${name%%.*}"
        if [[ "$name" == *.blif ]]
        then
            echo ${filename}
            (nohup ./main -i ${file} -s 0 -f 64 -c 30 -m 0.2 > log/${filename}.log &)
        fi
    fi
done
