#version 430 core

layout( binding = 1, r32ui ) uniform uimage2D current;

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

uniform vec2 randomValues[ 8 ];

out vec2 v_pos;

//takes argument in radians
vec2 rotate ( vec2 v, float a ) {
	float s = sin( a );
	float c = cos( a );
	mat2 m = mat2( c, -s, s, c );
	return m * v;
}

vec2 wrapPosition ( vec2 pos ) {

	if ( pos.x >= 1.0f )
		pos.x -= 2.0f;

	if ( pos.x <= -1.0f )
		pos.x += 2.0f;

	if ( pos.y >= 1.0f )
		pos.y -= 2.0f;

	if ( pos.y <= -1.0f )
		pos.y += 2.0f;

	return pos;
}

void main () {
	int index = gl_VertexID;
	agent_t a = data[ index ];
	
	// do the simulation logic to update the value of position
		// take your samples
	ivec2 rightSampleLoc = ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + senseDistance * rotate( a.direction, -senseAngle ) + vec2( 1.0f ) ) ) );
	ivec2 middleSampleLoc = ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + senseDistance * a.direction + vec2( 1.0f ) ) ) );
	ivec2 leftSampleLoc = ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + senseDistance * rotate( a.direction, senseAngle ) + vec2( 1.0f ) ) ) );

	uint rightSample 	= imageLoad( current, rightSampleLoc ).r;
	uint middleSample	= imageLoad( current, middleSampleLoc ).r;
	uint leftSample		= imageLoad( current, leftSampleLoc ).r;
	
	// make a decision on whether to turn left, right, go straight, or a random direction
		// this can be generalized and simplified, as some sort of weighted sum thing - will bear revisiting
	if ( middleSample > leftSample && middleSample > rightSample ) { 
		// just retain the direction
	} else if ( middleSample < leftSample && middleSample < rightSample ) { // turn a random direction
		a.direction = randomValues[ gl_VertexID % 8 ];
	} else if ( rightSample > middleSample && middleSample > leftSample ) { // turn right (positive)
		a.direction = rotate( a.direction, turnAngle );
	} else if ( leftSample > middleSample && middleSample > rightSample ) { // turn left (negative)
		a.direction = rotate( a.direction, -turnAngle );
	}
	// else, fall through and retain value of direction

	vec2 newPosition = wrapPosition( a.position + stepSize * a.direction );
	data[ index ].position = newPosition;
	// data[ index ].direction = a.direction; // old impl never updated direction????

	imageAtomicAdd( current, ivec2( imageSize( current ) * ( 0.5f * ( newPosition + vec2( 1.0f ) ) ) ), depositAmount );
	v_pos = newPosition;

	gl_Position = vec4( newPosition.x, newPosition.y, 0.0f, 1.0f );
}
