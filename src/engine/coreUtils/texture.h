#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include "../includes.h"

//===== Texture Options ===============================================================================================
struct textureOptions_t {

	// simplify this so that it's just handling the case of 2d textures - build from there

	// this will need to be simplified - just straight fill it out to match glTexImage2D()

		// need to also include filtering
		// need to also include wrap modes
		// ...

};


//===== Texture Record ================================================================================================
struct texture_t {

	// keep the information used to create the texture
	textureOptions_t creationOptions;

	GLuint textureHandle;
	GLenum textureUnit; // GL_TEXTURE0 + N
	size_t textureSize;

	string label;

};

//===== Texture Manager ===============================================================================================
class textureManager_t {
public:
	void Init () {

		// add config toggle for this eventually

		// report some platform detials:
			// number of available texture units
			// maximum dimensions of texture
			// ...

		// GLint val;
		// glGetIntegerv( GL_MAX_TEXTURE_SIZE, &val );
		// cout << endl << endl << "\t\tMax Texture Size Reports: " << val << endl;

		// glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &val );
		// cout << "\t\tMax 3D Texture Size Reports: " << val << endl;

		// glGetIntegerv( GL_MAX_DRAW_BUFFERS, &val );
		// cout << "\t\tMax Draw Buffers: " << val << endl;

		// glGetIntegerv( GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &val );
		// cout << "\t\tMax Compute Texture Image Units Reports: " << val << endl;

		// glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &val );
		// cout << "\t\tMax Combined Texture Image Units: " << val << endl << endl;

		// // make up for the spacing issues
		// cout << "\t............................................................. ";

	}

	int count = 0;
	void Add ( string label, textureOptions_t &texOptsIn ) {

		// increment count on add, so we can use this for glActiveTexture( GL_TEXTURE0 + count )
		count++;

	}

	// todo: update the below ( Get/GetUnit ) from the unordered map version to the vector version

	GLuint Get ( string label ) { // if the map contains the key, return it, else some nonsense value
		// return ( textures.find( label ) != textures.end() ) ?
		// 	textures[ label ].textureHandle : std::numeric_limits< GLuint >::max();
	}

	GLuint GetUnit ( string label ) {
		// return ( textures.find( label ) != textures.end() ) ?
		// 	textures[ label ].textureUnit : std::numeric_limits< GLuint >::max();
	}

	void Update ( string label /*, ... */ ) {
		// pass in new data for the texture
	}

	void EnumerateUnitUsage () {
		// give me a report for all the active textures like:
			// 0 : "Accumulator" ( unit : label ... type/dimensions information? )

		cout << "Texture Unit Usage:" << endl;
		for ( auto& tex : textures ) {
			cout << "  " << tex.textureUnit << " : " << tex.label << endl;
		}
	}

	// total size in bytes
	size_t TotalSize () {
		size_t total = 0;
		for ( auto& tex : textures )
			total += tex.textureSize;
		return total;
	}

	// TODO
	void Remove ( string label ) {
		// delete texture
	}

	// TODO
	void Shutdown () {
		// delete all textures
	}

	std::vector< texture_t > textures; // keep as an ordered set
};

#endif // TEXTURE_H