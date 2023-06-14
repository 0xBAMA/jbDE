--------------------------------------------------------------------------------------------------

This is a planning document for work on this project and in general, about the things I like to work on. Not particularly structured, trying to group information together and keep related links at hand. Varying levels of detail, from high level plans to implementation specifics.

--------------------------------------------------------------------------------------------------

## Engine Stuff

- jbDE - WIP - Replaces NQADE

- Top Priority: Getting up and running on the new machine(s)
	- [Another potential vector for getting up and running](https://askubuntu.com/questions/1451506/how-to-make-ubuntu-22-04-work-with-a-radeon-rx-7900-xtx) - will require another OS reinstall on that machine

	- Motivation
		- 4x floating point perf going from Radeon VII -> Radeon RX 7900XTX ( similar memory bandwidth, though, no HBM on the 7900XTX )
		- Finished OddJob and put Windows on it, working on set up with Visual Studio
			- Something I had not really considered, but this machine being the size of a shoebox, would be very good for being able to travel with
			- Need to figure out the [Gigabyte RGB Fusion](https://www.gigabyte.com/mb/rgb/sdk) software ( likely has to be done in C# )
			- I think this is the first actually relevant usage of the RGB LEDs I've thought of
			- Something like:
				- Yellow for running
				- Green for complete
				- Red for job failed
			- It may just make more sense to do it based on GPU/system load or GPU temp, otherwise I'd have to figure out some kind of signaling thing via a file in some location known to both programs

	- Work so far
		- ~~Suspicions point towards ImGui Docking branch, specifically the management of multiple contexts~~
		- ~~Switch to master, instead of docking branch? Is this a fix? Voraldo v1.1 will run, so I think maybe~~
			- Switching to master branch did not work, it is not the fault of the docking branch
			- I did not commit these changes to master, I like having the pop out windows
		- Voraldo-v1.1 and SoftBodies ( old version ) **do** work ( kind of, no block display on Voraldo, but SoftBodies works perfect with a couple paths modified )
			- Same two bizarre messages at startup for any of my OpenGL programs, but glewInit() does not fail on the older code:
				- `Xlib:  extension "AMDGPU" missing on display ":0".`
				- `Xlib:  extension "GLX_ARB_context_flush_control" missing on display ":0".`
				- Then glewInit() fails, program aborts
				- Nothing relevant found by googling these messages
				- Also interesting is the fact that glxinfo **does** show `GLX_ARB_context_flush_control` as an available extension
					- Regardless, it's not referenced anywhere in any of my code
			- This ImGUI code in these older projects is from back when you could use GLEW as a custom loader for ImGUI
				- This seems to have been since deprecated in favor of ImGUI's GL3W-based loader header ( which is clearly not properly contained )
			- There is some subtle interaction that I cannot determine the nature of, between GLEW and ImGUI's loader, which causes glewInit() to fail
				- This is the case, even when glewInit() is called before any ImGUI code at all - some strange compile-time behavior?

	- Potential Solutions:
		- Converting to Vulkan?
			- Maybe the move, we can get away from this OpenGL shitstorm
			- This is less work than it seems, it's just a lot of code to populate all the structs and shit
				- It can all be wrapped, as far as user-facing code, high level does not need to be difficult
				- You're doing the same things, it's just much more verbose
				- Really need to focus up and try to get through [vkGuide](https://vkguide.dev/) again at some point, see if I can follow along this time and lay the groundwork for this engine in Vulkan
				- Resource from Renderdoc - ["Vulkan in 30 Minutes"](https://renderdoc.org/vulkan-in-30-minutes.html)
				- [Vulkan all the way - Transitioning to modern low level graphics api in academia](https://www.sciencedirect.com/science/article/pii/S0097849323000249)
			- Hardware Raytracing Experimentation, not possible with OpenGL
		- More focus on cross-platform dev?
			- These same driver issues do not exist when I run the existing NQADE/Voraldo13 code under windows on the new machine
			- This may be the way to manage this, get jbDE running under Windows, in the general case, and then I can work on stuff on BlackSatan ( Ubuntu ) that will work on BlackSatan2 / Offline Rendering Machine ( Windows )
		- **BAD** Roll back to that version of ImGUI? **BAD**
			- Just take the ImGUI code from the old repo ( v1.76 WIP )
				- I hate this
				- No... but also... maybe
		- Other UI lib options?
			- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) - this looks very promising, similar interface to ImGUI
			- [libui](https://github.com/andlabs/libui) - interesting idea but looks like it isn't being maintained anymore
			- [RmlUi](https://github.com/mikke89/RmlUi) - interesting c++ based HTML generation thing, bring your own renderer, e.g. pass verts to the graphics API

--------------------------------------------------------------------------------------------------

- [Windows Screenshot Tool](https://getsharex.com/)

- General Engine Stuff
	- jbDE child applications
		- SDFs - SDF validation tool for the DEC
			- This needs a new name, SDFs is too generic / overloaded
			- SDF Validation Tool might be the new name, need to think on that one
		- PointDrop
		- Sponza viewer? tbd
		- redo boids [but with some kind of acceleration structure](https://observablehq.com/@rreusser/gpgpu-boids?collection=@rreusser/writeups)

	- One of the big things I want to focus on is being able to render high-quality animations offline
		- Maybe get into some Julius Horsthuis-style fractal animation stuff
		- Make the separate offline rendering machine earn its keep
		- Very specifically set timesteps
			- Exactly 60fps frames saved to disk, then compile video
			- Render time does not matter
				- Render with enough samples to get rid of the noise, then do the next frame
		- Parameter control and interpolation
			- Camera movement, etc, needs to be able to manage this
		- Some kind of JSON config for this? Some way to control application parameters by label
		- Some kind of timeline tool? Like what they do for demoscene stuff, sometimes, that could be a cool tool to try to write
			- Maybe part of the SDF validation tool

- Texture wrapper? At least a function - something to make the declaration of textures cleaner
	- I can do something that just returns a GLuint, because that's how I'm using it now

- More involved shader wrapper?
	- More than just compilation
	- Would be nice to have an abstraction layer over uniforms
		- std::unordered_map< string, value > kind of thing, but would need to think about how that would work ( unions? templating? )
		- Be able to set the uniform values as members of the shader stuff
		- Default arguments
		- Function to update
		- ".Use()" would call glUseProgram, pass current state of uniforms

- Image Wrapper Changes
	- Applications
		- Better Pixel Sorting, Add Thresholding Stuff
			- Need to do a little more research on how this is done, my previous impl is literally "sort rows or columns by color channel value or luma"
		- Add tonemapping to the image wrapper?
			- some new curves
				- https://alextardif.com/Tonemapping.html
				- https://github.com/h3r2tic/tony-mc-mapface - this one is supposed to be really good
		- Add dithering to the image wrapper
		- colorspace conversion + tonemapping code port to C++

- SoftRast
	- What is the plan for this?
	- What is the desired functionality for the wrapper?
		- I'm not looking to just write OpenGL again, really just a subset related to the actual rasterization of geometry, and even then pretty limited scope
		- I think I've also done most of what I want to do with it
			- I think I will probably do a more flexible OBJ voxelizer for Voraldo14, but beyond that I don't have much in the way of plans for it
	- Maintainence of my own buffers, this is something I want to use for plant simulation ( think, write nearest plant ID value per pixel, use this to track how much energy is recieved for that plant ID summed across all pixels )

- Dithering stuff
	- Palette based version: Precompute 3d LUT texture(s) in order to avoid having to do the distance calculations
		- This will absolutely help perf on large color count palettes
			- Instead of iterating through and doing N distance checks for every pixel, every frame, just keep LUTs of closest, second closest color for all points in a 3D LDR RGB colorspace, and use dither patterns as before
	- Probably make this built-in functionality in the engine, part of postprocessing stack?

- [Properly Seeding the Mersenne Twistor](https://www.phy.olemiss.edu/~kbeach/guide/2020/01/11/random/)
	- VAT, Spaceship Generator need repeatability in random number generation
	- Wang Hash - Other methods? Linear congruential, others? Is a mersene twister implementation practical?
	- Domain Remapping Functions, Point on disk, etc, utilities
		- [Bias and Gain Functions](https://arxiv.org/abs/2010.09714)
		- [iq's Usful Functions](https://iquilezles.org/articles/functions/)
		- [Easing Functions](https://easings.net/)
	- Come up with a way to characterize the output, make sure we are getting "good" RNG
		- do some histograms with the output, to measure

- [Poisson Disk Sampling](https://github.com/corporateshark/poisson-disk-generator)
	- Point locations based on [this paper](https://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf)
	- I'm sure there are interesting ways I can use this
		- Image based, but also want to be able to get list of points
	- [Interesting note from Alan Wolfe, the blue noise guy](https://twitter.com/Atrix256/status/1546211637656309761) - you can create something very much like these points by just thresholding a 2d blue noise mask - raising or lowering thresholds create different densities, I found the same thing doing some dithering down to 1 bit per channel, where the image data and the brightness of the pixels was basically what was modulating value to create different point densities

--------------------------------------------------------------------------------------------------

## Plans for Voraldo14

### Starting on this as one of the first jbDE projects

- Operation Scripting and Logging - Needs a ground-up redesign
	- Logging needs seeding for RNG resources so scripting operations can be deterministic
	- Scripting needs to be able to control everything
		- Rendering parameters
			- Camera type, position, orientation
		- Drawing operations
		- Lighting operations

- Is it practical to try to do fully-jsonified parameters, menus, etc?
	- Attempted and then gave up for Voraldo13
		- I still think it's possible, I just have to make a better plan
	- Operations are executed based on label
	- Menu layout and interface elements based on labels, types and ranges
	- What about format specifiers, indenting etc? Just treat those as a special type?
		- I like the use of JSON for these kinds of things, because it doesn't require recompilation, it can be relaunching or even just reparsing the file while still running

- Spaceship Generator reimpl - Could this maybe belong in jbDE codebase? Potential for reuse outside of Voraldo
	- Use of std::unordered_map is good, I like this
		- std::unordered_map< glm::ivec3, glm::vec4 >
			- ivec3 keys for easy negative indexing, vec4 contents for color and opacity
		- Potential for writing my own hashmap, but honestly I'm not seeing a huge need to do so right now
	- Using new glyph and palette resources
		- Abandoning the hoard-of-bitfonts fork
			- fontmess is the new one, with the simple pipeline for parsing etc
			- I don't know if it's really possible to put this all into one file
				- There was some manual processing that I had to do, I think, I don't remember
	- Palette Options from color header
	- Are there any more operations I can add?
		- Some operations require the creation of a new map, because moving the contents in place will corrupt the data.
		- Currently:

		| **Operation** | **Requires New Map** | **Description** |
		|---|---|---|
		| Get Bounding Box | No | Walk the entire map, find minimum and maximum occupied voxels on each axis |
		| Stamp Random Glyphs | No | Pick random glyphs and put them into the map at random location |
		| Square Model | Yes | Get the bounding box, then create a new map, translating the old map to have only positive contents |
		| Flip | Yes | Invert orientation on some axis |
		| Shave | Yes | Get the bounding box, then remove all entries that are greater/less than some threshold on a random axis |
		| Mirror | Yes | Copy contents across some axis - bounding box value for that axis minus the coordinate |

		- More ideas
			- Array modifier? stamp the same glyph several times in constant size steps along some axis
				- Rather than mirroring, just copy several times with the same orientation
				- trim to very thin, beforehand?
			- Maybe an input to enable / disable each operation individually? I think that makes sense
			- Seed value, repeatable RNG is important for this

	- Related, doing something with sets of AABBs
		- My interpretation of Vanhoutte's [Invisible Citites](https://twitter.com/wblut/status/1632101200458809346) building shapes
			- Blocky shells
				- Generate several overlapping AABBs
				- "Fill" all of them
				- Shrink 1 on each axis ( positive and negative )
				- Cut these smaller ones out from the filled ones, to create a blobby/blocky shell
			- AABBs, but then make them into a frame
				- Draw original box
				- Tube shape
					- Cut out an AABB which is shrunk on two picked axes ( 1 unit on the positive and negative sides on each dimension )
				- Frame shape
					- Cut out 3 AABBs, same as tube, but do this for all three axes ( shrink on x,y, shrink on y,z, shrink on x,z )
	

	- Blacklisting certain glyphs - hard to go through the full set, 140k is a lot to work with
		- Only include english characters, only include foreign characters, only include pictograms? something like that
		- Make sure we have unique IDs, and Black/Whitelist certain IDs
			- How do we maintain this if more glyphs get added? ( append only? probably best solution )
		- Is there some way to do some kind of ML sorting on these?

- Render mode control
	- Different cameras - add another header in the style of the colorspace conversions header - integer picker and switch kind of deal - UV -> view vector
		- [Joukowsky transform](https://www.shadertoy.com/view/tsdyWM)
		- [Fisheye](https://www.shadertoy.com/view/3sSfDz)
		- [Different Fisheye](https://www.shadertoy.com/view/MlS3zK)
		- [Panoramic projection](https://www.shadertoy.com/view/clB3Rz)
		- [Icosahedral](https://www.shadertoy.com/view/4dSyWh)
		- [Wrighter's Spherical](https://www.shadertoy.com/view/WlfyRs)
		- [Several from ollj](https://www.shadertoy.com/view/ls2cDt)
		- [Relativistic](https://www.shadertoy.com/view/XtSyWW)
		- [Realistic Camera ( PBRT )](https://pbr-book.org/3ed-2018/Camera_Models/Realistic_Cameras)
		- [uv distort from iq](https://www.shadertoy.com/view/Xsf3Rn)
		- [Another One](https://www.shadertoy.com/view/wlVSz3)
		- Some stuff like barrel distortion to just mess with the existing camera
			- [Lens distortion + chrom abb tutorial](https://blog.yarsalabs.com/lens-distortion-and-chromatic-aberration-unity-part1/)
	- different traversal methods
		- [Amanatides / Woo A Fast Voxel Traversal Algorithm](https://www.cse.chalmers.se/edu/year/2011/course/TDA361/grid.pdf)
		- [Cyril Crassin paper](https://jcgt.org/published/0007/03/04/)
		- Some kind of DDA traversal
		- Supercover

- Stuff from [Voraldo13](https://github.com/0xBAMA/Voraldo13)
	- Different types of screenshot utilities ( lower priority )
		- High bit depth capture of accumulator as EXR format now that I have that utility in the engine
			- Allows for a couple things:
				- Resuming long render process
				- External postprocessing
		- After postprocessing
		- Directly from framebuffer

- Normal vectors derived from alpha channel gradient ( similar to SDF normal )

- Tiled invocation structure of the block - this is important, especially for the laptop - it hits some driver limitation for e.g. directional lighting op
	- Job system, to decouple from the framerate? not sure if I want to show in a partial state...
	- Would help to keep from overwhelming GPU with large job dispatch, this has been an issue before
	- No block swap after each tile
	- Configurable delay between tile invocations
		- Some kind of rudimentary job system for this? Operations in sequence, each job has a list of volume tiles + sizes + operation parameters
		- How do copy loadbuffer ops interact with this? do we need special handling for those?

--------------------------------------------------------------------------------------------------

## Siren Plans

- Need to refit refraction, try this with explicit primitives
	- I think this is a good direction to pursue, [iq's raytracing booleans](https://www.shadertoy.com/view/mlfGRM)
	- Some [stuff on IoR of physical materials](https://pixelandpoly.com/ior.html)
	- [Another resource](https://physicallybased.info/) with some of the same kind of stuff
	- [Sampling Microfacet BRDF](https://agraphicsguynotes.com/posts/sample_microfacet_brdf/)
	- [Spectral Refraction](https://www.shadertoy.com/view/llVSDz)
- Abstract camera projections, more artsy stuff - see above, "Different cameras"

--------------------------------------------------------------------------------------------------

## [Website](https://jbaker.graphics/)

### DEC plans

- Operators section
- 2D section

### [Articles](https://jbaker.graphics/writings)

- follow up on the lighting on vertexture
	- probably wait till I have the deferred stuff in place

#### Keeping up with projects as they happen, so they don't pile up again

- Web Design stuff
	- [Prevent Default](https://event.preventdefault.net/)

--------------------------------------------------------------------------------------------------

## Concrete Future Software Project Ideas

- Visible Human Data Resampling - create MIP chain + implement C++ 3d sampling
	- Generate mip chain from original data, allow for arbitrary resampling at arbitrary scales
		- Learn about different 3D sampling methodologies
		- demofox [sample code on bicubic sampling](https://www.shadertoy.com/view/MllSzX)
		- Key thing is quality of sampling, this is an offline process, so it's less of a concern for it to be quick
	- Huge dataset, will be an interesting problem to solve

- SoftBodies2
	- Support larger models, easy swapping between models - currently nowhere near the cap on the current single thread impl
		- Consider learning how to use mutexes to make the multithreaded version practical?
		- I think it will work well on the GPU
			- Structure is designed for parallelism
	- I want to change the way the tension / compression coloring is done
		- Instead of just hard cut red to blue, I want to fade, through white at neutral
	- Simple cloth sim
		- SDF intersection? Sphere comes in and affects the sim nodes
		- Noise based perturbation, like wind?
	- Higher order physics solver? I think that there's some potential value there
		- How does vertlet integration differ from what I'm doing? See Pezza video, study up on that a bit

- Glitch thing, fucking up an image with bitfontCore2-based glyph masks
	- Incorporate into the Image wrapper? ( probably yes )
	- Previous ideas ( from first version of Siren ):
		- Pull image data to CPU or do in a shader and write back to the accumulator texture
			- Masking for application of the following effects
			- Add to color channels or masking by glyph stamps ( randomly additive or subtractive )
				- Change the included utility from hoard-of-bitfonts to bitfontCore, for the compressed format of the bitfont glyphs
			- Blurs
			- Clears ( every other row / column / checkerboard )
			- Dither inside tile / mask / both
			- Reset averaging ( sample count )
			- Use std::vector + shuffle to on an array of floats cast to a byte array - make a mess of it, but check for NaN's in the data before re-sending to keep from having issues from that - set these values to either 1 or 0, based on some RNG

--------------------------------------------------------------------------------------------------

## More Vague Future Software Project Ideas

- Little JS demo based on [this](https://www.youtube.com/watch?v=XGioNBHrFU4) ( particle text )? would be cool to have something with interaction

- [Impostor shader](https://www.landontownsend.com/single-post/super-imposter-shader-my-journey-to-make-a-high-quality-imposter-shader)
	- [Impostors for trees](https://shaderbits.com/blog/octahedral-impostors) / [Video](https://youtu.be/4Ghulpp6CPw)

- Space Game thing? I think this is something that would be fun to work on
	- [Planet Geo](https://www.researchgate.net/publication/3422533_Climate_Modeling_with_Spherical_Geodesic_Grids)
	- [Kali's Star Nest](https://www.shadertoy.com/view/slccDX) / [With A Bit Of Explanation](https://www.shadertoy.com/view/wlVSWV)

- More stuff with the Erosion thing, I think I have the 3D version worked out pretty well
	- Interesting variant of the erosion sim, much faster graph-based alternative to my slow particle-based impl
		- [Video](https://twitter.com/Th3HolyMoose/status/1627073949606748166) / [Paper](https://hal.inria.fr/hal-01262376/document)
	- That little acquarium simulator thing I wrote about in my notes
	- The Nick Mcdonald [wind erosion sim](https://nickmcd.me/2020/11/23/particle-based-wind-erosion/)
	- [Tangram heightmapper](https://tangrams.github.io/heightmapper/)
	- [Wrighter's Cyclic Noise](https://www.shadertoy.com/view/3tcyD7)

- Plants
	- Tree Growing Stuff
		- [Creation and Rendering of Realistic Trees](https://courses.cs.duke.edu/cps124/spring08/assign/07_papers/p119-weber.pdf)
		- [Self Organizing Tree Models for Image Synthesis](http://algorithmicbotany.org/papers/selforg.sig2009.small.pdf)
		- [Synthetic Silviculture: Multi-scale Modeling of Plant Ecosystems](https://dl.acm.org/doi/10.1145/3306346.3323039)
	- Grass
		- [Ghost of Tsushima GDC talk](https://youtu.be/Ibe1JBF5i5Y)
		- [Realtime Realistic Rendering and Lighting of Forests](https://hal.inria.fr/hal-00650120/file/article.pdf)
		- [Outerra Procedural Grass Rendering](https://outerra.blogspot.com/2012/05/procedural-grass-rendering.html)

- RTIOW on the GPU
	- More raytracing stuff, the next week, the rest of your life books
	- [Dutre Global Illumination Compendium](https://people.cs.kuleuven.be/~philip.dutre/GI/TotalCompendium.pdf)
	- Understanding Importance Sampling and More Materials
		- [BRDF Generator](https://github.com/HectorMF/BRDFGenerator)
		- [Crash Course BRDF](https://github.com/boksajak/brdf) / [Repo](https://boksajak.github.io/files/CrashCourseBRDF.pdf)

- [Grid-based SDFs](https://kosmonautblog.wordpress.com/2017/05/01/signed-distance-field-rendering-journey-pt-1/)

- Depth Rendering to generate heightmaps
	- Something like the face charms project, again, but generating the heightmaps from meshes directly
	- Maybe this is an application for SoftRast
		- I want to template the Image wrapper before I do that, though
	- Fogleman's [Heightmap Meshing Utility](https://github.com/fogleman/hmm)
	- Raymarching heightmaps / terrain
		- [ollj](https://www.shadertoy.com/view/Mtc3WX)
		- [sjz1](https://www.shadertoy.com/view/clG3DD) - terrain + quick cosine function

- Fluid/Physics Simulation
	- [SPlisHSPlasH](https://github.com/InteractiveComputerGraphics/SPlisHSPlasH)
	- Pezza tutorials [1](https://youtu.be/lS_qeBy3aQI) / [2](https://youtu.be/9IULfQH7E90)

- 3D Version of Physarum Simulation
	- I think there's a lot more cool stuff I can do with the physarum thing
		- Even just using the 2d version has a lot of potential
			- Treat it as a heightmap
			- Capture at certain states ( EXR saving for full bit depth? What about agent orientation + velocity? or just reinit with random positions and directions? )

- Experimenting with TinyGLTF and loading of the Intel Sponza models
	- Got to try shadowmapping at some point
		- [Imperfect Shadowmaps](https://www.researchgate.net/publication/47862111_Imperfect_Shadow_Maps_for_Efficient_Computation_of_Indirect_Illumination) - interesting idea
	- Normal mapping is cool as shit
		- [Normal Mapping Without Precomputed Tangents](http://www.thetenthplanet.de/archives/1180)

- Point cloud visualization
	- Aerosoap mentioned using jump flood algorithm for visualization, look into that
		- [Online LIDAR Point Cloud Viewer](http://lidarview.com/)
		- [Potree](https://potree.github.io/) / [Repo](https://github.com/potree/potree)
	- Mesh to point cloud conversion, Poisson Disk Sampling on triangles to generate points
	- Find more LIDAR scans data to look at
		- [Some on Sketchfab](https://sketchfab.com/3d-models/sy-carola-point-cloud-17bd8188447b48baab75125b9ad20788)
		- I thing the poisson disk sampling thing would be an interesting way to generate them from meshes
			- Generate evenly spaced sample points on the triangles, get texture, normal, etc samples
	- [Surface Splatting](https://www.shadertoy.com/view/XllGRl)
	- [Software Rasterization of 2 Billion Points in Real Time](https://arxiv.org/abs/2204.01287)

- Something with GPU hashmaps
	- https://nosferalatu.com/SimpleGPUHashTable.html

- Something with Cellular Automata
	- 3D CA is super cool, I would like to do more with that one project where I was doing the the realtime wireWorld sim
	- Maybe some explorations along the lines of the Wolfram 8-bit totalistic thing
		- How many bits to represent 2D equivalent? 3D?
		- Exploring the space, generate noise field for randomly picked rules and then you can watch it kind of thing
	- [Forest Fire Model](https://en.wikipedia.org/wiki/Forest-fire_model)

- Data visualization for its own sake
	- [Byte-level file format details by corkami](https://github.com/corkami/pics/blob/master/binary/README.md#images)
		- Cyberpunk 2077 has a lot of little decorations that seem to be styled after some human readable format of executables
		- Generate images that just use data in interesting and different ways
	- [Christopher Domas Dynamic Binary Visualization](https://www.youtube.com/watch?v=4bM3Gut1hIk)
		- This tool he wrote, [CantorDust](https://inside.battelle.org/blog-details/battelle-publishes-open-source-binary-visualization-tool) / [Repo](https://github.com/Battelle/cantordust)
		- Another talk by [Christopher Domas](https://youtu.be/HlUe0TUHOIc)
		- I really like his [Hilbert curve visualization of unstructured data from here](https://corte.si/posts/visualisation/entropy/index.html) and [here](https://corte.si/posts/visualisation/binvis/)
			- [Lots of other cool articles on that site](https://corte.si/posts/code/hilbert/portrait/index.html)
	- Inspiration from [hornet on shadertoy, "glitch: cyberpunk text"](https://www.shadertoy.com/view/3lKSz3)
	- Aras-P [Float Compression](https://aras-p.info/blog/2023/01/29/Float-Compression-0-Intro/)
	- Palette analysis tools [1](https://github.com/Quickmarble/censor) / [2 (the one on lospec)](https://pixeljoint.com/forum/forum_posts.asp?TID=26080)

- Engine and Suspension Simulation
	- Voxels for center of mass calculation
	- Interactive driving thing?
	- Visualize the inside of the engine cylinder using backface culling, like that one thing I wanted to do with the house walls in CS 4250
		- Only interior surface of walls will show, because the backface is facing the exterior side
			- I think it's a valid technique, I want find a way to try it on something
	- Reimplementation of the VIVS V8 Engine Animation as a Raymarching Demo ( on Shadertoy? )
		- Create decent looking crankshaft, connecting rods, pistons, valves, etc with 
			- Good practice with SDF BVH, so I can do more complex geo with soft intersections etc to make them look nice
			- Blockout view shows only the bounding volumes

- IFS Stuff - Nameless wants to do some stuff with this, too - would be cool to collaborate
	- Probabalistic picking of transforms, points in compute shaders
		- [Draves Paper](https://flam3.com/flame_draves.pdf)
		- [Lawlor Paper](https://www.cs.uaf.edu/~olawlor/papers/2011/ifs/lawlor_ifs_2011.pdf)
	- [Fong "Analytical methods for Squaring the Disc"](https://arxiv.org/ftp/arxiv/papers/1509/1509.06344.pdf) - circle<->square mappings

- Bokeh Stuff
	- Bokeh LUT ( Pencil Map ) - how to apply this? [Pencil Map Paper](https://www.siliconstudio.co.jp/rd/presentations/files/siggraph2015/06_MakingYourBokehFascinating_S2015_Kawase_EN.pdf)
	- [Hexagonal Bokeh Blur](https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited/)

- [Deferred Rendering Decals](https://mtnphil.wordpress.com/2014/05/24/decals-deferred-rendering/)

- IQ's Texture Repetition stuff - [article](https://iquilezles.org/articles/texturerepetition/) / [shadertoy](https://www.shadertoy.com/view/4tsGzf)
	- [Burley paper](https://www.jcgt.org/published/0008/04/02/paper.pdf)

- Outlines like in [Mars First Logistics](https://twitter.com/ianmaclarty/status/1499494878908403712)

- Jump flood algorithm
	- [Quest for very wide outlines](https://bgolus.medium.com/the-quest-for-very-wide-outlines-ba82ed442cd9)
	- [Jump flood alg for rendering point clouds](https://www.lcg.ufrj.br/thesis/renato-farias-MSc.pdf)

- Applicaiton for ECS? [a good looking C/C++ ECS library](https://github.com/SanderMertens/flecs)

- [Gaussian blur kernel computation](https://lisyarus.github.io/blog/graphics/2023/02/24/blur-coefficients-generator.html)

- [2d Patterns](https://www.shadertoy.com/view/lt3fDS)

- [Golden Ratio Sunflower](https://www.shadertoy.com/view/ttSyz1)

- Tonemapping / Color Stuff
	- Path-to-White tonemapping curves
		- [Tizian Tonemapper](https://github.com/tizian/tonemapper)
		- [Notorious Six Writeup](https://chrisbrejon.com/articles/ocio-display-transforms-and-misconceptions/)
		- [Kajiya Tonemapper](https://github.com/EmbarkStudios/kajiya/blob/79b7a74d2a6194029781ad17c809fd3ccc16b0e4/assets/shaders/inc/tonemap.hlsl), [also](https://github.com/EmbarkStudios/kajiya/commit/cb10c9ff9e894f0f0bf7cc8c9c540f0fc09ed371#diff-6c20208767044bc82bec72ba08288f6cb3cce3ec5d99d903172e6da054ed72b4)
		- [generalized tonemapper](https://www.shadertoy.com/view/XljBRK)
		- [AgX](https://www.shadertoy.com/view/dtSGD1)
	- General Color
		- [Color FAQ](http://poynton.ca/notes/colour_and_gamma/ColorFAQ.html)
		- [Another potential tonemapping setup](https://www.shadertoy.com/view/7dsczS)
		- [ollj "general rainbow scatter"](https://www.shadertoy.com/view/4ldcWN)

- Random Shadertoy Inspiration
	- cool pixel effect [Antipod - MAGFest #08 by Flopine](https://www.shadertoy.com/view/dtS3WG)

--------------------------------------------------------------------------------------------------


## Makerspace Project Ideas

- Reverse Avalanche Oscillator PCB
	- Learn KiCAD in order to be able to make the PCB
		- [Getting to Blinky](https://www.youtube.com/watch?v=BVhWh3AsXQs&list=PLy2022BX6EspFAKBCgRuEuzapuz_4aJCn)
		- [OSHPark for PCB manufacture](https://oshpark.com/)
	- Circuit from [here](https://www.lookmumnocomputer.com/simplest-oscillator)

- Learning KiCAD opens up a lot of other stuff
	- Microcontroller stuff, PIC
	- ToF laser distance sensors
	- Accelerometers

- Learning some arduino to help Space with some of the art / sensor / actuator stuff
	- Touch sensitivity, how do we make that happen
	- I have some ESP32's, find something to do with those, maybe something with some of those little OLED screens

- Getting into something with the Shapeoko
	- Reworking the old gcode generating code, maybe figure out better clearance algorithms?
		- Previous impl was very much brute force, given the limited cutting capabilities of the 3018 machine

- Computer Cooler Shroud Mount for the 140mm Fans
	- Base the design on [the NA-HC4 Heatsink Cover](https://noctua.at/en/na-hc4-chromax-black)
		- Can we mount to one of these? I have one to get measurements from
			- I could make something to adapt it, I think it would be significantly less 3d printing work vs printing something that is the whole shroud + mount
			- Failing that, could make something based on measurements taken from the unit
	- Consider also doing [the undervolting thing](https://youtu.be/1pizvaYiVbk) on the R9 7950X in BlackSatan2
		- Got to dig out the overclocking header connector thingy to enable these controls
		- I would really prefer not running the CPU at 95C under load, it just makes me uncomfortable
		- Basically what I read is that looking at something like a 10mv undervolt loses very little perf and improves thermals a significant amount ( 95C -> 80C under load kind of improvement ) but I don't really understand the particulars of what this is doing

- Computer Lower Power Supply Shroud
	- Existing shroud no longer fits with Red Devil card - too long, would collide with shroud
		- Take measurements from existing shroud, design something that would serve a similar purpose and then finish it nice

- Camera Stabilization Flywheel? ( maybe kinda goofy idea, low, low priority )
	- [Project Sharpshooter](https://www.bennetcaraher.com/project-sharpshooter.html) - Reaction flywheels for DSLR
	- [For RC Helicopter](http://myresearch.company/flywheel.phtml)
	- [Video](https://www.youtube.com/watch?v=cjV-yDNdeOI) - stabilization using an old hard drive motor, interesting

- [Photometric Stereo Rig](https://www.artstation.com/artwork/WmKeO3)

- Projector + Short DoF Photography
	- <https://twitter.com/JoanieLemercier/status/1534226118877782022>
	- Laser sweep / spinning mirror thing? maybe easier / cheaper than using a DLP etc type display projector

--------------------------------------------------------------------------------------------------

## Courses

- [Keenan Crane's Discrete Differential Geometry](https://youtube.com/playlist?list=PL9_jI1bdZmz0hIrNCMQW1YmZysAiIYSSS)
- [Keenan Crane's Intro to Computer Graphics](https://youtube.com/playlist?list=PL9_jI1bdZmz2emSh0UQ5iOdT2xRHFHL7E)
- [Cem Yuskel's Intro to Computer Graphics](https://youtube.com/playlist?list=PLplnkTzzqsZTfYh4UbhLGpI5kGd5oW_Hh)
- [Cem Yuskel's Interactive Computer Graphics](https://youtube.com/playlist?list=PLplnkTzzqsZS3R5DjmCQsqupu43oS9CFN)
- [UC Davis Computer Graphics](https://youtube.com/playlist?list=PL_w_qWAQZtAZhtzPI5pkAtcUVgmzdAP8g)
- [TU Wien Intro to Computer Graphics](https://www.youtube.com/playlist?list=PLmIqTlJ6KsE3CLa2Kcffo4_EJeHCYj-AH)
- [TU Wien Vulkan](https://www.youtube.com/playlist?list=PLmIqTlJ6KsE0UsR2E_84-twxX6G7ynZNq)
- [TU Wien Algorithms for Real-Time Rendering Course](https://www.youtube.com/playlist?list=PLmIqTlJ6KsE1-jb7DF313E6MvLcpTLqgH)
- [TU Wien Rendering](https://www.youtube.com/playlist?list=PLmIqTlJ6KsE3e8SQowQ-DjD1eZkBA_Xb9)

--------------------------------------------------------------------------------------------------

## Matlab / Octave

- Want to get into some more stuff with this, it's a really good tool and I want to get more practiced with it

- Some things I want to try:
	- Image manipulation
		- That NTSC thing, I want to understand how that works
			- Encoding image as a 1D signal, adding noise, then reinterpreting it
			- [LMP88959's all-integer C impl](https://github.com/LMP88959/NTSC-CRT)
			- [LMP88959's PAL version impl](https://github.com/LMP88959/PAL-CRT)
	- Convolution
	- DSP stuff, more audio stuff
		- Learn more about filters - [Hack Audio](https://www.youtube.com/watch?v=epXM6Jc2Lj0) has some good, simple tutorials
		- Interpolation of signals - [Cool Paper](http://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf)
	- Maybe this is a good way to get into the NTSC simulation stuff
	- C++ interop
		- Would be interesting to do something with the raytracing hardware like this, maybe
			- Octave calls into C++
			- C++ creates Vulkan context, sets up buffers, etc, returns another buffer with something from the GPU

--------------------------------------------------------------------------------------------------