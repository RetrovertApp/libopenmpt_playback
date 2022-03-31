#/bin/sh

rm -rf libopenmpt
mkdir libopenmpt

cp -r $1/common libopenmpt/common
cp -r $1/libopenmpt libopenmpt/libopenmpt
cp -r $1/openmpt123 libopenmpt/openmpt123
cp -r $1/sounddsp libopenmpt/sounddsp
cp -r $1/soundlib libopenmpt/soundlib
cp -r $1/src libopenmpt/src
cp $1/LICENSE libopenmpt
cp $1/README.md libopenmpt
