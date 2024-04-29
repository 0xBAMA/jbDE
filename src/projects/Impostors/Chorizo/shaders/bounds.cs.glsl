#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

#define SPHERE 0
#define CAPSULE 1
#define ROUNDEDBOX 2
// ===================================================================================================
// output from the bounds stage
// ===================================================================================================
struct transform_t {
	mat4 transform;
};
layout( binding = 0, std430 ) buffer transformsBuffer {
	transform_t transforms[];
};

// ===================================================================================================
// input to the bounds stage
// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 1, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};

// ===================================================================================================
// capsule bounds - primary axis is between endpoints, then some cross products to get the others
// ===================================================================================================
transform_t CapsuleBounds( parameters_t parameters ) {
	transform_t result;
	result.transform = mat4( 1.0f );

	const vec3 pointA = vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] );
	const vec3 pointB = vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] );
	const vec3 midPoint = ( pointA + pointB ) / 2.0f;
	const float radius = parameters.data[ 4 ];

	// compute the primary axis between pointA and pointB, and is of length 2 * radius + length( A - B )
	const vec3 d = ( pointA - pointB );
	const vec3 refVector = 0.5f * d * ( ( length( d ) + 2.0f * radius ) / length( d ) );

	vec3 displacementVector = normalize( d );
	const vec3 up = vec3( 1.0f, 1.0f, 1.0f );
	if ( dot( normalize( displacementVector ), up ) > 0.999f ) {
		// is it along some reference vector? if yes, we need to do the math with some linearly independent vector
		displacementVector = normalize( vec3( 1.0f, 1.0f, 0.0f ) );
	}

	// box transform needs to be constructed such that:
		// cross product to get first orthogonal vector
			// second axis is that first orthogonal vector, of length radius
	const vec3 firstOrthoVec = radius * normalize( cross( up, displacementVector ) );
		// cross product again to get second orthogonal vector
			// third axis is that second orthogonal vector, of length radius
	const vec3 secondOrthoVec = radius * normalize( cross( displacementVector, firstOrthoVec ) );

	result.transform = mat4(
		refVector, 0.0f,
		secondOrthoVec, 0.0f,
		firstOrthoVec, 0.0f,
		midPoint, 1.0f
	);

	return result;
}

// ===================================================================================================
void main () {

	// getting this object's parameters
	uint index = gl_GlobalInvocationID.x;
	parameters_t parameters = parametersList[ index ];
	transform_t result;

	// changing behavior, based on the contained primitive
	switch( int( parameters.data[ 0 ] ) ) {

		case CAPSULE:
			// compute the capsule bounding box
			result = CapsuleBounds( parameters );
			break;

		default:
			break;
	}

	// write the result to the other SSBO
	transforms[ index ] = result;
}