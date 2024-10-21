#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;


// ray state buffer
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

// basic raymarch stuff
vec3 Rotate(vec3 z,float AngPFXY,float AngPFYZ,float AngPFXZ) {
	float sPFXY = sin(radians(AngPFXY)); float cPFXY = cos(radians(AngPFXY));
	float sPFYZ = sin(radians(AngPFYZ)); float cPFYZ = cos(radians(AngPFYZ));
	float sPFXZ = sin(radians(AngPFXZ)); float cPFXZ = cos(radians(AngPFXZ));

	float zx = z.x; float zy = z.y; float zz = z.z; float t;

	// rotate BACK
	t = zx; // XY
	zx = cPFXY * t - sPFXY * zy; zy = sPFXY * t + cPFXY * zy;
	t = zx; // XZ
	zx = cPFXZ * t + sPFXZ * zz; zz = -sPFXZ * t + cPFXZ * zz;
	t = zy; // YZ
	zy = cPFYZ * t - sPFYZ * zz; zz = sPFYZ * t + cPFYZ * zz;
	return vec3(zx,zy,zz);
}

float de( vec3 p ) {
	float Scale = 1.34f;
	float FoldY = 1.025709f;
	float FoldX = 1.025709f;
	float FoldZ = 0.035271f;
	float JuliaX = -1.763517f;
	float JuliaY = 0.392486f;
	float JuliaZ = -1.734913f;
	float AngX = -80.00209f;
	float AngY = 0.0f;
	float AngZ = -28.096322f;
	float Offset = -3.036726f;
	int EnableOffset = 1;
	int Iterations = 80;
	float Precision = 1.0f;
	// output _sdf c = _SDFDEF)

	vec4 OrbitTrap = vec4(1,1,1,1);
	float u2 = 1;
	float v2 = 1;
	if(EnableOffset != 0)p = Offset+abs(vec3(p.x,p.y,p.z));

	vec3 p0 = vec3(JuliaX,JuliaY,JuliaZ);
	float l = 0.0;
	int i=0;
	for (i=0; i<Iterations; i++) {
		p = Rotate(p,AngX,AngY,AngZ);
		p.x=abs(p.x+FoldX)-FoldX;
		p.y=abs(p.y+FoldY)-FoldY;
		p.z=abs(p.z+FoldZ)-FoldZ;
		p=p*Scale+p0;
		l=length(p);

		float rr = dot(p,p) + 1.5f;
		// OrbitTrap.r = max( OrbitTrap.r, rr );
		// hitColor = vec3( abs( p / 10.0f ) );
	}
	// hitColor = vec3( OrbitTrap.r, abs( 10.0f * p.xy ) );
	return Precision*(l)*pow(Scale, -float(i));
}

const float raymarchMaxDistance = 50.0f;
const float raymarchUnderstep = 0.9f;
const int raymarchMaxSteps = 300;
const float epsilon = 0.001f;

float raymarch ( vec3 rayOrigin, vec3 rayDirection ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = rayOrigin;
	for ( int steps = 0; steps < raymarchMaxSteps; steps++ ) {
		pQuery = rayOrigin + dTotal * rayDirection;
		dQuery = de( pQuery );
		dTotal += dQuery * raymarchUnderstep;
		if ( dTotal > raymarchMaxDistance || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	return dTotal;
}

vec3 SDFNormal( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );
}

void main () {

	rayState_t myRay = state[ gl_GlobalInvocationID.x ];

	// do the raymarch...
	const vec3 origin = GetRayOrigin( myRay );
	const vec3 direction = GetRayDirection( myRay );

	if ( length( direction ) > 0.5f ) {
		// update the intersection info
		const float distanceToHit = raymarch( origin, direction );
		SetHitDistance( state[ gl_GlobalInvocationID.x ], distanceToHit );
		SetHitIntersector( state[ gl_GlobalInvocationID.x ], ( distanceToHit < raymarchMaxDistance ) ? SDFHIT : NOHIT );
		SetHitNormal( state[ gl_GlobalInvocationID.x ], SDFNormal( origin + direction * distanceToHit ) );
	}
}