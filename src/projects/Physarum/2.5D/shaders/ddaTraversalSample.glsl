// from https://www.shadertoy.com/view/7sdSzH


// https://www.shadertoy.com/view/7sdSzH Cast Voxels March Sub-Objects 2
// Extending https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting by fb39ca4
// (with loop optimization by kzy), DDA based on http://lodev.org/cgtutor/raycasting.html
// by adding raymarched subobjects & reconstructing bounding-box hit-positions from DDA (jt).

// tags: 3d, raymarching, raycasting, voxel, dda, textured, subobjects, fork

// Warning:
// I suspect there are a few corner-cases where the bounding-distances are wrong,
// resulting the shader to stall when looking at the grid from certain angles.
// (Added maximal iterations counter - does that fix this issue?)

float sdSphere(vec3 p, float d)
{
    return length(p) - d;
}

float sdBox( vec3 p, vec3 b )
{
    vec3 d = abs(p) - b;
    return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

float sdMixed(vec3 p, int id)
{
    if(id == 0) return sdSphere(p, 0.5);
    if(id == 1) return sdBox(p, vec3(0.4));
    return 0.0;
}

bool getVoxel(ivec3 c) {
    vec3 p = vec3(c) + vec3(0.5);
    float d = max(-sdSphere(p, 7.5), sdBox(p, vec3(6.0)));
    //float d = min(max(-sdSphere(p, 7.5), sdBox(p, vec3(6.0))), -sdSphere(p, 25.0));
    return d < 0.0;
}

#define PI 3.1415926

float checker(vec3 p)
{
    //return step(0.5, length(1.0 - abs(2.0 * fract(p) - 1.0))); // dots
    return step(0.0, sin(PI * p.x + PI/2.0)*sin(PI *p.y + PI/2.0)*sin(PI *p.z + PI/2.0));
    //return step(0.0, sin(p.x)*sin(p.y)*sin(p.z));
}

mat2 rotate(float t)
{
    return mat2(vec2(cos(t), sin(t)), vec2(-sin(t), cos(t)));
}

#define MAX_ITER 200u
#define MAX_DIST 1000.0
#define EPSILON 0.001

// raymarch subobject
float march(vec3 ro, vec3 rd, float tmin, float tmax, int id)
{
    uint i;
    float t;
    for(i = 0u, t = tmin; i < MAX_ITER, t < tmax; i++)
    {
        float h = sdMixed(ro + rd * t, id);
        if(h < EPSILON)
            return t;
        t += h;
    }
    return MAX_DIST;
}

// https://iquilezles.org/articles/normalsSDF tetrahedron normals
vec3 normal( vec3 p, int id )
{
    const float h = EPSILON;
    const vec2 k = vec2(1,-1);
    return normalize(k.xyy * sdMixed(p + k.xyy * h, id) +
                     k.yyx * sdMixed(p + k.yyx * h, id) +
                     k.yxy * sdMixed(p + k.yxy * h, id) +
                     k.xxx * sdMixed(p + k.xxx * h, id));
}

vec4 process_subobject(vec3 ro, vec3 rd, float tmin, float tmax, int id)
{
    float d = march(ro, rd, tmin, tmax, id);
    vec3 n = normal(ro + rd * d, id);
    return vec4(n, d);
}

// "The raycasting code is somewhat based around a 2D raycasting toutorial found here:
//  http://lodev.org/cgtutor/raycasting.html" (fb39ca4)

#define MAX_RAY_STEPS 64

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 screenPos = 2.0 * fragCoord.xy / iResolution.xy - 1.0;
    screenPos.x *= iResolution.x / iResolution.y;
    vec3 rayDir = vec3(screenPos.x, screenPos.y, 2.0);
    vec3 rayPos = vec3(0.0, 0.0,-20.0);

    mat2 S = mat2(vec2(0.0, 1.0), vec2(-1.0, 0.0));
    rayPos.yz = S * rayPos.yz;
    rayDir.yz = S * rayDir.yz;

    mat2 R = rotate(iTime / 5.0);
    rayPos.xy = R * rayPos.xy;
    rayDir.xy = R * rayDir.xy;
    
    rayDir = normalize(rayDir);

    vec3 color = vec3(0.0);
    vec4 result = vec4(vec3(0.0), MAX_DIST);

    {
        vec3 deltaDist = 1.0 / abs(rayDir);
        ivec3 rayStep = ivec3(sign(rayDir));
        bvec3 mask0 = bvec3(false);
        ivec3 mapPos0 = ivec3(floor(rayPos + 0.));
        vec3 sideDist0 = (sign(rayDir) * (vec3(mapPos0) - rayPos) + (sign(rayDir) * 0.5) + 0.5) * deltaDist;

        //for (int i = 0; i < MAX_RAY_STEPS; i++)
        for (int i = min(iFrame,0); i < MAX_RAY_STEPS; i++) // prevent unrolling loop to avoid crash on windows
        {
            // Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
            bvec3 mask1 = lessThanEqual(sideDist0.xyz, min(sideDist0.yzx, sideDist0.zxy));
            vec3 sideDist1 = sideDist0 + vec3(mask1) * deltaDist;
            ivec3 mapPos1 = mapPos0 + ivec3(vec3(mask1)) * rayStep;

            if (getVoxel(mapPos0))
            {
                // reconstruct near & far bounding-box hit-positions from previous & current state of DDA.
                //vec2 bounds = vec2(length(vec3(mask0) * (sideDist0 - deltaDist)) / length(rayDir), length(vec3(mask1) * (sideDist1 - deltaDist)) / length(rayDir)); // rayDir not normalized
                vec2 bounds = vec2(length(vec3(mask0) * (sideDist0 - deltaDist)), length(vec3(mask1) * (sideDist1 - deltaDist))); // rayDir already normalized
                if(min(abs(mapPos0.x), min(abs(mapPos0.y),abs(mapPos0.z))) > 5) // explicit sky-box (letting loop run-out without hitting a wall causes blocky artifacts)
                    break;
                //result = process_subobject(rayPos - vec3(mapPos0) - vec3(0.5), rayDir, bounds.x, bounds.y, i > 10 ? 1 : 0);
                result = process_subobject(rayPos - vec3(mapPos0) - vec3(0.5), rayDir, bounds.x, bounds.y, texture(iChannel0, 0.1 * vec3(mapPos0)).x < 0.5 ? 0 : 1);
                if(result.w > 0.0 && result.w < bounds.y)
                {
                    color = vec3(1.0);
                    //color *= 0.5 + 0.5 * normalize(result.xyz);
                    vec3 dst = rayPos + rayDir * result.w;
                    //color *= texture(iChannel0, dst).xyz;
                    color *= vec3(0.5 + 0.5 * checker(dst));

                    vec3 fogcolor = vec3(0.25, 0.4, 0.5); // fog
                    //vec3 fogcolor = vec3(0.75, 0.6, 0.3); // smog
                    color *= mix(fogcolor, color, exp(-result.w * result.w / 200.0)); // fog for depth impression & to suppress flickering

                    break;
                }
            }
            
            sideDist0 = sideDist1;
            mapPos0 = mapPos1;
        }
    }

    vec3 ambient = vec3(0.1);
    vec3 lightdir = normalize(vec3(3.0, 2.0, 1.0));
    color *= mix(ambient, vec3(1.0), clamp(dot(lightdir, result.xyz), 0.0, 1.0));

    fragColor.rgb = sqrt(color);
}
