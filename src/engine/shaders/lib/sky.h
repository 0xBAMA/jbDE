// //ad 3d atmospheric scattering
// //self  : https://www.shadertoy.com/view/4lBcRm
// //parent: https://www.shadertoy.com/view/XtGXWV
// //looks like its very cobbled together, i cleaned it up a but
// //but a lot of it is still redundant.

// vec3 lightPosition0 = vec3(1,0,0);

// //this atmospheric scatttering is an O(n*O(m*2)) complexity nested loop
// #define iterScatteringOut  20.
// #define iterScatteringIn   20.

// //#define eps			0.00001
// #define pi				acos(-1.)
// //why would it need pi*4 ????
// #define pit4 			12.566370614359172953850573533118

// //atmospheric scattering parameters
// #define PLANET_RADIUS     6.36
// #define ATMOSPHERE_RADIUS 6.42
// #define H0Rayleigh        .13333
// #define H0Mie             .02
// #define KRayleigh         vec3(.058,.135,.331)
// #define KMie              .21

// //lib.common
// #define dd(a) dot(a,a)
// #define u2(a) ((a)*2.-1.)
// #define u5(a) ((a)*.5+.5)

// //carthesian to sphere (and back)
// vec3 c2s(vec3 v){float r=length(v)
// ;return vec3(r, atan(v.x,v.z),acos(v.y/r));}
// vec3 s2c(vec3 v)
// {return v.x*vec3(sin(v.z)*sin(v.y),cos(v.z),sin(v.z)*cos(v.y));}

// //ray sphere intersection
// vec2 rsiab(vec3 p, vec3 v){return vec2(dd(v),dot(p,v)*2.00001);}
// //2.00001 ommits left shift optimitation issues.
// //return closest intersection of ray and sphere
// vec3 irs(vec3 p,vec3 v,float r){;vec2 a=rsiab(p,v);
// ;return p-v*(a.y+sqrt(a.y*a.y-4.*a.x*(dd(p)-r*r)))*.5/a.x;}
// //return distance to ray-sphere interection(s).
// float irsfd(vec3 p,vec3 v,float r){vec2 a=rsiab(p,v);
// ;return -.5*(a.y+sqrt(a.y*a.y-4.*a.x*dd(p)-r*r))/a.x;}
// vec2 irsfd2(vec3 p,vec3 v,float r){vec2 a=rsiab(p,v);
// ;return (vec2(-1,1)*sqrt(a.y*a.y-4.*a.x*(dd(p)-r*r))-a.y)*.5/a.x;}

// //scat() is an O(n*O(m*2)) complexity nested loop
// //scatS() is an inner loop of scat()
// //and it is iterated over twice
// //,which seems VERY inefficient and optimizable
// //as in, it interpolates over 2 loops
// // , instead of interpolating within the loop
// vec4 scatS(vec3 a,vec3 b,vec4 k,vec2 o)
// {vec3 s=(b-a)/iterScatteringOut,u=a+s*.5;vec2 r=vec2(0)
// ;for(float i=.0;i<iterScatteringOut; ++i
// ){float h=(length(u)-PLANET_RADIUS)/(ATMOSPHERE_RADIUS-PLANET_RADIUS)
//  ;h=max(.0,h);r+=exp(-h/o);u+=s;
// }r*=length(s);return r.xxxy*k*pit4;}

// //a,b,k,h0,lightDir
// vec4 scat(vec3 a,vec3 b,vec4 k,vec2 o,vec3 d)
// {;vec3 s=(b-a)/iterScatteringIn,u=a+s*.5;float stepLength=length(s);
// ;vec4 rr=vec4(0)
// ;for(float i=.0;i<iterScatteringIn;++i
// ){float h=(PLANET_RADIUS-length(u))/(ATMOSPHERE_RADIUS-PLANET_RADIUS)
//  ;rr+=exp(-scatS(u,irs(u,-d,ATMOSPHERE_RADIUS),k,o)
//           -scatS(u,a                          ,k,o)
//          )*exp(min(.0,h)/o).xxxy;u+=s;
// }return rr*stepLength*k;}

// float RayleighPhase(float m){return (3./4.)*(1.+m*m);}

// float MiePhase(float m){const float g=-.99,h=g*g
// ;return (3.*(1.-h)/(2.*(2.+h)))*(1.+m*m)/pow((1.+h-2.*g*m),3./2.);}

// vec3 tldg(vec3 l){float t=-.16+iTime
// ;return normalize(vec3(l.x*cos(t)+l.y*sin(t)
// ,l.x*(-sin(t))+l.y *cos(t),.0));}

