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
	mat4 inverseTransform;
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
	const vec3 refVector = ( pointA - pointB ) * ( 1.0f + 2.0f * radius );
	result.transform[ 0 ][ 2 ] = refVector.x;
	result.transform[ 1 ][ 2 ] = refVector.y;
	result.transform[ 2 ][ 2 ] = refVector.z;

	vec3 displacementVector = refVector;
	const vec3 up = vec3( 0.0f, 1.0f, 0.0f );
	if ( normalize( refVector ) == up ) {
		// is it along some reference vector? if yes, we need to do the math with some linearly independent vector
		displacementVector = normalize( vec3( 1.0f, 1.0f, 0.0f ) );
	}

	// box transform needs to be constructed such that:
		// cross product to get first orthogonal vector
			// second axis is that first orthogonal vector, of length radius
	const vec3 firstOrthoVec = radius * normalize( cross( displacementVector, up ) );
	result.transform[ 0 ][ 1 ] = firstOrthoVec.x;
	result.transform[ 1 ][ 1 ] = firstOrthoVec.y;
	result.transform[ 2 ][ 1 ] = firstOrthoVec.z;

		// cross product again to get second orthogonal vector
			// third axis is that second orthogonal vector, of length radius
	const vec3 secondOrthoVec = radius * normalize( cross( firstOrthoVec, displacementVector ) );
	result.transform[ 0 ][ 0 ] = secondOrthoVec.x;
	result.transform[ 1 ][ 0 ] = secondOrthoVec.y;
	result.transform[ 2 ][ 0 ] = secondOrthoVec.z;

	result.transform[ 0 ][ 3 ] = midPoint.x;
	result.transform[ 1 ][ 3 ] = midPoint.y;
	result.transform[ 2 ][ 3 ] = midPoint.z;

	// result.transform = mat4( 1.0f );
	result.inverseTransform = inverse( result.transform );
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