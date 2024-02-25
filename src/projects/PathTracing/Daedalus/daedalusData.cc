#include "daedalus.h"

void Daedalus::ResetAccumulators() {
	ZoneScoped;
	textureManager.ZeroTexture2D( "Color Accumulator" );
	textureManager.ZeroTexture2D( "Depth/Normals Accumulator" );
	textureManager.ZeroTexture2D( "Tonemapped" );
}

void Daedalus::ResizeAccumulators( uint32_t x, uint32_t y ) {
	ZoneScoped;
	// destroy the existing textures
	textureManager.Remove( "Depth/Normals Accumulator" );
	textureManager.Remove( "Color Accumulator Scratch" );
	textureManager.Remove( "Color Accumulator" );
	textureManager.Remove( "Tonemapped" );

	// create the new ones
	textureOptions_t opts;
	opts.dataType		= GL_RGBA32F;
	opts.width			= x;
	opts.height			= y;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	textureManager.Add( "Depth/Normals Accumulator", opts );
	textureManager.Add( "Color Accumulator Scratch", opts );
	textureManager.Add( "Color Accumulator", opts );
	textureManager.Add( "Tonemapped", opts );

	// regen the tile list, reset timer, etc
	daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize, x, y );
}