// vec4 scatter(vec2 u){u.y=1.-u.y
// ;vec3 s=vec3(1,(u.x*2.-1.)*pi,u.y*pi),d=s2c(s);s=vec3(.0,PLANET_RADIUS,.0)
// ;return 10.*scat(s,s+d*irsfd2(s,d,ATMOSPHERE_RADIUS).y
// ,vec4(KRayleigh,KMie),vec2(H0Rayleigh,H0Mie)
// ,tldg(normalize(lightPosition0.xyz)));}

// float ratio(){return mix(iResolution.x/iResolution.y
//                         ,iResolution.y/iResolution.x
//                    ,step(iResolution.x,iResolution.y));}

// void mainImage( out vec4 o, in vec2 v )
// {vec2 u=v/iResolution.xy
// ;u.y=1.-u.y
// ;vec3 sphericalDirection=vec3(1,(u.x*2.-1.)*pi,u.y*pi)
// ;vec3 d = s2c(sphericalDirection)
// ;vec3 l=normalize(lightPosition0.xyz)//sunDirection
// ;vec3 tld=tldg(l);
// ;vec3 s=vec3(.0,PLANET_RADIUS, .0)
// ;vec3 atmospherePos=s+d*irsfd2(s,d,ATMOSPHERE_RADIUS).y
// ;const float fov=90.;//looks aas if this was cobbled from many souces. 
// ;vec2 w=u2((v+.5)/iResolution.xy)*vec2(ratio(),-1);    
// ;vec2 p=w*tan(fov*pi/360.0);
// ;vec3 rayOrigin=vec3(0); 
// ;vec3 rayDirection=normalize(vec3(p,-1)-rayOrigin)
// //above is equal to Vec3f(w,-1); 
// ;vec2 f=c2s(rayDirection).yz/pi;f.x=u5(f.x);
// ;vec4 c=scatter(f);
// ;float t=-.16 +iTime
// ;vec3 sunPos=vec3(l.x*cos(t)+l.y*sin(t),-l.x*sin(t)+l.y*cos(t),.0)
// ;float m=dot(d,-sunPos)      
// ;o=vec4(c.xyz*RayleighPhase(m)+c.www*MiePhase(m),1.0);
// }


// =================================================================
// ======= ^ Original shader text above this line from olij ^ ======
// link: https://www.shadertoy.com/view/4lBcRm


//ad 3d atmospheric scattering
//self  : https://www.shadertoy.com/view/4lBcRm
//parent: https://www.shadertoy.com/view/XtGXWV
//looks like its very cobbled together, i cleaned it up a but
//but a lot of it is still redundant.

vec3 lightPosition0 = vec3(1,0,0);

//this atmospheric scatttering is an O(n*O(m*2)) complexity nested loop
#define iterScatteringOut  20.
#define iterScatteringIn   20.

//#define eps			0.00001
#define pi				acos(-1.)
//why would it need pi*4 ????
#define pit4 			12.566370614359172953850573533118

//atmospheric scattering parameters
#define PLANET_RADIUS     6.36
#define ATMOSPHERE_RADIUS 6.42
#define H0Rayleigh        .13333
#define H0Mie             .02
#define KRayleigh         vec3(.058,.135,.331)
#define KMie              .21

//lib.common
#define dd(a) dot(a,a)
#define u2(a) ((a)*2.-1.)
#define u5(a) ((a)*.5+.5)

//carthesian to sphere (and back)
vec3 c2s ( vec3 v ) {
	float r = length( v );
	return vec3( r, atan( v.x, v.z ), acos( v.y / r ) );
}

vec3 s2c ( vec3 v ) {
	return v.x * vec3( sin( v.z ) * sin( v.y ), cos( v.z ), sin( v.z ) * cos( v.y ) );
}

//ray sphere intersection
vec2 rsiab ( vec3 p, vec3 v ) {
//2.00001 ommits left shift optimitation issues.
	return vec2( dd( v ), dot( p, v ) * 2.00001f );
}

//return closest intersection of ray and sphere
vec3 irs ( vec3 p, vec3 v, float r ) {
	vec2 a = rsiab( p, v );
	return p - v * ( a.y + sqrt( a.y * a.y - 4.0f * a.x * ( dd( p ) - r * r ) ) ) * 0.5f / a.x;
}

//return distance to ray-sphere interection(s).
float irsfd ( vec3 p, vec3 v, float r ) {
	vec2 a = rsiab( p, v );
	return -0.5f * ( a.y + sqrt( a.y * a.y - 4.0f * a.x * dd( p ) - r * r ) ) / a.x;
}

vec2 irsfd2 ( vec3 p, vec3 v, float r ) {
	vec2 a = rsiab( p, v );
	return ( vec2( -1.0f, 1.0f ) * sqrt( a.y * a.y - 4.0f * a.x * ( dd( p ) - r * r ) ) - a.y ) * 0.5f / a.x;
}

