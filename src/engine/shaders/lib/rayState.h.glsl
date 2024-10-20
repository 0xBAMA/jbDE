// rayState_t setup for Icarus

// intersection types
#define TERMINATED	-1
#define NOHIT		0
#define SDFHIT		1

// material types
#define NONE	0
#define DIFFUSE 1

// total 104 bytes... pad to 128? tbd
struct rayState_t {
	vec4 data1; // ray origin in .xyz, distance to hit in .w
	vec4 data2; // ray direction in .xyz, type of material in .w
	vec4 data3; // throughput in .xyz, type of intersector in .w
	vec4 data4; // energy total in .xyz, frontface in .w
	vec4 data5; // normal vector in .xyz, IoR in .w
	vec4 data6; // albedo of hit in .xyz, roughness in .w
	ivec2 data7; // pixel index
};

void SetRayOrigin 		( inout rayState_t rayState, vec3 origin )		{ rayState.data1.xyz = origin; }
vec3 GetRayOrigin		( rayState_t rayState )							{ return rayState.data1.xyz; }

void SetHitDistance		( inout rayState_t rayState, float dist )		{ rayState.data1.w = dist; }
float GetHitDistance	( rayState_t rayState )							{ return rayState.data1.w; }

void SetRayDirection	( inout rayState_t rayState, vec3 direction )	{ rayState.data2.xyz = direction; }
vec3 GetRayDirection	( rayState_t rayState )							{ return rayState.data2.xyz; }

void SetHitMaterial		( inout rayState_t rayState, int material )		{ rayState.data2.w = float( material ); }
int GetHitMaterial		( rayState_t rayState )							{ return int( rayState.data2.w ); }

void SetThroughput		( inout rayState_t rayState, vec3 throughput )	{ rayState.data3.xyz = throughput; }
vec3 GetThroughput		( rayState_t rayState )							{ return rayState.data3.xyz; }

void SetHitIntersector	( inout rayState_t rayState, int intersector )	{ rayState.data3.w = float( intersector ); }
int GetHitIntersector	( rayState_t rayState )							{ return int( rayState.data3.w ); }

void SetEnergyTotal 	( inout rayState_t rayState, vec3 energy )		{ rayState.data4.xyz = energy; }
vec3 GetEnergyTotal		( rayState_t rayState )							{ return rayState.data4.xyz; }

void SetHitFrontface 	( inout rayState_t rayState, bool frontface )	{ rayState.data4.w = frontface ? 1.0f : 0.0f; }
bool GetHitFrontface	( rayState_t rayState )							{ return ( rayState.data4.w == 1.0f ); }

void SetHitNormal 		( inout rayState_t rayState, vec3 normal )		{ rayState.data5.xyz = normal; }
vec3 GetHitNormal		( rayState_t rayState )							{ return rayState.data5.xyz; }

void SetHitIoR 			( inout rayState_t rayState, float IoR )		{ rayState.data5.w = IoR; }
float GetHitIoR			( rayState_t rayState )							{ return rayState.data5.w; }

void SetHitAlbedo 		( inout rayState_t rayState, vec3 albedo )		{ rayState.data6.xyz = albedo; }
vec3 GetHitAlbedo		( rayState_t rayState )							{ return rayState.data6.xyz; }

void SetHitRoughness	( inout rayState_t rayState, float roughness )	{ rayState.data6.w = roughness; }
float GetHitRoughness	( rayState_t rayState )							{ return rayState.data6.w; }

void SetPixelIndex		( inout rayState_t rayState, ivec2 pixelIndex )	{ rayState.data7 = pixelIndex; }
ivec2 GetPixelIndex		( rayState_t rayState )							{ return rayState.data7; }

void StateReset ( inout rayState_t rayState ) {
	rayState.data1 = rayState.data2 = rayState.data3 = rayState.data4 = rayState.data5 = rayState.data6 = vec4( 0.0f );
	rayState.data7 = ivec2( 0 );

	// need saner defaults...
	SetThroughput( rayState, vec3( 1.0f ) );
}