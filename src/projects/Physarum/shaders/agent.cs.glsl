#version 430 core
layout( local_size_x = 1024, local_size_y = 1, local_size_z = 1 ) in;

layout( r32ui ) uniform uimage2D current;

struct agent_t {
	vec2 position;
	vec2 direction;
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

uniform vec2 randomValues[ 8 ];

// takes argument in radians
vec2 rotate ( const vec2 v, const float a ) {
	const float s = sin( a );
	const float c = cos( a );
	const mat2 m = mat2( c, -s, s, c );
	return m * v;
}

vec2 wrapPosition ( vec2 pos ) {
	if ( pos.x >=  1.0f ) pos.x -= 2.0f;
	if ( pos.x <= -1.0f ) pos.x += 2.0f;
	if ( pos.y >=  1.0f ) pos.y -= 2.0f;
	if ( pos.y <= -1.0f ) pos.y += 2.0f;
	return pos;
}

void main () {

	const uvec3 globalDims = gl_NumWorkGroups * gl_WorkGroupSize;
	const int index = int(
		gl_GlobalInvocationID.z * globalDims.x * globalDims.y +
		gl_GlobalInvocationID.y * globalDims.x +
		gl_GlobalInvocationID.x );

	if ( index < numAgents ) {
		agent_t a = data[ index ];

		// do the simulation logic to update the value of position
			// take your samples
		const ivec2 rightSampleLoc	= ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + senseDistance * rotate( a.direction, -senseAngle ) + vec2( 1.0f ) ) ) );
		const ivec2 middleSampleLoc	= ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + senseDistance * a.direction + vec2( 1.0f ) ) ) );
		const ivec2 leftSampleLoc	= ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + senseDistance * rotate( a.direction, senseAngle ) + vec2( 1.0f ) ) ) );

		const uint rightSample		= imageLoad( current, rightSampleLoc ).r;
		const uint middleSample		= imageLoad( current, middleSampleLoc ).r;
		const uint leftSample		= imageLoad( current, leftSampleLoc ).r;

		// make a decision on whether to turn left, right, go straight, or a random direction
			// this can be generalized and simplified, as some sort of weighted sum thing - will bear revisiting
		if ( middleSample > leftSample && middleSample > rightSample ) {
			// just retain the existing direction
		} else if ( middleSample < leftSample && middleSample < rightSample ) { // turn a random direction
			a.direction = randomValues[ index % 8 ];
		} else if ( rightSample > middleSample && middleSample > leftSample ) { // turn right (positive)
			a.direction = rotate( a.direction, turnAngle );
		} else if ( leftSample > middleSample && middleSample > rightSample ) { // turn left (negative)
			a.direction = rotate( a.direction, -turnAngle );
		}
		// else, fall through and retain value of direction

		vec2 newPosition = wrapPosition( a.position + stepSize * a.direction );
		data[ index ].position = newPosition;
		if ( writeBack ) {
			data[ index ].direction = a.direction; // old impl never updated direction????
		}

		imageAtomicAdd( current, ivec2( imageSize( current ) * ( 0.5f * ( newPosition + vec2( 1.0f ) ) ) ), depositAmount );
	}
}
