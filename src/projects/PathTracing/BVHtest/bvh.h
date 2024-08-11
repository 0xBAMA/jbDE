#include "../../../engine/includes.h"

// drawing heavily from:
// https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/

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
	vec3 aabbMin, aabbMax;
	uint32_t leftChild, firstPrimitiveIdx, primitiveCount;
	bool isLeaf() { return primitiveCount > 0; }
};

// top level bvh class
struct bvh_t {
	std::vector< triangle_t > triangleList;
	std::vector< uint32_t > triangleIndices;

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
			vec3 r1 = vec3( val(), val(), val() ) * 2.0f;
			vec3 r2 = vec3( val(), val(), val() ) * 2.0f;
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
	std::vector< bvhNode_t > bvhNodes;
	uint32_t rootNodeIdx = 0;
	uint32_t nodesUsed = 1;

	void UpdateNodeBounds ( uint32_t nodeIndex ) {
		// calculate the bounds for a BVH node, based on contained primitives
		bvhNode_t &node = bvhNodes[ nodeIndex ];
		node.aabbMin = vec3( 1e30f );
		node.aabbMax = vec3( -1e30f );
		for ( uint i = 0; i < node.primitiveCount; i++ ) {
			triangle_t &leafTriangle = triangleList[ triangleIndices[ node.firstPrimitiveIdx + i ] ];

			// this is shorter with helper functions, but it's not that big a deal
			node.aabbMin.x = std::min( node.aabbMin.x, leafTriangle.vertex0.x );
			node.aabbMin.y = std::min( node.aabbMin.y, leafTriangle.vertex0.y );
			node.aabbMin.z = std::min( node.aabbMin.z, leafTriangle.vertex0.z );

			node.aabbMin.x = std::min( node.aabbMin.x, leafTriangle.vertex1.x );
			node.aabbMin.y = std::min( node.aabbMin.y, leafTriangle.vertex1.y );
			node.aabbMin.z = std::min( node.aabbMin.z, leafTriangle.vertex1.z );

			node.aabbMin.x = std::min( node.aabbMin.x, leafTriangle.vertex2.x );
			node.aabbMin.y = std::min( node.aabbMin.y, leafTriangle.vertex2.y );
			node.aabbMin.z = std::min( node.aabbMin.z, leafTriangle.vertex2.z );

			node.aabbMax.x = std::max( node.aabbMax.x, leafTriangle.vertex0.x );
			node.aabbMax.y = std::max( node.aabbMax.y, leafTriangle.vertex0.y );
			node.aabbMax.z = std::max( node.aabbMax.z, leafTriangle.vertex0.z );

			node.aabbMax.x = std::max( node.aabbMax.x, leafTriangle.vertex1.x );
			node.aabbMax.y = std::max( node.aabbMax.y, leafTriangle.vertex1.y );
			node.aabbMax.z = std::max( node.aabbMax.z, leafTriangle.vertex1.z );

			node.aabbMax.x = std::max( node.aabbMax.x, leafTriangle.vertex2.x );
			node.aabbMax.y = std::max( node.aabbMax.y, leafTriangle.vertex2.y );
			node.aabbMax.z = std::max( node.aabbMax.z, leafTriangle.vertex2.z );
		}
	}

	void Subdivide ( uint32_t nodeIndex ) {
		// ref current node
		bvhNode_t &currentNode = bvhNodes[ nodeIndex ];

		// we can go ahead and kill it if we at a leaf node
		if ( currentNode.primitiveCount <= 2 ) return;

		// pick the split plane axis ( longest axis of the node's bounding box )
		vec3 axisSpans = vec3(
			abs( currentNode.aabbMin.x - currentNode.aabbMax.x ),
			abs( currentNode.aabbMin.y - currentNode.aabbMax.y ),
			abs( currentNode.aabbMin.z - currentNode.aabbMax.z )
		);
		int axis = 0; // initially pick x axis
		if ( axisSpans.y > axisSpans.x ) axis = 1; // we see y axis is larger than x: pick it, at least temporarily
		if ( axisSpans.z > axisSpans[ axis ] ) axis = 2; // and again for the z axis, pick it, if larger

		// this is for the in-place partitioning - it's halfway along this selected longest axis
		const float dividingLine = currentNode.aabbMin[ axis ]  + axisSpans[ axis ] * 0.5f;

		// divide the group into two halves along this axis, partition by putting the split primitives at the end of the list
		int i = currentNode.firstPrimitiveIdx;
		int j = i + currentNode.primitiveCount - 1;

		// iterate through, and place triangles based on centroid position relative to the dividing line
		while ( i <= j ) {
			if ( triangleList[ triangleIndices[ i ] ].centroid[ axis ] < dividingLine ) {
				i++;
			} else {
				std::swap( triangleIndices[ i ], triangleIndices[ j-- ] );
			}
		}

		// because these are child nodes, we know they are subsets of this parent node, interesting
			// this enables that in-place partitioning, which is pretty cool

		// where is the partition?
		const uint32_t leftCount = i - currentNode.firstPrimitiveIdx;

		// abort the split if either of the sides are empty
		if ( leftCount == 0 || leftCount == currentNode.primitiveCount ) return;

		// otherwise we're creating child nodes for each half
		int leftChildIdx = nodesUsed++;
		int rightChildIdx = nodesUsed++;

		// ...so I guess you can always get the right child index back, by looking at base idx + primitive count
		bvhNodes[ leftChildIdx ].firstPrimitiveIdx = currentNode.firstPrimitiveIdx;
		bvhNodes[ leftChildIdx ].primitiveCount = leftCount;

		bvhNodes[ rightChildIdx ].firstPrimitiveIdx = i;
		bvhNodes[ rightChildIdx ].primitiveCount = currentNode.primitiveCount - leftCount;

		// not tracking the isLeaf bool, means primitiveCount has to be used to id leaves vs interior nodes
		currentNode.primitiveCount = 0; // so setting this now interior node to zero to indicate this isn't a leaf
		currentNode.leftChild = leftChildIdx; // and the child there - note that right child can always be recovered with +1

		// update the bounds for the child nodes
		UpdateNodeBounds( leftChildIdx );
		UpdateNodeBounds( rightChildIdx );

		// recurse into each of the child nodes, from this
		Subdivide( leftChildIdx );
		Subdivide( rightChildIdx );
	}

