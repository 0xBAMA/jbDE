#include "../../../engine/includes.h"

const float MAX_DISTANCE = 10000000.0f;

// representing a ray
struct ray_t {
	vec3 origin = vec3( 0.0f );
	vec3 direction = vec3( 0.0f );

	float distance = MAX_DISTANCE;
	vec2 uv = vec2( -1.0f );
};

// triangle data class
struct triangle_t {
	vec3 vertex0, vertex1, vertex2;
	vec3 centroid;

	// further data, vertex colors, texcoords, etc
	vec3 color0, color1, color2;
	vec3 texcoord0, texcoord1, texcoord2;

	// moller-trombore intersection
	void intersect ( ray_t &ray ) {
		const vec3 edge1 = vertex1 - vertex0;
		const vec3 edge2 = vertex2 - vertex0;
		const vec3 h = cross( ray.direction, edge2 );
		const float a = dot( edge1, h );
		if ( a > -0.0001f && a < 0.0001f ) return; // ray parallel to triangle
		const float f = 1.0f / a;
		const vec3 s = ray.origin - vertex0;
		const float u = f * dot( s, h );
		if ( u < 0.0f || u > 1.0f ) { // fail on bad barycentric coords
			return;
		}
		const vec3 q = cross( s, edge1 );
		const float v = f * dot( ray.direction, q );
		if ( v < 0.0f || u + v > 1.0f ) { // fail on bad barycentric coords
			return;
		}
		const float t = f * dot( edge2, q );
		if ( t > 0.0001f ) {
			ray.distance = std::min( ray.distance, t ); // positive distance hit
			if ( ray.distance == t ) { // if we updated the hit, update the corresponding uv
				ray.uv = vec2( u, v );
			}
		}
	}
};

// bvh node class
struct bvhNode_t {

};

// top level bvh class
struct bvh_t {
	std::vector< triangle_t > triangleList;

	// load model triangles
	void Load ( const string filename ) {
		// blah blah refer to old obj loading code
			// they go into an indexed list? tbd - grass etc OBJs
	}

	// before getting into loading models, specifically, let's do some placeholder geometry
	void RandomTriangles () {
		rng val = rng( -1.0f, 1.0f );
		rng col = rng( 0.0f, 1.0f );
		for ( int i = 0; i < 100; i++ ) {
			vec3 r0 = vec3( val(), val(), val() );
			vec3 r1 = vec3( val(), val(), val() );
			vec3 r2 = vec3( val(), val(), val() );
			triangle_t triangle;

			triangle.vertex0 = 10.0f * r0;
			triangle.vertex1 = triangle.vertex0 + r1;
			triangle.vertex2 = triangle.vertex1 + r2;

			triangle.color0 = vec3( col(), col(), col() );
			triangle.color1 = vec3( col(), col(), col() );
			triangle.color2 = vec3( col(), col(), col() );

			triangleList.push_back( triangle );
		}

		cout << newline << "Created " << triangleList.size() << " triangles" << newline;
	}

	// create acceleration structure of bvh nodes, from the list of triangles
	void BuildTree () {
		// ... todo
	}

	// test ray against acceleration structure
		// return information at hit location

	// naiive traversal for comparison
	void naiiveTraversal ( ray_t &ray ) {
		for ( uint i = 0; i < triangleList.size(); i++ ) {
			triangleList[ i ].intersect( ray );
		}
	}
};

// probably a wrapper class for the renderer, here, is easiest
struct testRenderer_t {

	// holding scene data
	bvh_t accelerationStructure;

	// image data, to display
	Image_4F imageBuffer;
	bool imageBufferDirty; // does this need to be resent to the GPU?

	void init () {
		// // create the preview image - probably use alpha channel to send traversal depth kind of stats
		imageBuffer = Image_4F( 720, 480 );
		for ( uint32_t x = 0; x < imageBuffer.Width(); x++ ) {
			for ( uint32_t y = 0; y < imageBuffer.Height(); y++ ) {
				color_4F val;
				val[ red ] = val[ green ] = val[ blue ] = 0.0f;
				val[ alpha ] = 1.0f;
				imageBuffer.SetAtXY( x, y, val );
			}
		}

		accelerationStructure.RandomTriangles();
		accelerationStructure.BuildTree();
		naiiveTraversal();
		imageBufferDirty = true;
	}

	void naiiveTraversal () {
		auto tStart = std::chrono::system_clock::now();
		for ( uint32_t x = 0; x < imageBuffer.Width(); x++ ) {
			for ( uint32_t y = 0; y < imageBuffer.Height(); y++ ) {
				const float xRemap = RangeRemap( x, 0, imageBuffer.Width(), -1.0f, 1.0f );
				const float yRemap = RangeRemap( y, 0, imageBuffer.Height(), -1.0f, 1.0f );

				// test a ray against the triangles
				ray_t ray;
				ray.origin = 16.0f * vec3( xRemap, yRemap, 5.0f );
				ray.direction = vec3( 0.0f, 0.0f, -1.0f );

				accelerationStructure.naiiveTraversal( ray );

				if ( ray.distance < MAX_DISTANCE ) {
					color_4F val;
					val[ red ] = val[ green ] = val[ blue ] = 1.0f / ray.distance;
					// val[ red ] = val[ green ] = val[ blue ] = 1.0f;
					val[ alpha ] = 1.0f;
					imageBuffer.SetAtXY( x, y, val );
				}
			}
		}
		auto tStop = std::chrono::system_clock::now();
		cout << "Finished naiive traversal in " << std::chrono::duration_cast<std::chrono::microseconds>( tStop - tStart ).count() / 1000.0f << "ms." << newline;
	}
};