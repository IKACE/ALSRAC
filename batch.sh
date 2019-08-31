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

for file in data/blif_used/*
do
    if test -f $file
    then
        name=`basename $file`
		filename="${name%%.*}"
       	if [[ "$name" == *.blif ]]
       	then
            echo ${filename}
            (nohup ./main -i ${file} -n 64 -m 30 > log/${filename}.log &)
		fi
    fi
done
