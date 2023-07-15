# jbDE - the jb Demo Engine

This is an evolution of the [NQADE](https://github.com/0xBAMA/not-quite-a-demo-engine) engine. One of the key points is for each project to inherit from a base engine class so that keeping projects up to date and working is a minimal load, makes my life a lot easier. I will be containing all the child projects as folders within this repository, and they will all inherit from the base engine class. General architecture is subject to change, but this is the current high-level plan.

# Setup
- Requires `libsdl2-dev`, `libglew-dev` on Ubuntu.
- Make sure to recurse submodules to pull in submodule code - `git submodule update --init --recursive`, alternatively just run scripts/init.sh to do the same thing.
- Run scripts/build.sh in order to build on Ubuntu. It just automates the building of the executables and moves it to the root directory, as files are referenced relative to that location, and then deletes the build folder (if called with the "clean" option).
- New in jbDE
	- All applications inherit from a base engine class, and are built as part of the build script. Dependencies are pretty general across the apps, so there's really just a separate main.cc defining the inherited class for each one, plus whatever additional textures/shaders/etc they require. CMake script will create make targets for each one, `scripts/build.sh` calls each one in sequence.
	- Current Project List:
		- EngineDemo - minimal functioning of the display pipeline, imgui demo window, etc.
		- Cellular Automata Experiments - Game of Life + some variants ( multiple parallel bitplanes, using bits for history ).
		- Physarum Sim - reimplementation of my old [physarum sim](https://jbaker.graphics/writings/physarum.html), but with everything moved to compute shaders.
		- SoftBodies - port of [Softbodies](https://github.com/0xBAMA/SoftBodies) softbody sim on simple car chassis. Also, WIP GPU Implementation.
		- Vertexture2 - reimplementation of [Vertexture](https://jbaker.graphics/writings/vertexture.html), making heavier use of the GPU, extending the point sprite sphere impostor thing to write correct depths
		- VoxelSpace - Reimplementation of the VoxelSpace algorithm in a compute shader. I had done this [previously](https://jbaker.graphics/writings/voxelspace.html), but this time using a custom framebuffer in order to do postprocessing etc on the color attachment.
		- Future Projects
			- Port of [Siren](https://github.com/0xBAMA/Siren)
			- Next version of [Voraldo](https://github.com/0xBAMA/Voraldo13), Voraldo14

## Features
**Graphics Stuff**
- [SDL2](https://wiki.libsdl.org/) is used for windowing and input handling
- [GLEW](https://glew.sourceforge.net/) as an OpenGL function loader
- OpenGL Debug callback for error/warning reporting
- [Dear ImGui Docking Branch](https://github.com/ocornut/imgui/tree/docking) 1.89.3 + [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit)
	- Setup to spawn platform windows
- [GLM](http://glm.g-truc.net/0.9.8/api/index.html) for vector and matrix types
- Fast text renderer overlay applied as a post process, currently used for reporting of main loop timing but I eventually want to extend it to a drop down in engine terminal a la [guake](http://guake-project.org/), renderer is loosely based on / inspired by [this implementation](https://jmickle66666666.github.io/blog/techart/2019/12/18/bitmap-font-renderer.html), but with the addition of an extended ASCII character set for textmode graphics + 8 bit per channel color, per character.
	- Hex dump tool built on top of the text renderer
- WIP software rasterizer, SoftRast
	- Currently serves as a model loader, wrapper around [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- Texture manager, which simplifies the declaration, binding, and usage of textures.
	- Vastly easier to manage the textures that I use for different things, after figuring out the OpenGL texture binding model a little bit better.
- OpenGL texture bindings management with bindsets - needs to be updated to use the texture manager.

**Noise**
- Diamond-Square algorithm heightmap generation
- [FastNoise2](https://github.com/Auburn/FastNoise2) - flexible, powerful, very fast noise generation
- 4 channel blue noise texture from [Christoph Peters](http://momentsingraphics.de/BlueNoise.html)

**Utilities**
- CMake parallel build setup
- Image2 wrapper for loading/saving/resizing/etc of images ( STB and [LodePNG](https://lodev.org/lodepng/) I/O backends )
	- Templated for underlying type ( generally assumes it is either uint8_t or float ) and number of channels ( technically up to 255 channels, but some functions make assumptions about channel count e.g. calculating luma )
	- Incorporates the [irFlip2](https://jbaker.graphics/writings/irFlip2.html#irflip2) image swizzling utility
	- Supports loading and saving 32-bit per channel floating point images with [TinyEXR](https://github.com/syoyo/tinyexr)
	- Several different models for applying lens distortion to images - multisample versions require the use of floating point images, due to some clipping issues.
- JSON parsing using [nlohmann's single header implementation](https://github.com/nlohmann/json)
- XML parsing using [TinyXML2](https://tinyxml2.docsforge.com/)
- TinyOBJLoader for loading of Wavefront .OBJ 3D model files
- [Brent Werness' Voxel Automata Terrain ( VAT )](https://bitbucket.org/BWerness/voxel-automata-terrain/src/master/), converted from processing to C++
	- BigInt library required by the VAT implementation ( to replace java/processing BigInt )
- Startup config in json format, setting application parameters at startup without requiring a recompile
- Tick() / Tock() timing wrapper around std::chrono
- Orientation Trident from Voraldo13, with the addition of multisampled AA and proper alpha blending in this implementation
- [Tracy](https://github.com/wolfpld/tracy) profiler integration ( client mode )
- fork of [Raikiri's LegitProfiler](https://github.com/Raikiri/LegitProfiler/tree/master) for in-engine CPU and GPU profiling + cool visualization
	- Scoped timers, which start timing in the constructor, and end in the destructor - using floating scopes, I can very easily time how long different options are taking.

**Data Resources**
- Bayer pattern header, size 2, 4, 8 patterns
- Header with some color utilities
	- Large number of color palettes, and some utilities for randomly picking + interpolated or nearest sampling of color sets
- PNG encoded/obfuscated data resources with integrated loader/decoder
	- [bitfontCore2](https://jbaker.graphics/writings/bitfontCore2.html) ( ~140k glyphs from [robhagemans](https://github.com/robhagemans/hoard-of-bitfonts) ) + loader
	- Collection of palettes from [lospec](https://lospec.com/) + matplotlib + others + loader
	- Bad word list - collected from a few different resources, english word blacklists
	- Color word list - mostly gathered from Wikipedia's tables of names of colors and dyes
