#ifndef INCLUDES
#define INCLUDES

//====== General STL Stuff ====================================================
#include <stdio.h>
#include <algorithm>
#include <bitset>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
// #include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
// #include <print>
#include <regex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// fftw3 include
#include "../utils/fftw-3.3.10/api/fftw3.h"

// iostream stuff
using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::flush;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::to_string;
constexpr char newline = '\n';

// pi definition - definitely sufficient precision
constexpr double pi = 3.14159265358979323846;
constexpr double tau = 2.0 * pi;

//====== OpenGL / SDL =========================================================
// GLM - vector math library GLM
#define GLM_FORCE_SWIZZLE
#define GLM_SWIZZLE_XYZW
#include "../utils/GLM/glm.hpp"						// general vector types
#include "../utils/GLM/gtc/matrix_transform.hpp"	// for glm::ortho
#include "../utils/GLM/gtc/type_ptr.hpp"			// to send matricies gpu-side
#include "../utils/GLM/gtx/rotate_vector.hpp"
#include "../utils/GLM/gtx/transform.hpp"
#include "../utils/GLM/gtx/string_cast.hpp"			// to_string for glm types ( cout << glm::to_string( val ) )
// #define GLX_GLEXT_PROTOTYPES 					// not sure as to the utility of this

// convenience defines for GLM
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::uvec2;
using glm::uvec3;
using glm::uvec4;
using glm::bvec2;
using glm::bvec3;
using glm::bvec4;
using glm::mat2;
using glm::mat3;
using glm::mat4;
using glm::normalize;

// OpenGL Function Loader
#define GLEW_STATIC // found this in the old Vertexture code... is this something useful?
#include <GL/glew.h>

// GUI library (dear ImGUI)
#include "../utils/ImGUI/TextEditor/TextEditor.h"
#include "../utils/ImGUI/imgui.h"
#include "../utils/ImGUI/imgui_impl_sdl2.h"
#include "../utils/ImGUI/imgui_impl_opengl3.h"

// SDL includes - windowing, gl context, system info
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

//==== Third Party Libraries ==================================================
// tracy profiler annotation
#include "../utils/tracy/public/tracy/Tracy.hpp"

// more general noise solution
#include "../utils/noise/FastNoise2/include/FastNoise/FastNoise.h"

// wrapper for TinyOBJLoader
#include "../utils/ModelLoading/TinyOBJLoader/tiny_obj_loader.h"

// Niels Lohmann - JSON for Modern C++
#include "../utils/Serialization/JSON/json.hpp"
using json = nlohmann::json;

// tinyXML2 XML parser
#include "../utils/Serialization/tinyXML2/tinyxml2.h"
using XMLDocument = tinyxml2::XMLDocument;

// autocomplete stuff
#include "../utils/autocomplete/DictionaryTrie.hpp"

// .PLY file format I/O
#include "../utils/happly/happly.h"

//==== My Stuff ===============================================================
// my fork of Alexander Sannikov's LegitProfiler
#include "../utils/ImGUI/LegitProfiler/ImGuiProfilerRenderer.h"

// managing bindings of textures to binding points
#include "./coreUtils/bindset.h"

// wrapper around std::random + wip other methods
#include "./coreUtils/random.h"

// some useful math functions
#include "./coreUtils/math.h"

// coloring of CLI output + palette access stuff
#include "../data/colors.h"

// image load/save/resize/access/manipulation wrapper
#include "./coreUtils/image2.h"

// simplified texture management
#include "./coreUtils/texture.h"

// simple std::chrono and OpenGL timer queries wrappers
#include "./coreUtils/timer.h"

// more polished input handling
#include "./coreUtils/inputHandler.h"

// orientation trident
#include "../utils/trident/trident.h"

// terminal
#include "./coreUtils/terminal.h"

// font rendering header
#include "../utils/fonts/fontRenderer/renderer.h"

// software rasterizer reimplementation
#include "../utils/SoftRast/SoftRast.h"

// tinyBVH software BVH build/traversal
// #include "../utils/tinybvh/tiny_bvh.h"

// shader compilation wrapper
#include "shaders/lib/shaderWrapper.h"

// bayer patterns 2, 4, 8 + four channel helper func
#include "../data/bayer.h"

// png-encoded palette list decoder
#include "../data/paletteLoader.h"

// png-encoded glyph list decoder
#include "../data/glyphLoader.h"

// wordlist decoders
#include "../data/wordlistLoader.h"

// templated diamond square heightmap generation
#include "../utils/noise/diamondSquare/diamond_square.h"

// particle based erosion
#include "../utils/erosion/particleBased.h"

// Brent Werness' Voxel Automata Terrain, adapted to C++
#include "../utils/noise/VAT/VAT.h"

// bringing the old perlin implementation back
#include "../utils/noise/perlin.h"

// config struct, tonemapping struct
#include "./dataStructs.h"

// management of window and GL context
#include "./coreUtils/window.h"

#endif