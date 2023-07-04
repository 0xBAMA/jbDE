#version 430 core

layout( binding = 1, r32ui ) uniform uimage2D current;

layout( binding = 0, std430 ) buffer agent_data
{
	vec2 data[];	// [position0, direction0], [position1, direction1], ...
};





uniform float step_size;
uniform float sense_angle;
uniform float sense_distance;
uniform float turn_angle;
uniform uint deposit_amount;

uniform vec2 random_values[8];

out vec2 v_pos;




//takes argument in radians
vec2 rotate(vec2 v, float a) 
{
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, -s, s, c);
	return m * v;
}



void main()
{
	int index = gl_VertexID * 2;
	
	vec2 position = data[index];
	vec2 direction = data[index+1];
	
	// do the simulation logic to update the value of position
	
		// take your samples
	ivec2 right_sample_loc = ivec2(imageSize(current) * (0.5 * (position + sense_distance * rotate(direction, -sense_angle) + vec2(1))));
	ivec2 middle_sample_loc = ivec2(imageSize(current) * (0.5 * (position + sense_distance * direction + vec2(1))));
	ivec2 left_sample_loc = ivec2(imageSize(current) * (0.5 * (position + sense_distance * rotate(direction, sense_angle) + vec2(1))));
		
	uint right_sample = imageLoad(current, right_sample_loc).r;
	uint middle_sample = imageLoad(current, middle_sample_loc).r;
	uint left_sample = imageLoad(current, left_sample_loc).r;
	
		// make a decision on whether to turn left, right, go straight, or a random direction
	if(middle_sample > left_sample && middle_sample > right_sample)
	{ 
		// just retain the direction		
	}
	else if(middle_sample < left_sample && middle_sample < right_sample)
	{ // turn a random direction
		direction = random_values[gl_VertexID % 8];
	}
	else if(right_sample > middle_sample && middle_sample > left_sample)
	{ // turn right (positive)
		direction = rotate(direction, turn_angle);
	}
	else if(left_sample > middle_sample && middle_sample > right_sample)
	{ // turn left (negative)
		direction = rotate(direction, -turn_angle);
	}
	// else, fall through and retain value of direction
	
	vec2 new_position = (position + step_size * direction);
	
	// wrap logic
	if(new_position.x > 1.0)
		new_position.x = -1.0;
		
	if(new_position.x < -1.0)
		new_position.x = 1.0;
		
	if(new_position.y > 1.0)
		new_position.y = -1.0;
		
	if(new_position.y < -1.0)
		new_position.y = 1.0;
	
	data[index] = new_position;
	
	
	imageAtomicAdd(current, ivec2(imageSize(current)*(0.5*(new_position+vec2(1)))), deposit_amount);
	v_pos = new_position;
	
	gl_Position = vec4(new_position.x, new_position.y, 0, 1);
}
