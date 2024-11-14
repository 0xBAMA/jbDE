#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// ray state buffer
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

#include "random.h"
#include "hg_sdf.glsl"
#include "colorRamps.glsl.h"
#include "twigl.glsl"

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

float escape = 0.0f;
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
	float Offset = -3.036726f + p.x / 10.0f;
	int EnableOffset = 1;
	int Iterations = 80;
	float Precision = 1.0f;
	// output _sdf c = _SDFDEF)

	escape = 0.0f;

	float u2 = 1;
	float v2 = 1;
	if( EnableOffset != 0)
		p = Offset+abs(vec3(p.x,p.y,p.z));

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

		escape += exp( -0.2f * dot( p, p ) );
	}

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

float deGround ( vec3 p ) {
	float i,j,d=1.,a;
	vec3 Q;
	a=1.;
	d=p.y+1.;
	for(j=0.;j++<9.;)
		Q=(p+fract(sin(j)*1e4)*3.141592)*a,
		Q+=sin(Q)*2.,
		d+=sin(Q.x)*sin(Q.z)/a,
		a*=2.;
	return d*.15;
}

float deTree(vec3 p){
	float d, a;
	d=a=1.;
	for(int j=0;j++<14;)
		p.xz=abs(p.xz)*rotate2D(PI/4.),
		d=min(d,max(length(p.zx)-.3,p.y-.4)/a),
		p.yx*=rotate2D(.5),
		p.y-=3.,
		p*=1.5,
		a*=1.5;
	return d;
}

float deTreeFoliage(vec3 p){
	float d, a;
	d=a=1.;
	for(int j=0;j++<19;)
		p.xz=abs(p.xz)*rotate2D(PI/4.),
		d=min(d,max(length(p.zx)-.3,p.y-.4)/a),
		p.yx*=rotate2D(.5),
		p.y-=3.,
		p*=1.5,
		a*=1.5;
	return d;
}

mat2 rot2(in float a){ float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float deGyroid ( vec3 p ) {
	float d = 1e5;
	const int n = 3;
	const float fn = float(n);
	for(int i = 0; i < n; i++){
		vec3 q = p;
		float a = float(i)*fn*2.422; //*6.283/fn
		a *= a;
		q.z += float(i)*float(i)*1.67; //*3./fn
		q.xy *= rot2(a);
		float b = (length(length(sin(q.xy) + cos(q.yz))) - .15);
		float f = max(0., 1. - abs(b - d));
		d = min(d, b) - .25*f*f;
	}
	return d;
}

const float raymarchMaxDistance = 50.0f;
const float raymarchUnderstep = 0.9f;
const int raymarchMaxSteps = 300;
const float epsilon = 0.001f;

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

	// {
	// 	// const float d = fBox( p, vec3( 0.1f, 100.0f, 0.1f ) );
	// 	// const float d = fCylinder( p - vec3( 0.0f, 0.0f, 0.0f ), 0.1f, 100.0f );
	// 	const float d = fCylinder( p.zyx - vec3( 0.0f, 0.0f, 0.0f ), 0.1f, 100.0f );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		hitSurfaceType = EMISSIVE;
	// 		hitColor = vec3( 3.0f );
	// 	}
	// }

	// {
	// 	const float scale = 0.2f;
	// 	// const float d = max( fBox( p, vec3( 3.0f ) ), DEnew( p ) );
	// 	const float d = deTemp( p.xzy * scale ) / scale;
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		hitSurfaceType = ( NormalizedRandomFloat() < 0.1f ) ? MIRROR : DIFFUSE;
	// 		// hitSurfaceType = DIFFUSE;
	// 		hitColor = bone;
	// 	}
	// }

	{
		pModInterval1( p.z, 0.8f, -10.0f, 0.0f );
		pModInterval1( p.x, 0.8f, -10.0f, 0.0f );

		const float scale = 1.0f;
		// const float d = max( deTrees( p * scale - vec3( 0.0f, 10.0f, 0.0f ) * scale ) / scale, fBox( p, vec3( 4.0f ) ) );
		// const float d = deTrees( p * scale - vec3( 0.0f, 5.0f, 10.0f ) * scale ) / scale;
		// const float d = deTrees( p * scale ) / scale;
		// const float d = max( deSDFSDFe( p * scale + vec3( 0.4, 0.1, 2.0f ) ) / scale, fBox( p, vec3( 5.0f, 0.5f, 10.8f ) ) );
		const float d = deTree( p * scale ) / scale;
		const float d2 = deTreeFoliage( p * scale ) / scale;

		const float dCombined = min( d, d2 );

		sceneDist = min( sceneDist, dCombined );
		if ( sceneDist == dCombined && dCombined < epsilon ) {
			// hitSurfaceType = ( NormalizedRandomFloat() < 0.1f ) ? MIRROR : DIFFUSE;

			// // if ( escape > 0.58f ) {
			// if ( escape > 0.45f ) {
			// 	// hitColor = mix( carrot, bone, ( escape - 0.45f ) * 2.0f );
			// 	hitColor = mix( carrot, vec3( 0.4f, 0.0f, 0.0f ), ( escape - 0.45f ) * 2.0f );
			// 	hitSurfaceType = EMISSIVE;
			// } else {
				// hitSurfaceType = DIFFUSE;
				// hitColor = ( escape < 0.01f ) ? vec3( 1.0f ) : mix( carrot, bone, 0.618f ).rgb * saturate( escape ) * 2.6f;
			// }

			if ( dCombined == d ) {
				hitSurfaceType = MIRROR;
				hitColor = vec3( 0.918f );
			} else {
				hitSurfaceType = MIRROR;
				hitColor = carrot.grb * 0.1f;
			}
		}
	}

	// {
	// 	const float d = fBox
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// 
	// 	}
	// }

	// {
	// 	const float scale = 0.8f;
	// 	const float d = max( deGyroid( p * scale ) / scale, fBox( p, vec3( 10.0f, 3.0f, 12.0f ) ) );
	// 	// const float d = deTemp( p * scale ) / scale;
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		// hitSurfaceType = ( NormalizedRandomFloat() < 0.1f ) ? MIRROR : DIFFUSE;
	// 		hitSurfaceType = MIRROR;
	// 		// hitColor = vec3( 0.95f );
	// 		hitColor = mix( carrot, bone, 0.618f ) * 0.1f;
	// 		// hitColor = tungsten;
	// 	}
	// }

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

		// if ( distanceToHit <= GetHitDistance( myState ) &&  ) {
			SetHitAlbedo( myState, hitColor );
			SetHitRoughness( myState, hitRoughness );
			SetHitDistance( myState, distanceToHit );
			SetHitMaterial( myState, hitSurfaceType );
			SetHitNormal( myState, SDFNormal( origin + direction * distanceToHit ) );
			SetHitIntersector( myState, ( distanceToHit < raymarchMaxDistance ) ? SDFHIT : NOHIT );
		// }

		state[ gl_GlobalInvocationID.x ] = myState;
	}
}