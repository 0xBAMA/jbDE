#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// ray state buffers
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayStateFront { rayState_t stateFront[]; };
layout( binding = 2, std430 ) buffer rayStateBack  { rayState_t stateBack[]; };
layout( binding = 3, std430 ) buffer rayBufferOffset { uint offset; };

void main () {
	StateReset( stateFront[ gl_GlobalInvocationID.x ] );
}