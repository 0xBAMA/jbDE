#include "../../../engine/includes.h"

struct geometryManager_t {
	rng paletteValue = rng( 0.0f, 1.0f );
	std::vector< float > parametersList;
	int count = 0;

	void AddPrimitive( const float parameters[ 16 ] );
	// void AddSphere( const vec3 center, const float radius ) = {}
	void AddCapsule( const vec3 pointA, const vec3 pointB, const float radius );
};

#define SPHERE 0
#define CAPSULE 1
#define ROUNDEDBOX 2

void geometryManager_t::AddPrimitive( const float parameters[ 16 ] ) {
	// adding one shape to the buffer
	count++;
	for ( int i = 0; i < 16; i++ ) {
		parametersList.push_back( parameters[ i ] );
	}
}

void geometryManager_t::AddCapsule( const vec3 pointA, const vec3 pointB, const float radius ) {
	// this one is pretty trivial
	const float parameters[] = {
		CAPSULE, pointA.x, pointA.y, pointA.z,
		radius, pointB.x, pointB.y, pointB.z,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, GetPaletteValue()
	};
	AddPrimitive( parameters );
}

// need to add the rounding factor, how much to round the edges
void geometryManager_t::AddRoundedBox( const vec3 centerPoint, const vec3 scaleFactors, const vec2 eulerAngles, const float roundingFactor ) {
	// packing the euler angles together - theta, about the poles, is in the fractional component, and phi, elevation, is in the integer part
	const float thetaRemapped = fmodf( eulerAngles.x, tau ) / tau;
	const float phiRemapped = glm::trunc( RangeRemap( eulerAngles.y, -pi / 2.0f, pi / 2.0f, -255.0f, 255.0f ) );
	const float packedEuler = thetaRemapped + phiRemapped;
	const float parameters[] = {
		ROUNDEDBOX, centerPoint.x, centerPoint.y, centerPoint.z,
		packedEuler, scaleFactors.x, scaleFactors.y, scaleFactors.z,
		roundingFactor, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, GetPaletteValue()
	};
	AddPrimitive( parameters );
}