//scat() is an O(n*O(m*2)) complexity nested loop
//scatS() is an inner loop of scat()
//and it is iterated over twice
//,which seems VERY inefficient and optimizable
//as in, it interpolates over 2 loops
// , instead of interpolating within the loop
vec4 scatS ( vec3 a, vec3 b, vec4 k, vec2 o ) {
	vec3 s = ( b - a ) / iterScatteringOut, u = a + s * 0.5f;
	vec2 r = vec2( 0.0f );
	for ( float i = 0.0f; i < iterScatteringOut; ++i ) {
		float h = ( length( u ) - PLANET_RADIUS ) / ( ATMOSPHERE_RADIUS - PLANET_RADIUS );
		h = max( 0.0f, h );
		r += exp( -h / o );
		u += s;
	}
	r *= length( s );
	return r.xxxy * k * pit4;
}

//a,b,k,h0,lightDir
vec4 scat ( vec3 a, vec3 b, vec4 k, vec2 o, vec3 d ) {
	vec3 s = ( b - a ) / iterScatteringIn, u = a + s * 0.5f;
	float stepLength = length( s );
	vec4 rr = vec4( 0.0f );
	for ( float i = 0.0f; i < iterScatteringIn; ++i ) {
		float h = ( PLANET_RADIUS - length( u ) ) / ( ATMOSPHERE_RADIUS - PLANET_RADIUS );
		rr += exp(-scatS( u, irs( u, -d, ATMOSPHERE_RADIUS ), k, o ) - scatS( u, a, k, o ) ) * exp( min( 0.0f, h ) / o ).xxxy;
		u +=s;
	}
	return rr * stepLength * k;
}

float RayleighPhase ( float m ) {
	return ( 3.0f / 4.0f ) * ( 1.0f + m * m );
}
float MiePhase ( float m ) { 
	const float g = -0.99f, h = g * g;
	return ( 3.0f * ( 1.0f - h ) / ( 2.0f * ( 2.0f + h ) ) ) * ( 1.0f + m * m ) / pow( ( 1.0f + h - 2.0f * g * m ), 3.0f / 2.0f );
}

vec3 tldg ( vec3 l, float t ) {
	return normalize( vec3( l.x * cos( t ) + l.y * sin( t ), l.x * ( -sin( t ) ) + l.y * cos( t ), 0.0f ) );
}

// from here, down, makes some assumptions about the uniform environment available to the shader ( skyTime, skyCache )

vec4 scatter ( vec2 u ) {
	u.y = 1.0f - u.y;
	vec3 s = vec3( 1.0f, ( u.x * 2.0f - 1.0f ) * pi, u.y * pi ), d = s2c( s );
	s = vec3( 0.0f, PLANET_RADIUS, 0.0 );
	return 10.0f * scat( s, s + d * irsfd2( s, d, ATMOSPHERE_RADIUS ).y, vec4( KRayleigh, KMie ), vec2( H0Rayleigh, H0Mie ), tldg( normalize( lightPosition0.xyz ), skyTime ) );
}

float ratio () {
	vec2 res = vec2( imageSize( skyCache ).xy );
	return mix( res.x / res.y ,res.y / res.x, step( res.x, res.y ) );
}

vec3 skyColor () {
	vec2 uv = vec2( gl_GlobalInvocationID.xy ) / imageSize( skyCache ).xy;
	uv.y = 1.0f - uv.y;
	vec3 sphericalDirection = vec3( 1.0f, ( uv.x * 2.0f - 1.0f ) * pi, uv.y * pi );
	vec3 d = s2c( sphericalDirection );
	vec3 l = normalize( lightPosition0.xyz ); //sunDirection
	vec3 s = vec3( 0.0f, PLANET_RADIUS, 0.0f );
	const float fov = 90.0f; //looks as if this was cobbled from many sources - two, different spherical mappings
	vec2 w = u2( ( vec2( gl_GlobalInvocationID.xy ) + 0.5f ) / imageSize( skyCache ).xy ) * vec2( ratio(), -1.0f );
	vec2 p = w * tan( fov * pi / 360.0f );
	vec3 rayOrigin = vec3( 0.0f );
	vec3 rayDirection = normalize( vec3( p, -1 ) - rayOrigin );
	vec2 f = c2s( rayDirection ).yz / pi;
	f.x = u5( f.x );
	vec4 c = scatter( f );
	vec3 sunPos = vec3( l.x * cos( skyTime ) + l.y * sin( skyTime ), -l.x * sin( skyTime ) + l.y * cos( skyTime ), 0.0f );
	float m = dot( d, -sunPos );
	return c.xyz * RayleighPhase( m ) + c.www * MiePhase( m );
}
