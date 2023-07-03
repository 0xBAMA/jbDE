#!/bin/bash

mkdir build
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release
cd build
# -j arg is max jobs + 1, here configured for 16 compilation threads

# engine stuff
# time make -j17 TracyClient
time make -j17 engineDemo				# super basic demo

# child apps
# time make -j17 AirplaneMode2
# time make -j17 FontGame
# time make -j17 Physarum
# time make -j17 Siren
# time make -j17 SoftBodies				# soft body simulation
# time make -j17 Vertexture				# point sprite sphere impostors
# time make -j17 VertextureClassic		# Vertexture, with the trees
# time make -j17 ProjectedFramebuffers	# projected framebuffers using the vertexture renderer
# time make -j17 Voraldo14
# time make -j17 VoxelSpace

# so apparently the default target builds everything in a
# parallel fashion - need to test to see if this is faster
# time make -j16 # I think it would also build the noise tool, tbd
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
