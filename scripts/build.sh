#!/bin/bash

mkdir build
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release
cd build
# -j arg is max jobs + 1, here configured for 16 compilation threads

# engine stuff
# time make -j17 TracyClient
# time make -j17 EngineDemo				# super basic demo

# # child apps
# time make -j17 Physarum				# agent-based sim
# time make -j17 VoxelSpace				# voxelspace algorithm renderer
# time make -j17 VoxelSpace_Erode		# visualizing the erosion update
# time make -j17 VoxelSpace_Physarum	# visualizing the physarum sim

# # softbody sim
# time make -j17 SoftBodies				# soft body sim on the CPU
# time make -j17 SoftBodiesGPU			# soft body sim on the GPU ( unfinished )

# # cellular automata
# time make -j17 CABitPlanes			# parallel cellular automata
# time make -j17 CAHistory				# cellular automata with history
# time make -j17 GoL					# basic Game of Life

# # Vertexture Variants
# time make -j17 Vertexture				# point sprite sphere impostors
# time make -j17 VertextureClassic		# Vertexture, with the trees
# time make -j17 ProjectedFramebuffers	# projected framebuffers using the vertexture renderer

# this runs all the targets in parallel
	# use the above individual targets to enable / disable piecemeal
time make -j17 all

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
