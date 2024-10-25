#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// ray state buffer
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

#include "random.h"
#include "hg_sdf.glsl"

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

float deTrees ( vec3 p ) {
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

float deTemple ( vec3 p ) {
	float s = 2.;
	float e = 0.;
	for(int j=0;++j<7;)
		p.xz=abs(p.xz)-2.3,
		p.z>p.x?p=p.zyx:p,
		p.z=1.5-abs(p.z-1.3+sin(p.z)*.2),
		p.y>p.x?p=p.yxz:p,
		p.x=3.-abs(p.x-5.+sin(p.x*3.)*.2),
		p.y>p.x?p=p.yxz:p,
		p.y=.9-abs(p.y-.4),
		e=12.*clamp(.3/min(dot(p,p),1.),.0,1.)+
		2.*clamp(.1/min(dot(p,p),1.),.0,1.),
		p=e*p-vec3(7,1,1),
		s*=e;
	return length(p)/s;
}

#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
float deTemple2(vec3 p){
	p=abs(p)-3.;
	if(p.x < p.z)p.xz=p.zx;
	if(p.y < p.z)p.yz=p.zy;
	if(p.x < p.y)p.xy=p.yx;
	float s=2.; vec3 off=p*.5;
	for(int i=0;i<12;i++){
		p=1.-abs(p-1.);
		float k=-1.1*max(1.5/dot(p,p),1.5);
		s*=abs(k); p*=k; p+=off;
		p.zx*=rot(-1.2);
	}
	float a=2.5;
	p-=clamp(p,-a,a);
	return length(p)/s;
}

float deTemple3 ( vec3 pos ) {
	vec3 tpos=pos;
	tpos.xz=abs(.5-mod(tpos.xz,1.));
	vec4 p=vec4(tpos,1.);
	float y=max(0.,.35-abs(pos.y-3.35))/.35;
	for (int i=0; i<7; i++) {
		p.xyz = abs(p.xyz)-vec3(-0.02,1.98,-0.02);
		p=p*(2.0+0.*y)/clamp(dot(p.xyz,p.xyz),.4,1.)-vec4(0.5,1.,0.4,0.);
		p.xz*=mat2(-0.416,-0.91,0.91,-0.416);
	}
	return (length(max(abs(p.xyz)-vec3(0.1,5.0,0.1),vec3(0.0)))-0.05)/p.w;
}

const float raymarchMaxDistance = 50.0f;
const float raymarchUnderstep = 0.9f;
const int raymarchMaxSteps = 300;
const float epsilon = 0.001f;

const vec3 carrot = vec3( 0.713f, 0.170f, 0.026f );
const vec3 honey = vec3( 0.831f, 0.397f, 0.038f );
const vec3 bone = vec3( 0.887f, 0.789f, 0.434f );
const vec3 tire = vec3( 0.023f, 0.023f, 0.023f );
const vec3 sapphire = vec3( 0.670f, 0.764f, 0.855f );
const vec3 nickel = vec3( 0.649f, 0.610f, 0.541f );

vec3 hitColor;
float hitRoughness;
int hitSurfaceType;

float de ( vec3 p ) {
	const vec3 pOriginal = p;
	float sceneDist = 1000.0f;

	hitColor = vec3( 0.0f );
	hitSurfaceType = NONE;
	hitRoughness = 0.0f;

	// form for the following:
	// {
		// const float d = blah whatever SDF
		// sceneDist = min( sceneDist, d );
		// if ( sceneDist == d && d < epsilon ) {
			// set material specifics - hitColor, hitSurfaceType, hitRoughness
		// }
	// }

	{
		const float d = fBox( p, vec3( 0.1f, 100.0f, 0.1f ) );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			hitSurfaceType = EMISSIVE;
			hitColor = vec3( 1.0f );
		}
	}

	{
		const float d = deTemple3( p );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			// hitSurfaceType = MIRROR;
			hitSurfaceType = ( NormalizedRandomFloat() < 0.1f ) ? MIRROR : DIFFUSE;
			hitColor = vec3( 0.95f );
			// hitColor = bone;
		}
	}

	return sceneDist;
}

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

	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 2335;

	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );

	// ray is live, do the raymarch...
	if ( length( direction ) > 0.5f ) {
		// update the intersection info
		const float distanceToHit = raymarch( origin, direction );

		SetHitAlbedo( myState, hitColor );
		SetHitRoughness( myState, hitRoughness );
		SetHitDistance( myState, distanceToHit );
		SetHitMaterial( myState, hitSurfaceType );
		SetHitNormal( myState, SDFNormal( origin + direction * distanceToHit ) );
		SetHitIntersector( myState, ( distanceToHit < raymarchMaxDistance ) ? SDFHIT : NOHIT );

		state[ gl_GlobalInvocationID.x ] = myState;
	}
}