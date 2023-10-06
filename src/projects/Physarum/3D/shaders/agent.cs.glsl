#version 430 core
layout( local_size_x = 1024, local_size_y = 1, local_size_z = 1 ) in;

layout( r32ui ) uniform uimage3D current;

struct agent_t {

	// location, extra float available
	vec4 position;

	// direction now needs 
	vec4 basisX;
	vec4 basisY;
	vec4 basisZ;

	// other info? parameters per-agent?

};

layout( binding = 0, std430 ) buffer agentData {
	agent_t data[];
};

uniform float stepSize;
uniform float senseAngle;
uniform float senseDistance;
uniform float turnAngle;
uniform uint depositAmount;
uniform uint numAgents;
uniform bool writeBack;

// takes argument in radians
vec2 rotate ( const vec2 v, const float a ) {
	const float s = sin( a );
	const float c = cos( a );
	const mat2 m = mat2( c, -s, s, c );
	return m * v;
}

vec3 wrapPosition ( vec3 pos ) {
	if ( pos.x >=  1.0f ) pos.x -= 2.0f;
	if ( pos.x <= -1.0f ) pos.x += 2.0f;
	if ( pos.y >=  1.0f ) pos.y -= 2.0f;
	if ( pos.y <= -1.0f ) pos.y += 2.0f;
	if ( pos.z >=  1.0f ) pos.z -= 2.0f;
	if ( pos.z <= -1.0f ) pos.z += 2.0f;
	return pos;
}

// random utilites
uniform int wangSeed;
uint seed = 0;
uint wangHash () {
	seed = uint( seed ^ uint( 61 ) ) ^ uint( seed >> uint( 16 ) );
	seed *= uint( 9 );
	seed = seed ^ ( seed >> 4 );
	seed *= uint( 0x27d4eb2d );
	seed = seed ^ ( seed >> 15 );
	return seed;
}

float normalizedRandomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

const float pi = 3.14159265358979323846f;
vec3 randomUnitVector () {
	float z = normalizedRandomFloat() * 2.0f - 1.0f;
	float a = normalizedRandomFloat() * 2.0f * pi;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

vec2 randomInUnitDisk () {
	return randomUnitVector().xy;
}

void main () {

	const uvec3 globalDims = gl_NumWorkGroups * gl_WorkGroupSize;
	const int index = int(
		gl_GlobalInvocationID.z * globalDims.x * globalDims.y +
		gl_GlobalInvocationID.y * globalDims.x +
		gl_GlobalInvocationID.x );

	if ( index < numAgents ) {

		// initialize the rng
		seed = wangSeed + 69 * index;

		agent_t a = data[ index ];

		// vec3 direction = 

		// do the simulation logic to update the value of position
			// take your samples
		const ivec3 rightSampleLoc	= ivec3( imageSize( current ) * wrapPosition( 0.5f * ( a.position.xyz + senseDistance * vec3( rotate( a.basisX.xy, -senseAngle ), a.basisX.z ) + vec3( 1.0f ) ) ) );
		const ivec3 middleSampleLoc	= ivec3( imageSize( current ) * wrapPosition( 0.5f * ( a.position.xyz + senseDistance * a.basisX.xyz + vec3( 1.0f ) ) ) );
		const ivec3 leftSampleLoc	= ivec3( imageSize( current ) * wrapPosition( 0.5f * ( a.position.xyz + senseDistance * vec3( rotate( a.basisX.xy, senseAngle ), a.basisX.z ) + vec3( 1.0f ) ) ) );

		const uint rightSample		= imageLoad( current, rightSampleLoc ).r;
		const uint middleSample		= imageLoad( current, middleSampleLoc ).r;
		const uint leftSample		= imageLoad( current, leftSampleLoc ).r;



		// make a decision on whether to turn left, right, go straight, or a random direction
			// this can be generalized and simplified, as some sort of weighted sum thing - will bear revisiting
		if ( middleSample > leftSample && middleSample > rightSample ) {
			// just retain the existing direction
		} else if ( middleSample < leftSample && middleSample < rightSample ) { // turn a random direction
			a.basisX.xyz = randomUnitVector();
		} else if ( rightSample > middleSample && middleSample > leftSample ) { // turn right (positive)
			a.basisX.xy = rotate( a.basisX.xy, turnAngle );
		} else if ( leftSample > middleSample && middleSample > rightSample ) { // turn left (negative)
			a.basisX.xy = rotate( a.basisX.xy, -turnAngle );
		}
		// else, fall through and retain value of direction

		vec3 newPosition = wrapPosition( a.position.xyz + stepSize * a.basisX.xyz );
		data[ index ].position.xyz = newPosition;
		if ( writeBack ) {
			data[ index ].basisX = a.basisX; // old impl never updated direction????
		}

		imageAtomicAdd( current, ivec3( imageSize( current ) * ( 0.5f * ( newPosition + vec3( 1.0f ) ) ) ), depositAmount );
	}
}
