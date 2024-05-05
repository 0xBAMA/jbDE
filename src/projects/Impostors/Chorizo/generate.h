#include "../../../engine/includes.h"

struct geometryManager_t {
	// 0..1 within the palette, with an integer palette select
	rng paletteValue = rng( 0.0f, 1.0f );
	int paletteSelect = 0;
	float GetPaletteValue() { return paletteValue() + paletteSelect; }

	std::vector< float > parametersList;
	int count = 0;

	void AddPrimitive( const float parameters[ 16 ] );
	void AddCapsule( const vec3 pointA, const vec3 pointB, const float radius, const vec3 color );
	void AddRoundedBox( const vec3 centerPoint, const vec3 scaleFactors, const vec2 eulerAngles, const float roundingFactor, const vec3 color );
};

#define SPHERE		0 // plan is to redo point sprites for this - they are much faster
#define CAPSULE		1
#define ROUNDEDBOX	2

void geometryManager_t::AddPrimitive( const float parameters[ 16 ] ) {
	// adding one shape to the buffer
	count++;
	for ( int i = 0; i < 16; i++ ) {
		parametersList.push_back( parameters[ i ] );
	}
}

void geometryManager_t::AddCapsule( const vec3 pointA, const vec3 pointB, const float radius, const vec3 color = vec3( -1.0f ) ) {
	// handling color in an interesting way - alpha is a signalling value that tells it whether to use the contained color, or a palette value
	vec4 c = ( color == vec3( -1.0f ) ) ? vec4( 0.0f, 0.0f, 0.0f, GetPaletteValue() ) : vec4( color.xyz(), -1.0f );

	const float parameters[] = {
		CAPSULE, pointA.x, pointA.y, pointA.z,
		radius, pointB.x, pointB.y, pointB.z,
		0.0f, 0.0f, 0.0f, 0.0f,
		c.x, c.y, c.z, c.w
	};
	AddPrimitive( parameters );
}

// need to add the rounding factor, how much to round the edges
void geometryManager_t::AddRoundedBox( const vec3 centerPoint, const vec3 scaleFactors, const vec2 eulerAngles, const float roundingFactor, const vec3 color = vec3( -1.0f ) ) {
	vec4 c = ( color == vec3( -1.0f ) ) ? vec4( 0.0f, 0.0f, 0.0f, GetPaletteValue() ) : vec4( color.xyz(), -1.0f );

	// packing the euler angles together - theta, about the poles, is in the fractional component, and phi, elevation, is in the integer part
	const float thetaRemapped = fmodf( eulerAngles.x, tau ) / tau;
	const float phiRemapped = glm::trunc( RangeRemap( eulerAngles.y, -pi / 2.0f, pi / 2.0f, -255.0f, 255.0f ) );
	const float packedEuler = thetaRemapped + phiRemapped;
	const float parameters[] = {
		ROUNDEDBOX, centerPoint.x, centerPoint.y, centerPoint.z,
		packedEuler, scaleFactors.x, scaleFactors.y, scaleFactors.z,
		roundingFactor, 0.0f, 0.0f, 0.0f,
		c.x, c.y, c.z, c.w
	};
	AddPrimitive( parameters );
}