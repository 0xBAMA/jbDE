#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the Cellular automata
layout( binding = 2, r8ui ) uniform uimage3D CAStateBuffer;

uniform vec3 viewerPosition;

const float maxDist = 1e10f;
const vec2 distBound = vec2( 0.0001f, maxDist );
float box( in vec3 ro, in vec3 rd, in vec3 boxSize, in vec3 boxPosition ) {
	ro -= boxPosition;

	vec3 m = sign( rd ) / max( abs( rd ), 1e-8 );
	vec3 n = m * ro;
	vec3 k = abs( m ) * boxSize;

	vec3 t1 = -n - k;
	vec3 t2 = -n + k;

	float tN = max( max( t1.x, t1.y ), t1.z );
	float tF = min( min( t2.x, t2.y ), t2.z );

	if ( tN > tF || tF <= 0.0f ) {
		return maxDist;
	} else {
		if ( tN >= distBound.x && tN <= distBound.y ) {
			return tN;
		} else if ( tF >= distBound.x && tF <= distBound.y ) {
			return tF;
		} else {
			return maxDist;
		}
	}
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 color = vec3( 0.0f );

	// generate eye ray
	const vec2 iS = imageSize( accumulatorTexture ).xy;
	const vec2 uv = vec2( writeLoc + 0.5f ) / iS;
	const vec2 p = ( uv * 2.0f - 1.0f ) * vec2( iS.x / iS.y, 1.0f );

	// lookat
	const vec3 forwards = normalize( -viewerPosition );
	const vec3 left = cross( forwards, vec3( 0.0f, 0.0f, 1.0f ) );
	const vec3 up = cross( forwards, left );

	const vec3 location = viewerPosition + p.x * left + p.y * up;

	// ray-plane test
	const float d = box( location, forwards, vec3( 0.75f, 0.75f, 10.0f ), vec3( 0.0f, 0.0f, -10.0f ) );
	const vec3 hitPosition = location + forwards * d;

	// dda traversal, from intersection point

	// write the data to the image
	if ( d < maxDist ) {
		color = vec3( d / 5.0f );
	}

	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}
