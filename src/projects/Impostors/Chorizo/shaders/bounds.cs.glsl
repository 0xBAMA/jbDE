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
	// todo
	transform_t result;
	result.transform = mat4( 1.0f );
	result.inverseTransform = mat4( 1.0f );
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