#version 430 core

layout( binding = 1, r32ui ) uniform uimage2D current;

struct agent_t {
	vec2 position;
	vec2 direction;
};

layout( binding = 0, std430 ) buffer agent_data {
	agent_t data[];
};

uniform float step_size;
uniform float sense_angle;
uniform float sense_distance;
uniform float turn_angle;
uniform uint deposit_amount;

uniform vec2 random_values[ 8 ];

out vec2 v_pos;

//takes argument in radians
vec2 rotate( vec2 v, float a ) {
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
	ivec2 right_sample_loc = ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + sense_distance * rotate( a.direction, -sense_angle ) + vec2( 1.0f ) ) ) );
	ivec2 middle_sample_loc = ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + sense_distance * a.direction + vec2( 1.0f ) ) ) );
	ivec2 left_sample_loc = ivec2( imageSize( current ) * wrapPosition( 0.5f * ( a.position + sense_distance * rotate( a.direction, sense_angle ) + vec2( 1.0f ) ) ) );

	uint right_sample 	= imageLoad( current, right_sample_loc ).r;
	uint middle_sample	= imageLoad( current, middle_sample_loc ).r;
	uint left_sample	= imageLoad( current, left_sample_loc ).r;
	
	// make a decision on whether to turn left, right, go straight, or a random direction
		// this can be generalized and simplified, as some sort of weighted sum thing - will bear revisiting
	if ( middle_sample > left_sample && middle_sample > right_sample ) { 
		// just retain the direction
	} else if ( middle_sample < left_sample && middle_sample < right_sample ) { // turn a random direction
		a.direction = random_values[ gl_VertexID % 8 ];
	} else if ( right_sample > middle_sample && middle_sample > left_sample ) { // turn right (positive)
		a.direction = rotate( a.direction, turn_angle );
	} else if ( left_sample > middle_sample && middle_sample > right_sample ) { // turn left (negative)
		a.direction = rotate( a.direction, -turn_angle);
	}
	// else, fall through and retain value of direction

	vec2 new_position = wrapPosition( a.position + step_size * a.direction );
	data[ index ].position = new_position;
	// data[ index ].direction = a.direction; // old impl never updated direction???? what the fuck???

	imageAtomicAdd( current, ivec2( imageSize( current ) * ( 0.5f * ( new_position + vec2( 1.0f ) ) ) ), deposit_amount );
	v_pos = new_position;

	gl_Position = vec4( new_position.x, new_position.y, 0, 1 );
}
