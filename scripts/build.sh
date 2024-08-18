#!/bin/bash

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

mkdir build
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release
# cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Debug
cd build

# -j arg is max jobs + 1, here configured for 16 compilation threads

# engine stuff
# time make -j$(nproc) TracyClient
# time make -j$(nproc) EngineDemo				# super basic demo

# # child apps

# # Simulations
# time make -j$(nproc) Physarum2D				# agent-based sim
# time make -j$(nproc) Physarum3D				# agent-based sim

# # Pathtracing
# time make -j$(nproc) Siren					# tile based async pathtracer

# # voxelspace variants
# time make -j$(nproc) VoxelSpace				# voxelspace algorithm renderer
# time make -j$(nproc) VoxelSpace_Erode		# visualizing the erosion update
# time make -j$(nproc) VoxelSpace_Physarum	# visualizing the physarum sim

# # softbody sim
# time make -j$(nproc) SoftBodies				# soft body sim on the CPU
# time make -j$(nproc) SoftBodiesGPU			# soft body sim on the GPU ( unfinished )

# # cellular automata variants
# time make -j$(nproc) CABitPlanes			# parallel cellular automata
# time make -j$(nproc) CAHistory				# cellular automata with history
# time make -j$(nproc) CAColorSplit			# same as above, but different color distribution
# time make -j$(nproc) GoL					# basic Game of Life

# # vertexture point sprite sphere impostors variants
# time make -j$(nproc) Vertexture				# point sprite sphere impostors
# time make -j$(nproc) VertextureClassic		# Vertexture, with the trees
# time make -j$(nproc) ProjectedFramebuffers	# projected framebuffers using the vertexture renderer

# this runs all the targets in parallel, scaled with the number of processors on the machine ( e.g. -j4, -j16, etc )
	# can use the above individual targets to enable / disable piecemeal
time make -j$(nproc) all

cd ..
