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
	const float parameters[] = {
		CAPSULE, pointA.x, pointA.y, pointA.z,
		radius, pointB.x, pointB.y, pointB.z,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, paletteValue()
	};
	AddPrimitive( parameters );
}