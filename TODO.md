--------------------------------------------------------------------------------------------------

## Engine Stuff

- jbDE - WIP - Replaces NQADE
	- Converting to Vulkan - a little further out
		- Hardware Raytracing Experimentation, not possible with OpenGL
	- Adding some kind of thing where projects inherit from a base engine class
		- This will be a new engine, jbDE, the jb Demo Engine - but much carried forward from NQADE
			- All projects will be kept in the same repo, build script generates separate executables
	- Need to work on figuring out how projects will inherit from the base engine class
	- Once I get to that point, I will be able to maintain the projects automagically
		- Vertexture
		- SoftBodies
		- Voraldo
		- Physarum
		- Sponza viewer?
		- Siren
		- SDFs - SDF validation tool for the DEC
- Texture wrapper? At least a function - something to make the declaration of textures cleaner
	- I can do something that just returns a GLuint, because that's how I'm using it now
- Image Wrapper Changes
	- Better Pixel Sorting, Add Thresholding Stuff
	- Rewrite, Templated Version
		- Specify both type and number of channels as template parameters
- Dithering stuff
	- Palette based version: Precompute 3d LUT texture(s) in order to avoid having to do the distance calculations
	- Probably make this built-in functionality in the engine
- Deterministic RNG for the CPU - need deterministic alternative to std::random
	- VAT, Spaceship Generator need repeatability in random number generation
	- I haven't found a way to seed std::random to be able to get the same sequence back out again
		- <https://www.phy.olemiss.edu/~kbeach/guide/2020/01/11/random/> is this something? looks promising
		- would still like a wrapper around the RNG stuff, so it's less to jack around with with all the std::random syntax
	- Start with Wang Hash - Other methods? Linear congruential, others? Is a mersene twister implementation practical?
	- Domain Remapping Functions, Point on disk, etc, utilities
	- Come up with a way to characterize the output, make sure we are getting "good" RNG
