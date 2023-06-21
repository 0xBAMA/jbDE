#version 430 core
// layout( binding = 0 ) uniform sampler2D current;
uniform sampler2D current;
uniform vec2 resolution;
out vec4 fragmentOutput;
void main () {
	fragmentOutput = texture( current, gl_FragCoord.xy / resolution );
	// fragmentOutput = vec4( gl_FragCoord.xy / resolution, 0.0f, 1.0f );
}