	void BuildTree () {
		// maximum possible size is 2N + 1 nodes, where N is the number of triangles
			// ( N leaves have N/2 parents, N/4 granparents, etc... )
		bvhNodes.resize( triangleList.size() * 2 - 1 );

		// precompute the centroids of all the triangles
		for ( auto& triangle : triangleList ) {
			triangle.centroid = vec3( triangle.vertex0 + triangle.vertex1 + triangle.vertex2 ) / 3.0f;
		}

		// populate the index buffer, initially just in order
		triangleIndices.resize( triangleList.size() );
		for ( uint32_t i = 0; i < triangleIndices.size(); i++ ) {
			triangleIndices[ i ] = i;
		}

		// not fully understanding this piece, yet
		bvhNode_t &root = bvhNodes[ rootNodeIdx ];
		root.leftChild = 0;
		root.firstPrimitiveIdx = 0;
		root.primitiveCount = triangleList.size();

		// updating node bounds, for the root node - so it's the bounding box for entire model
		UpdateNodeBounds( rootNodeIdx );

		// subdivide from the root node, recursively
		Subdivide( rootNodeIdx );
	}

	// naiive traversal for comparison
	void naiiveTraversal ( ray_t &ray ) {
		// return information at hit location
		for ( uint i = 0; i < triangleList.size(); i++ ) {
			triangleList[ i ].intersect( ray );
		}
	}

	// test ray against acceleration structure
	void acceleratedTraversal ( ray_t &ray ) {
		// todo
	}
};

// probably a wrapper class for the renderer, here, is easiest
struct testRenderer_t {

	// holding scene data
	bvh_t accelerationStructure;

	// image data, to display
	Image_4F imageBuffer;
	bool imageBufferDirty; // does this image need to be resent to the GPU?

	void init () {
		// create the preview image - probably use alpha channel to send traversal depth kind of stats
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
		rng jitter = rng( 0.0f, 1.0f );
		for ( uint32_t x = 0; x < imageBuffer.Width(); x++ ) {
			for ( uint32_t y = 0; y < imageBuffer.Height(); y++ ) {
				const int numSamples = 20;
				vec3 colorAverage = vec3( 0.0f );
				for ( int sample = 0; sample < 10; sample++ ) {
					const float xRemap = RangeRemap( x + jitter(), 0, imageBuffer.Width(), -1.0f, 1.0f );
					const float yRemap = RangeRemap( y + jitter(), 0, imageBuffer.Height(), -1.0f, 1.0f );

					// test a ray against the triangles
					ray_t ray;
					ray.origin = 16.0f * vec3( xRemap * ( float( imageBuffer.Width() ) / float( imageBuffer.Height() ) ), yRemap, 5.0f );
					ray.direction = vec3( 0.0f, 0.0f, -1.0f );

					accelerationStructure.naiiveTraversal( ray );

					if ( ray.distance < MAX_DISTANCE ) {
						colorAverage += vec3( ray.uv.x, ray.uv.y, 1.0f - ray.uv.x - ray.uv.y ); // barycentric colors;
					}
				}
				colorAverage /= float( numSamples );
				color_4F val;
				val[ red ] = colorAverage.r;
				val[ green ] = colorAverage.g;
				val[ blue ] = colorAverage.b;
				val[ alpha ] = 1.0f;
				imageBuffer.SetAtXY( x, y, val );
			}
		}
		auto tStop = std::chrono::system_clock::now();
		cout << "Finished naiive traversal in " << std::chrono::duration_cast<std::chrono::microseconds>( tStop - tStart ).count() / 1000.0f << "ms." << newline;
	}

	// accelerated traversal, for comparison
};