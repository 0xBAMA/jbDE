#!/bin/bash

mkdir build
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release
cd build
# -j arg is max jobs + 1, here configured for 16 compilation threads

# engine stuff
# time make -j17 TracyClient
# time make -j17 engineDemo

# child apps
time make -j17 AirplaneMode2
# time make -j17 FontGame
# time make -j17 Physarum
# time make -j17 Siren
# time make -j17 SoftBodies
# time make -j17 Vertexture
# time make -j17 Voraldo14
# time make -j17 VoxelSpace

# so apparently the default target builds everything in a parallel fashion - need to test to see if this is faster
# time make -j5 # I think it would also build the noise tool, tbd
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
	rm -r ./bin
	rm -r ./build
fi