- [Poisson Disk Sampling](https://github.com/corporateshark/poisson-disk-generator)
	- Point locations based on [this paper](https://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf)
	- I'm sure there are interesting ways I can use this
		- Image based, but also want to be able to get list of points
- Color Header
	- Palette Options - I want to make this a lot more flexible
		- simple palette from the lospec set
		- interpolated lospec set
		- iq-style sinusoid palettes
		- simple gradients between two ( or more ) colors
		- heatmap gradient
		- color temperature sweep

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
	- Operations are executed based on label
	- Menu layout based on labels and ranges
- Spaceship Generator reimpl
	- Use of std::unordered_map is good, I like this
		- std::unordered_map< glm::ivec3, glm::vec4 >
			- ivec3 keys for easy negative indexing, vec4 contents for color and opacity
		- Potential for writing my own hashmap, but honestly I'm not seeing a huge need to do so
	- Using new glyph and palette resources
		- Abandoning the hoard-of-bitfonts fork
			- fontmess is the new one, with the simple pipeline for parsing etc
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

	- Blacklisting certain glyphs - hard to go through the full set, 140k is a lot to work with
		- Only include english characters, only include foreign characters, only include pictograms? something like that
		- Make sure we have unique IDs, and Black/Whitelist certain IDs
			- How do we maintain this if more glyphs get added? ( append only? probably best solution )
- Render mode control
	- Different cameras
		- Joukowsky transform <https://www.shadertoy.com/view/tsdyWM>
		- Fisheye <https://www.shadertoy.com/view/3sSfDz>
		- Different Fisheye <https://www.shadertoy.com/view/MlS3zK>
		- Panoramic projection <https://www.shadertoy.com/view/clB3Rz>
		- Icosahedral <https://www.shadertoy.com/view/4dSyWh>
		- Wrighter's Spherical <https://www.shadertoy.com/view/WlfyRs>
	- different traversal methods
		- <https://www.cse.chalmers.se/edu/year/2011/course/TDA361/grid.pdf>
- Stuff from [Voraldo13](https://github.com/0xBAMA/Voraldo13)
	- Different types of screenshot utilities ( lower priority )
		- Accumulator
		- After postprocessing
		- Directly from framebuffer
- Normal vectors derived from alpha channel gradient ( similar to SDF normal )

--------------------------------------------------------------------------------------------------

## Siren Plans

- Need to refit refraction, try this with explicit primitives
- Abstract camera projections - see above, "Different cameras"

--------------------------------------------------------------------------------------------------

## [Website](https://jbaker.graphics/)

### DEC plans

- Operators section
- 2D section

### [Articles](https://jbaker.graphics/writings)

#### Keeping up with projects as they happen, so they don't pile up again

- currently none are pending

--------------------------------------------------------------------------------------------------

## Concrete Future Software Project Ideas

- Reimplementation of the VIVS V8 Engine Animation as a Raymarching Demo ( likely on Shadertoy )
- Visible Human Data Resampling - MIP chain + 3d sampling
	- Generate mip chain from original data, allow for arbitrary resampling at arbitrary scales
- SoftBodies2
	- Support larger models - currently nowhere near the cap on the current single thread impl
		- Consider learning how to use mutexes to make the multithreaded version practical?
		- I think it will work well on the GPU
	- I want to change the way the tension / compression coloring is done
		- Instead of just hard cut red to blue, I want to fade, through white at neutral
	- Simple cloth sim
		- SDF intersection? Sphere comes in and affects the sim nodes
		- Noise based perturbation, like wind?
	- Higher order physics solver? I think that there's some potential value there
- Space Game thing? I think this is something that would be fun to work on

--------------------------------------------------------------------------------------------------

## More Vague Future Software Project Ideas

- More stuff with the Erosion thing, I think I have the 3D version worked out pretty well
	- that little acquarium simulator thing I wrote about in my notes
- RTIOW on the GPU
	- More raytracing stuff, the next week, the rest of your life books
	- <https://people.cs.kuleuven.be/~philip.dutre/GI/TotalCompendium.pdf>
	- Understanding Importance Sampling and More Materials
		- <https://github.com/HectorMF/BRDFGenerator>
		- <https://github.com/boksajak/brdf> / <https://boksajak.github.io/files/CrashCourseBRDF.pdf>
- Glitch effects using the bitfontCore2 bit masks
- Fluid Simulation
	- <https://github.com/InteractiveComputerGraphics/SPlisHSPlasH>
- 3D Version of Physarum Simulation
	- I think there's a lot more cool stuff I can do with the physarum thing
- More Advanced Optics Simulation
	- need to look at explicit primitives for lens shapes, SDF lens shapes are problematic
- Experimenting with TinyGLTF and loading of the Intel Sponza models
	- Got to try shadowmapping at some point
	- Normal mapping is cool as shit
- Point cloud visualization
	- Aerosoap mentioned using jump flood algorithm for visualization, look into that
		- <http://lidarview.com/>
		- <https://potree.github.io/> / <https://github.com/potree/potree>
	- Mesh to point cloud conversion, Poisson Disk Sampling on triangles to generate points
- Engine and Suspension Simulation
	- Voxels for center of mass calculation
	- Interactive driving thing?
	- Visualize the inside of the engine cylinder using backface culling, like that one thing I wanted to do with the house walls in CS 4250
		- only interior surface of walls will show, because the backface is facing the exterior side
			- I think it's a valid technique, I want find a way to try it on something
- IFS Stuff - Nameless wants to do some stuff with this, too - would be cool to collaborate
	- probabalistic picking of transforms, points in compute shaders
		- <https://flam3.com/flame_draves.pdf>
		- <https://www.cs.uaf.edu/~olawlor/papers/2011/ifs/lawlor_ifs_2011.pdf>
- Tree Growing Stuff
	- <https://courses.cs.duke.edu/cps124/spring08/assign/07_papers/p119-weber.pdf>
	- <http://algorithmicbotany.org/papers/selforg.sig2009.small.pdf>
- Bokeh LUT ( Pencil Map ) - how to apply this?
	- <https://www.siliconstudio.co.jp/rd/presentations/files/siggraph2015/06_MakingYourBokehFascinating_S2015_Kawase_EN.pdf>
- Tonemapping / Color Stuff
	- Path-to-White tonemapping curves
		- <https://github.com/tizian/tonemapper>
		- <https://chrisbrejon.com/articles/ocio-display-transforms-and-misconceptions/>
	- General Color
		- <http://poynton.ca/notes/colour_and_gamma/ColorFAQ.html>

--------------------------------------------------------------------------------------------------

## Makerspace Project Ideas

- Reverse Avalanche Oscillator PCB
	- Learn KiCAD in order to be able to make the PCB
		- [Getting to Blinky](https://www.youtube.com/watch?v=BVhWh3AsXQs&list=PLy2022BX6EspFAKBCgRuEuzapuz_4aJCn)
	- Circuit from [here](https://www.lookmumnocomputer.com/simplest-oscillator)
- Learning KiCAD opens up a lot of other stuff
	- Microcontroller stuff, PIC
	- ToF laser distance sensors
	- Accelerometers
- Learning some arduino to help Space with some of the art / sensor / actuator stuff
	- Touch sensitivity, how do we make that happen
- Getting into something with the Shapeoko
	- Reworking the old gcode generating code, maybe figure out better clearance algorithms?
		- Previous impl was very much brute force, given the capabilities of the 3018 machine
- Computer Cooler Shroud Mount for the 140mm Fans
	- Base the design on [the NA-HC4 Heatsink Cover](https://noctua.at/en/na-hc4-chromax-black)
		- Can we mount to one of these? something to adapt it, would be significantly less 3d printing work
	- Consider also doing [the undervolting thing](https://youtu.be/1pizvaYiVbk)
- Camera Stabilization Flywheel? ( maybe kinda goofy idea )
	- <https://www.bennetcaraher.com/project-sharpshooter.html>
	- <http://myresearch.company/flywheel.phtml>
	- <https://www.youtube.com/watch?v=cjV-yDNdeOI> using an old hard drive motor, interesting
- Projector + Short DoF Photography
	- <https://twitter.com/JoanieLemercier/status/1534226118877782022>
	- Laser sweep? maybe easier / cheaper than using a display projector

--------------------------------------------------------------------------------------------------
