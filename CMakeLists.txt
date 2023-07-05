cmake_minimum_required( VERSION 3.16 )

project( NQADE VERSION 1.2 LANGUAGES C CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED True )
set( CMAKE_CXX_FLAGS "-std=c++17 -lstdc++fs -O4" )
set( CMAKE_REQUIRED_FLAGS -lstdc++fs )

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# this makes SDL2 work
set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

find_package( SDL2 REQUIRED )
add_library( sdl2 INTERFACE )
target_include_directories( sdl2
	INTERFACE
		${SDL2_INCLUDE_DIR}
)
target_link_libraries( sdl2
	INTERFACE
		${SDL2_LIBRARY}
		${CMAKE_DL_LIBS}
)

# this makes OpenGL work
set( OpenGL_GL_PREFERENCE "GLVND" )
find_package( OpenGL REQUIRED )
add_library( opengl INTERFACE )
target_link_libraries( opengl INTERFACE OpenGL::GL )

# GL Extension Wrangler
find_package( GLEW REQUIRED )
include_directories( ${GLEW_INCLUDE_DIRS} )
link_libraries( ${GLEW_LIBRARIES} )

# FastNoise2
add_subdirectory( ${PROJECT_SOURCE_DIR}/src/utils/noise/FastNoise2 )

# Tracy Profiler
option( TRACY_ENABLE "" ON )
option( TRACY_ON_DEMAND "" ON )
add_subdirectory( src/utils/tracy )

# this is for ImGUI
add_library( imgui
	src/utils/ImGUI/imgui_impl_sdl2.cc
	src/utils/ImGUI/imgui_impl_opengl3.cc
	src/utils/ImGUI/imgui.cc
	src/utils/ImGUI/imgui_demo.cc
	src/utils/ImGUI/imgui_draw.cc
	src/utils/ImGUI/imgui_widgets.cc
	src/utils/ImGUI/imgui_tables.cc
	src/utils/ImGUI/TextEditor/TextEditor.cc
	src/utils/ImGUI/TextEditor/LanguageDefinitions.cc
)

target_link_libraries( imgui PUBLIC sdl2 opengl )

target_compile_options( imgui PUBLIC -I/usr/include/SDL2 )
target_compile_definitions( imgui PUBLIC -D IMGUI_IMPL_OPENGL_LOADER_GL3W -D_REENTRANT )

# this is the bigint library to support the VAT code
add_library( BigInt
	src/utils/noise/VAT/BigInt/BigUnsigned.cc
	src/utils/noise/VAT/BigInt/BigInteger.cc
	src/utils/noise/VAT/BigInt/BigIntegerAlgorithms.cc
	src/utils/noise/VAT/BigInt/BigUnsignedInABase.cc
	src/utils/noise/VAT/BigInt/BigIntegerUtils.cc
)
target_compile_options( BigInt PUBLIC -Wno-deprecated )

add_library( tinyXML2 src/utils/Serialization/tinyXML2/tinyxml2.cc )
add_library( tinyEXR src/utils/imageLibs/tinyEXR/tinyexr.cc src/utils/imageLibs/tinyEXR/miniz/miniz.c )
add_library( STB_ImageUtilsWrapper src/utils/imageLibs/stb/impl.cc )
add_library( TinyOBJLoader src/utils/ModelLoading/TinyOBJLoader/tiny_obj_loader.cc )
add_library( LodePNG src/utils/imageLibs/LodePNG/lodepng.cc )
add_library( Perlin src/utils/noise/perlin.cc )
# add_library( Tracy src/utils/tracy/public/TracyClient.cpp ) # not sure what state this is in right now, can't build the server application because of some freetype dependency

add_library( CompilerFlags INTERFACE )
target_compile_options( CompilerFlags INTERFACE -Wall -O3 -std=c++17 -lGL -lstdc++fs -lSDL2 -ldl -Wno-maybe-uninitialized -Wno-unused-function ) # suppresses warnings for Tracy

# this builds the final executable(s)
# probably some room for optimization here, there's a lot of redundant work happening in this last stage ( create library from engine cc files? )

# base engine functionality
file ( GLOB ENGINE_BASE
	src/engine/engine.cc
	src/engine/engineUtils.cc
	src/engine/engineInit.cc
	src/engine/engineImguiUtils.cc
)

# libraries which are linked
add_library( ENGINE_LIBS INTERFACE )
target_link_libraries(
	ENGINE_LIBS
	INTERFACE
	imgui
	BigInt
	tinyEXR
	tinyXML2
	opengl
	sdl2
	STB_ImageUtilsWrapper
	stdc++fs
	FastNoise
	Perlin
	Tracy::TracyClient
	TinyOBJLoader
	LodePNG
	CompilerFlags
)


# =================================================================================================
# base engine demo
# =================================================================================================
add_executable( engineDemo
	${ENGINE_BASE}
	src/projects/engineDemo/main.cc
)

target_link_libraries( engineDemo
	PUBLIC
	ENGINE_LIBS
)


# =================================================================================================
# Physarum
# =================================================================================================
add_executable( Physarum
	${ENGINE_BASE}
	src/projects/Physarum/main.cc
)

target_link_libraries( Physarum
	PUBLIC
	ENGINE_LIBS
)


# =================================================================================================
# SoftBodies
# =================================================================================================
add_executable( SoftBodies
	${ENGINE_BASE}
	src/projects/SoftBodies/main.cc
	src/projects/SoftBodies/model.cc
)

target_link_libraries( SoftBodies
	PUBLIC
	ENGINE_LIBS
)


# =================================================================================================
# =================================================================================================
# Point Sprite Experiments
# =================================================================================================
# =================================================================================================
# ProjectedFramebuffers
# =================================================================================================
add_executable( ProjectedFramebuffers
	${ENGINE_BASE}
	src/projects/PointSprite/ProjectedFramebuffers/main.cc
)

target_link_libraries( ProjectedFramebuffers
	PUBLIC
	ENGINE_LIBS
)

# =================================================================================================
# Vertexture
# =================================================================================================
add_executable( Vertexture
	${ENGINE_BASE}
	src/projects/PointSprite/Vertexture/main.cc
)

target_link_libraries( Vertexture
	PUBLIC
	ENGINE_LIBS
)

# =================================================================================================
# VertextureClassic
# =================================================================================================
add_executable( VertextureClassic
	${ENGINE_BASE}
	src/projects/PointSprite/VertextureClassic/main.cc
)

target_link_libraries( VertextureClassic
	PUBLIC
	ENGINE_LIBS
)