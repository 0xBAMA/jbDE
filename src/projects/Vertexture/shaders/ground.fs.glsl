#version 430

in vec3 color;
out vec4 glFragColor;

// moving lights state
uniform int lightCount;

struct light_t {
	vec4 position;
	vec4 color;
};

layout( binding = 4, std430 ) buffer lightData {
	light_t data[];
};

void main() {
	glFragColor = vec4( color, 1.0f );
}