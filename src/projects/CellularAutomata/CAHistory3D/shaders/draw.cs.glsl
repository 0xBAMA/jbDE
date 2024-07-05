#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the Cellular automata
layout( binding = 2, r8ui ) uniform uimage3D CAStateBuffer;

uniform vec3 viewerPosition;

float planeTest( in vec3 ro, in vec3 rd, in vec3 planeNormal, in float planeDist ) {
	const float maxDist = 1e10f;
	const vec2 distBound = vec2( 0.0001f, maxDist );

	float a = dot( rd, planeNormal );
	float d = -( dot( ro, planeNormal ) + planeDist ) / a;
	if ( a > 0.0f || d < distBound.x || d > distBound.y ) {
		return maxDist;
	} else {
		return d;
	}
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 color = vec3( 0.0f );

	const vec2 iS = imageSize( accumulatorTexture ).xy;

	// generate eye ray
	const vec2 uv = vec2( writeLoc + 0.5f ) / iS;
	const vec2 p = ( uv * 2.0f - 1.0f ) * vec2( iS.x / iS.y, 1.0f );

	// lookat
	const vec3 forwards = normalize( -viewerPosition );
	const vec3 left = cross( forwards, vec3( 0.0f, 0.0f, 1.0f ) );
	const vec3 up = cross( forwards, left );

	const vec3 location = viewerPosition + p.x * left + p.y * up;

	// ray-plane test
	const float d = planeTest( location, forwards, vec3( 0.0f, 0.0f, 1.0f ), 0.0f );
	const vec3 hitPosition = location + forwards * d;

	// dda traversal, from intersection point

	// write the data to the image
	if ( abs( hitPosition.x ) < 0.5f && abs( hitPosition.y ) < 0.5f ) {
		color = vec3( d ) * vec3( 1.0f, 0.0f, 0.0f );
	}

	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}
