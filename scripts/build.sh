#!/bin/bash

mkdir build
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release
cd build
time make -j17 exe_base # -j arg is max jobs + 1, here configured for 16 compilation threads
time make -j17 exe_base2
cd ..

if [ "$1" == "noiseTool" ]
then
	cd build/src/FastNoise2/NoiseTool/
	make
	cd ../../..
	cp ./Release/bin/NoiseTool ..
	cd ..
fi

if [ "$1" == "clean" ]
then
	rm -r ./build
fi
