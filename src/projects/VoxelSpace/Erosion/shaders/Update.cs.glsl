#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( r32f ) uniform image2D sourceData;
layout( rgba8ui ) uniform uimage2D map;

const float sqrt2 = sqrt( 2.0f );
const float scale = 60.0f;

vec3 GetNormal ( ivec2 location ) {
	const float atLocCache = imageLoad( sourceData, location + ivec2( 0, 0 ) ).r;
	const float cachep0 = imageLoad( sourceData, location + ivec2(  1,  0 ) ).r;
	const float cachen0 = imageLoad( sourceData, location + ivec2( -1,  0 ) ).r;
	const float cache0p = imageLoad( sourceData, location + ivec2(  0,  1 ) ).r;
	const float cache0n = imageLoad( sourceData, location + ivec2(  0, -1 ) ).r;
	const float cachepp = imageLoad( sourceData, location + ivec2(  1,  1 ) ).r;
	const float cachepn = imageLoad( sourceData, location + ivec2(  1, -1 ) ).r;
	const float cachenp = imageLoad( sourceData, location + ivec2( -1,  1 ) ).r;
	const float cachenn = imageLoad( sourceData, location + ivec2( -1, -1 ) ).r;

	vec3 n = vec3( 0.15f ) * normalize(vec3( scale * ( atLocCache - cachep0 ), 1.0f, 0.0f ) );  // Positive X
	n += vec3( 0.15f ) * normalize( vec3( scale * ( cachen0 - atLocCache ), 1.0f, 0.0f ) );     // Negative X
	n += vec3( 0.15f ) * normalize( vec3( 0.0f, 1.0f, scale * ( atLocCache - cache0p ) ) );     // Positive Y
	n += vec3( 0.15f ) * normalize( vec3( 0.0f, 1.0f, scale * ( cache0n - atLocCache ) ) );     // Negative Y
	// diagonals
	n += vec3( 0.1f ) * normalize( vec3( scale * ( atLocCache - cachepp ) / sqrt2, sqrt2, scale * ( atLocCache - cachepp ) / sqrt2 ) );
	n += vec3( 0.1f ) * normalize( vec3( scale * ( atLocCache - cachepn ) / sqrt2, sqrt2, scale * ( atLocCache - cachepn ) / sqrt2 ) );
	n += vec3( 0.1f ) * normalize( vec3( scale * ( atLocCache - cachenp ) / sqrt2, sqrt2, scale * ( atLocCache - cachenp ) / sqrt2 ) );
	n += vec3( 0.1f ) * normalize( vec3( scale * ( atLocCache - cachenn ) / sqrt2, sqrt2, scale * ( atLocCache - cachenn ) / sqrt2 ) );

	n.y *= 0.001f;
	return normalize( n );
}

void main () {
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	float src = 120.0f * imageLoad( sourceData, location ).r + 75.0f;
	vec3 normal = GetNormal( location );

	vec3 shadingResult = vec3( dot( vec3( 0.0f, 1.0f, 0.0f ), normal ) ) * ( 1.0f / 255.0f ) * vec3( 1.618f * ( 255.0f - src * 255.0f ), 1.618 * float( src * 255.0f ), 1.0f / ( src * 255.0f ) );

	imageStore( map, location, uvec4( uvec3( shadingResult.xyz ), src ) );
}
