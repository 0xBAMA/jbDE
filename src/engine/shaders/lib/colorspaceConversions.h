// this could do with a cleanup pass at some point in the future


// Define saturation macro, if not already user-defined
#ifndef saturate
#define saturate(v) clamp(v, 0, 1)
#endif

// Constants
const float HCV_EPSILON = 1e-10;
const float HSL_EPSILON = 1e-10;
const float HCY_EPSILON = 1e-10;

const float SRGB_GAMMA = 1.0 / 2.2;
const float SRGB_INVERSE_GAMMA = 2.2;
const float SRGB_ALPHA = 0.055;

// Used to convert from linear RGB to XYZ space
const mat3 RGB_2_XYZ = (mat3(
	0.4124564, 0.2126729, 0.0193339,
	0.3575761, 0.7151522, 0.1191920,
	0.1804375, 0.0721750, 0.9503041
));

// Used to convert from XYZ to linear RGB space
const mat3 XYZ_2_RGB = (mat3(
	 3.2404542,-0.9692660, 0.0556434,
	-1.5371385, 1.8760108,-0.2040259,
	-0.4985314, 0.0415560, 1.0572252
));

const vec3 LUMA_COEFFS = vec3(0.2126, 0.7152, 0.0722);

// Returns the luminance of a !! linear !! rgb color
float get_luminance(vec3 rgb) {
	return dot(LUMA_COEFFS, rgb);
}

// Converts a linear rgb color to a srgb color (approximated, but fast)
vec3 rgb_to_srgb_approx(vec3 rgb) {
	return pow(rgb, vec3(SRGB_GAMMA));
}

// Converts a srgb color to a rgb color (approximated, but fast)
vec3 srgb_to_rgb_approx(vec3 srgb) {
	return pow(srgb, vec3(SRGB_INVERSE_GAMMA));
}

// Converts a single linear channel to srgb
float linear_to_srgb(float channel) {
	if(channel <= 0.0031308)
		return 12.92 * channel;
	else
		return (1.0 + SRGB_ALPHA) * pow(channel, 1.0/2.4) - SRGB_ALPHA;
}

// Converts a single srgb channel to rgb
float srgb_to_linear(float channel) {
	if (channel <= 0.04045)
		return channel / 12.92;
	else
		return pow((channel + SRGB_ALPHA) / (1.0 + SRGB_ALPHA), 2.4);
}

// Converts a linear rgb color to a srgb color (exact, not approximated)
vec3 rgb_to_srgb(vec3 rgb) {
	return vec3(
		linear_to_srgb(rgb.r),
		linear_to_srgb(rgb.g),
		linear_to_srgb(rgb.b)
	);
}

// Converts a srgb color to a linear rgb color (exact, not approximated)
vec3 srgb_to_rgb(vec3 srgb) {
	return vec3(
		srgb_to_linear(srgb.r),
		srgb_to_linear(srgb.g),
		srgb_to_linear(srgb.b)
	);
}

// Converts a color from linear RGB to XYZ space
vec3 rgb_to_xyz(vec3 rgb) {
	return RGB_2_XYZ * rgb;
}

// Converts a color from XYZ to linear RGB space
vec3 xyz_to_rgb(vec3 xyz) {
	return XYZ_2_RGB * xyz;
}

// Converts a color from XYZ to xyY space (Y is luminosity)
vec3 xyz_to_xyY(vec3 xyz) {
	float Y = xyz.y;
	float x = xyz.x / (xyz.x + xyz.y + xyz.z);
	float y = xyz.y / (xyz.x + xyz.y + xyz.z);
	return vec3(x, y, Y);
}

// Converts a color from xyY space to XYZ space
vec3 xyY_to_xyz(vec3 xyY) {
	float Y = xyY.z;
	float x = Y * xyY.x / xyY.y;
	float z = Y * (1.0 - xyY.x - xyY.y) / xyY.y;
	return vec3(x, Y, z);
}

// Converts a color from linear RGB to xyY space
vec3 rgb_to_xyY(vec3 rgb) {
	vec3 xyz = rgb_to_xyz(rgb);
	return xyz_to_xyY(xyz);
}

// Converts a color from xyY space to linear RGB
vec3 xyY_to_rgb(vec3 xyY) {
	vec3 xyz = xyY_to_xyz(xyY);
	return xyz_to_rgb(xyz);
}

// Converts a value from linear RGB to HCV (Hue, Chroma, Value)
vec3 rgb_to_hcv(vec3 rgb) {
	// Based on work by Sam Hocevar and Emil Persson
	vec4 P = (rgb.g < rgb.b) ? vec4(rgb.bg, -1.0, 2.0/3.0) : vec4(rgb.gb, 0.0, -1.0/3.0);
	vec4 Q = (rgb.r < P.x) ? vec4(P.xyw, rgb.r) : vec4(rgb.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + HCV_EPSILON) + Q.z);
	return vec3(H, C, Q.x);
}

// Converts from pure Hue to linear RGB
vec3 hue_to_rgb(float hue) {
	float R = abs(hue * 6 - 3) - 1;
	float G = 2 - abs(hue * 6 - 2);
	float B = 2 - abs(hue * 6 - 4);
	return saturate(vec3(R,G,B));
}

// Converts from HSV to linear RGB
vec3 hsv_to_rgb(vec3 hsv) {
	vec3 rgb = hue_to_rgb(hsv.x);
	return ((rgb - 1.0) * hsv.y + 1.0) * hsv.z;
}

// Converts from HSL to linear RGB
vec3 hsl_to_rgb(vec3 hsl) {
	vec3 rgb = hue_to_rgb(hsl.x);
	float C = (1 - abs(2 * hsl.z - 1)) * hsl.y;
	return (rgb - 0.5) * C + hsl.z;
}

// Converts from HCY to linear RGB
vec3 hcy_to_rgb(vec3 hcy) {
	const vec3 HCYwts = vec3(0.299, 0.587, 0.114);
	vec3 RGB = hue_to_rgb(hcy.x);
	float Z = dot(RGB, HCYwts);
	if (hcy.z < Z) {
		hcy.y *= hcy.z / Z;
	} else if (Z < 1) {
		hcy.y *= (1 - hcy.z) / (1 - Z);
	}
	return (RGB - Z) * hcy.y + hcy.z;
}


// Converts from linear RGB to HSV
vec3 rgb_to_hsv(vec3 rgb) {
	vec3 HCV = rgb_to_hcv(rgb);
	float S = HCV.y / (HCV.z + HCV_EPSILON);
	return vec3(HCV.x, S, HCV.z);
}

// Converts from linear rgb to HSL
vec3 rgb_to_hsl(vec3 rgb) {
	vec3 HCV = rgb_to_hcv(rgb);
	float L = HCV.z - HCV.y * 0.5;
	float S = HCV.y / (1 - abs(L * 2 - 1) + HSL_EPSILON);
	return vec3(HCV.x, S, L);
}

// Converts from rgb to hcy (Hue, Chroma, Luminance)
vec3 rgb_to_hcy(vec3 rgb) {
	const vec3 HCYwts = vec3(0.299, 0.587, 0.114);
	// Corrected by David Schaeffer
	vec3 HCV = rgb_to_hcv(rgb);
	float Y = dot(rgb, HCYwts);
	float Z = dot(hue_to_rgb(HCV.x), HCYwts);
	if (Y < Z) {
		HCV.y *= Z / (HCY_EPSILON + Y);
	} else {
		HCV.y *= (1 - Z) / (HCY_EPSILON + 1 - Y);
	}
	return vec3(HCV.x, HCV.y, Y);
}

// RGB to YCbCr, ranges [0, 1]
vec3 rgb_to_ycbcr(vec3 rgb) {
	float y = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
	float cb = (rgb.b - y) * 0.565;
	float cr = (rgb.r - y) * 0.713;
	return vec3(y, cb, cr);
}

// YCbCr to RGB
vec3 ycbcr_to_rgb(vec3 yuv) {
	return vec3(
		yuv.x + 1.403 * yuv.z,
		yuv.x - 0.344 * yuv.y - 0.714 * yuv.z,
		yuv.x + 1.770 * yuv.y
	);
}

// Fill out the matrix of conversions by converting to rgb first and
// then to the desired color space.

// To srgb
vec3 xyz_to_srgb(vec3 xyz)  { return rgb_to_srgb(xyz_to_rgb(xyz)); }
vec3 xyY_to_srgb(vec3 xyY)  { return rgb_to_srgb(xyY_to_rgb(xyY)); }
vec3 hue_to_srgb(float hue) { return rgb_to_srgb(hue_to_rgb(hue)); }
vec3 hsv_to_srgb(vec3 hsv)  { return rgb_to_srgb(hsv_to_rgb(hsv)); }
vec3 hsl_to_srgb(vec3 hsl)  { return rgb_to_srgb(hsl_to_rgb(hsl)); }
vec3 hcy_to_srgb(vec3 hcy)  { return rgb_to_srgb(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_srgb(vec3 yuv){ return rgb_to_srgb(ycbcr_to_rgb(yuv)); }

// To xyz
vec3 srgb_to_xyz(vec3 srgb) { return rgb_to_xyz(srgb_to_rgb(srgb)); }
vec3 hue_to_xyz(float hue)  { return rgb_to_xyz(hue_to_rgb(hue)); }
vec3 hsv_to_xyz(vec3 hsv)   { return rgb_to_xyz(hsv_to_rgb(hsv)); }
vec3 hsl_to_xyz(vec3 hsl)   { return rgb_to_xyz(hsl_to_rgb(hsl)); }
vec3 hcy_to_xyz(vec3 hcy)   { return rgb_to_xyz(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_xyz(vec3 yuv) { return rgb_to_xyz(ycbcr_to_rgb(yuv)); }

// To xyY
vec3 srgb_to_xyY(vec3 srgb) { return rgb_to_xyY(srgb_to_rgb(srgb)); }
vec3 hue_to_xyY(float hue)  { return rgb_to_xyY(hue_to_rgb(hue)); }
vec3 hsv_to_xyY(vec3 hsv)   { return rgb_to_xyY(hsv_to_rgb(hsv)); }
vec3 hsl_to_xyY(vec3 hsl)   { return rgb_to_xyY(hsl_to_rgb(hsl)); }
vec3 hcy_to_xyY(vec3 hcy)   { return rgb_to_xyY(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_xyY(vec3 yuv) { return rgb_to_xyY(ycbcr_to_rgb(yuv)); }

// To HCV
vec3 srgb_to_hcv(vec3 srgb) { return rgb_to_hcv(srgb_to_rgb(srgb)); }
vec3 xyz_to_hcv(vec3 xyz)   { return rgb_to_hcv(xyz_to_rgb(xyz)); }
vec3 xyY_to_hcv(vec3 xyY)   { return rgb_to_hcv(xyY_to_rgb(xyY)); }
vec3 hue_to_hcv(float hue)  { return rgb_to_hcv(hue_to_rgb(hue)); }
vec3 hsv_to_hcv(vec3 hsv)   { return rgb_to_hcv(hsv_to_rgb(hsv)); }
vec3 hsl_to_hcv(vec3 hsl)   { return rgb_to_hcv(hsl_to_rgb(hsl)); }
vec3 hcy_to_hcv(vec3 hcy)   { return rgb_to_hcv(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_hcv(vec3 yuv) { return rgb_to_hcy(ycbcr_to_rgb(yuv)); }

// To HSV
vec3 srgb_to_hsv(vec3 srgb) { return rgb_to_hsv(srgb_to_rgb(srgb)); }
vec3 xyz_to_hsv(vec3 xyz)   { return rgb_to_hsv(xyz_to_rgb(xyz)); }
vec3 xyY_to_hsv(vec3 xyY)   { return rgb_to_hsv(xyY_to_rgb(xyY)); }
vec3 hue_to_hsv(float hue)  { return rgb_to_hsv(hue_to_rgb(hue)); }
vec3 hsl_to_hsv(vec3 hsl)   { return rgb_to_hsv(hsl_to_rgb(hsl)); }
vec3 hcy_to_hsv(vec3 hcy)   { return rgb_to_hsv(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_hsv(vec3 yuv) { return rgb_to_hsv(ycbcr_to_rgb(yuv)); }

// To HSL
vec3 srgb_to_hsl(vec3 srgb) { return rgb_to_hsl(srgb_to_rgb(srgb)); }
vec3 xyz_to_hsl(vec3 xyz)   { return rgb_to_hsl(xyz_to_rgb(xyz)); }
vec3 xyY_to_hsl(vec3 xyY)   { return rgb_to_hsl(xyY_to_rgb(xyY)); }
vec3 hue_to_hsl(float hue)  { return rgb_to_hsl(hue_to_rgb(hue)); }
vec3 hsv_to_hsl(vec3 hsv)   { return rgb_to_hsl(hsv_to_rgb(hsv)); }
vec3 hcy_to_hsl(vec3 hcy)   { return rgb_to_hsl(hcy_to_rgb(hcy)); }
vec3 ycbcr_to_hsl(vec3 yuv) { return rgb_to_hsl(ycbcr_to_rgb(yuv)); }

// To HCY
vec3 srgb_to_hcy(vec3 srgb) { return rgb_to_hcy(srgb_to_rgb(srgb)); }
vec3 xyz_to_hcy(vec3 xyz)   { return rgb_to_hcy(xyz_to_rgb(xyz)); }
vec3 xyY_to_hcy(vec3 xyY)   { return rgb_to_hcy(xyY_to_rgb(xyY)); }
vec3 hue_to_hcy(float hue)  { return rgb_to_hcy(hue_to_rgb(hue)); }
vec3 hsv_to_hcy(vec3 hsv)   { return rgb_to_hcy(hsv_to_rgb(hsv)); }
vec3 hsl_to_hcy(vec3 hsl)   { return rgb_to_hcy(hsl_to_rgb(hsl)); }
vec3 ycbcr_to_hcy(vec3 yuv) { return rgb_to_hcy(ycbcr_to_rgb(yuv)); }

// YCbCr
vec3 srgb_to_ycbcr(vec3 srgb) { return rgb_to_ycbcr(srgb_to_rgb(srgb)); }
vec3 xyz_to_ycbcr(vec3 xyz)   { return rgb_to_ycbcr(xyz_to_rgb(xyz)); }
vec3 xyY_to_ycbcr(vec3 xyY)   { return rgb_to_ycbcr(xyY_to_rgb(xyY)); }
vec3 hue_to_ycbcr(float hue)  { return rgb_to_ycbcr(hue_to_rgb(hue)); }
vec3 hsv_to_ycbcr(vec3 hsv)   { return rgb_to_ycbcr(hsv_to_rgb(hsv)); }
vec3 hsl_to_ycbcr(vec3 hsl)   { return rgb_to_ycbcr(hsl_to_rgb(hsl)); }
vec3 hcy_to_ycbcr(vec3 hcy)   { return rgb_to_ycbcr(hcy_to_rgb(hcy)); }

// end tobspr color space conversions

//----------------------------------------------------------------------------
// these come from: https://www.shadertoy.com/view/4dcSRN

// YUV, generic conversion
// ranges: Y=0..1, U=-uvmax.x..uvmax.x, V=-uvmax.x..uvmax.x

vec3 yuv_rgb (vec3 yuv, vec2 wbwr, vec2 uvmax) {
	vec2 br = yuv.x + yuv.yz * (1.0 - wbwr) / uvmax;
	float g = (yuv.x - dot(wbwr, br)) / (1.0 - wbwr.x - wbwr.y);
	return vec3(br.y, g, br.x);
}

vec3 rgb_yuv (vec3 rgb, vec2 wbwr, vec2 uvmax) {
	float y = wbwr.y*rgb.r + (1.0 - wbwr.x - wbwr.y)*rgb.g + wbwr.x*rgb.b;
	return vec3(y, uvmax * (rgb.br - y) / (1.0 - wbwr));
}

//----------------------------------------------------------------------------

// YUV, HDTV, gamma compressed, ITU-R BT.709
// ranges: Y=0..1, U=-0.436..0.436, V=-0.615..0.615

vec3 yuv_rgb (vec3 yuv) {
	return yuv_rgb(yuv, vec2(0.0722, 0.2126), vec2(0.436, 0.615));
}

vec3 rgb_yuv (vec3 rgb) {
	return rgb_yuv(rgb, vec2(0.0722, 0.2126), vec2(0.436, 0.615));
}

//----------------------------------------------------------------------------

// Y*b*r, generic conversion
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

vec3 ypbpr_rgb (vec3 ybr, vec2 kbkr) {
	return yuv_rgb(ybr, kbkr, vec2(0.5));
}

vec3 rgb_ypbpr (vec3 rgb, vec2 kbkr) {
	return rgb_yuv(rgb, kbkr, vec2(0.5));
}

//----------------------------------------------------------------------------

// YPbPr, analog, gamma compressed, HDTV
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YPbPr to RGB, after ITU-R BT.709
vec3 ypbpr_rgb (vec3 ypbpr) {
	return ypbpr_rgb(ypbpr, vec2(0.0722, 0.2126));
}

// RGB to YPbPr, after ITU-R BT.709
vec3 rgb_ypbpr (vec3 rgb) {
	return rgb_ypbpr(rgb, vec2(0.0722, 0.2126));
}

//----------------------------------------------------------------------------

// YPbPr, analog, gamma compressed, VGA, TV
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YPbPr to RGB, after ITU-R BT.601
vec3 ypbpr_rgb_bt601 (vec3 ypbpr) {
	return ypbpr_rgb(ypbpr, vec2(0.114, 0.299));
}

// RGB to YPbPr, after ITU-R BT.601
vec3 rgb_ypbpr_bt601 (vec3 rgb) {
	return rgb_ypbpr(rgb, vec2(0.114, 0.299));
}

//----------------------------------------------------------------------------

// in the original implementation, the factors and offsets are
// ypbpr * (219, 224, 224) + (16, 128, 128)

// YPbPr to YCbCr (analog to digital)
vec3 ypbpr_ycbcr (vec3 ypbpr) {
	return ypbpr * vec3(0.85546875,0.875,0.875) + vec3(0.0625, 0.5, 0.5);
}

// YCbCr to YPbPr (digital to analog)
vec3 ycbcr_ypbpr (vec3 ycbcr) {
	return (ycbcr - vec3(0.0625, 0.5, 0.5)) / vec3(0.85546875,0.875,0.875);
}

//----------------------------------------------------------------------------

// YCbCr, digital, gamma compressed
// ranges: Y=0..1, b=0..1, r=0..1

// YCbCr to RGB (generic)
vec3 ycbcr_rgb(vec3 ycbcr, vec2 kbkr) {
	return ypbpr_rgb(ycbcr_ypbpr(ycbcr), kbkr);
}
// RGB to YCbCr (generic)
vec3 rgb_ycbcr(vec3 rgb, vec2 kbkr) {
	return ypbpr_ycbcr(rgb_ypbpr(rgb, kbkr));
}
// YCbCr to RGB
vec3 ycbcr_rgb(vec3 ycbcr) {
	return ypbpr_rgb(ycbcr_ypbpr(ycbcr));
}
// RGB to YCbCr
vec3 rgb_ycbcr(vec3 rgb) {
	return ypbpr_ycbcr(rgb_ypbpr(rgb));
}

//----------------------------------------------------------------------------

// ITU-R BT.2020:
// YcCbcCrc, linear
// ranges: Y=0..1, b=-0.5..0.5, r=-0.5..0.5

// YcCbcCrc to RGB
vec3 yccbccrc_rgb(vec3 yccbccrc) {
	return ypbpr_rgb(yccbccrc, vec2(0.0593, 0.2627));
}

// RGB to YcCbcCrc
vec3 rgb_yccbccrc(vec3 rgb) {
	return rgb_ypbpr(rgb, vec2(0.0593, 0.2627));
}

//----------------------------------------------------------------------------

// YCoCg
// ranges: Y=0..1, Co=-0.5..0.5, Cg=-0.5..0.5

vec3 ycocg_rgb (vec3 ycocg) {
	vec2 br = vec2(-ycocg.y,ycocg.y) - ycocg.z;
	return ycocg.x + vec3(br.y, ycocg.z, br.x);
}

vec3 rgb_ycocg (vec3 rgb) {
	float tmp = 0.5*(rgb.r + rgb.b);
	float y = rgb.g + tmp;
	float Cg = rgb.g - tmp;
	float Co = rgb.r - rgb.b;
	return vec3(y, Co, Cg) * 0.5;
}

//----------------------------------------------------------------------------

vec3 yccbccrc_norm(vec3 ypbpr) {
	vec3 p = yccbccrc_rgb(ypbpr);
	vec3 ro = yccbccrc_rgb(vec3(ypbpr.x, 0.0, 0.0));
	vec3 rd = normalize(p - ro);
	vec3 m = 1./rd;
	vec3 b = 0.5*abs(m)-m*(ro - 0.5);
	float tF = min(min(b.x,b.y),b.z);
	p = ro + rd * tF * max(abs(ypbpr.y),abs(ypbpr.z)) * 2.0;
	return rgb_yccbccrc(p);
}

vec3 ycocg_norm(vec3 ycocg) {
	vec3 p = ycocg_rgb(ycocg);
	vec3 ro = ycocg_rgb(vec3(ycocg.x, 0.0, 0.0));
	vec3 rd = normalize(p - ro);
	vec3 m = 1./rd;
	vec3 b = 0.5*abs(m)-m*(ro - 0.5);
	float tF = min(min(b.x,b.y),b.z);
	p = ro + rd * tF * max(abs(ycocg.y),abs(ycocg.z)) * 2.0;
	return rgb_ycocg(p);
}

//----------------------------------------------------------------------------


///////////////////////////////////////////////////////////////////////
// B C H
///////////////////////////////////////////////////////////////////////
// from: https://www.shadertoy.com/view/lsVGz1
vec3 rgb2DEF(vec3 _col){
	mat3 XYZ; // Adobe RGB (1998)
	XYZ[0] = vec3(0.5767309, 0.1855540, 0.1881852);
	XYZ[1] = vec3(0.2973769, 0.6273491, 0.0752741);
	XYZ[2] = vec3(0.0270343, 0.0706872, 0.9911085);
	mat3 DEF;
	DEF[0] = vec3(0.2053, 0.7125, 0.4670);
	DEF[1] = vec3(1.8537, -1.2797, -0.4429);
	DEF[2] = vec3(-0.3655, 1.0120, -0.6104);
	vec3 xyz = _col.rgb * XYZ;
	vec3 def = xyz * DEF;
	return def;
}

vec3 def2RGB(vec3 _def){
	mat3 XYZ;
	XYZ[0] = vec3(0.6712, 0.4955, 0.1540);
	XYZ[1] = vec3(0.7061, 0.0248, 0.5223);
	XYZ[2] = vec3(0.7689, -0.2556, -0.8645);
	mat3 RGB; // Adobe RGB (1998)
	RGB[0] = vec3(2.0413690, -0.5649464, -0.3446944);
	RGB[1] = vec3(-0.9692660, 1.8760108, 0.0415560);
	RGB[2] = vec3(0.0134474, -0.1183897, 1.0154096);
	vec3 xyz = _def * XYZ;
	vec3 rgb = xyz * RGB;
	return rgb;
}
float getB(vec3 _def){
	float b = sqrt((_def.r*_def.r) + (_def.g*_def.g) + (_def.b*_def.b));
	return b;
}
float getC(vec3 _def){
	vec3 def_D = vec3(1.,0.,0.);
	float C = atan(length(cross(_def,def_D)), dot(_def,def_D));
	return C;
}
float getH(vec3 _def){
	vec3 def_E_axis = vec3(0.,1.,0.);
	float H = atan(_def.z, _def.y) - atan(def_E_axis.z, def_E_axis.y) ;
	return H;
}
// RGB 2 BCH
vec3 rgb2BCH(vec3 _col){
	vec3 DEF = rgb2DEF(_col);
	float B = getB(DEF);
	float C = getC(DEF);
	float H = getH(DEF);
	return vec3(B,C,H);
}
// BCH 2 RGB
vec3 bch2RGB(vec3 _bch){
	vec3 def;
	def.x = _bch.x * cos(_bch.y);
	def.y = _bch.x * sin(_bch.y) * cos(_bch.z);
	def.z = _bch.x * sin(_bch.y) * sin(_bch.z);
	vec3 rgb = def2RGB(def);
	return rgb;
}

// BRIGHTNESS
vec3 Brightness(vec3 _col, float _f){
	vec3 BCH = rgb2BCH(_col);
	vec3 b3 = vec3(BCH.x,BCH.x,BCH.x);
	float x = pow((_f + 1.)/2.,2.);
	x = _f;
	_col = _col + (b3 * x)/3.;
	return _col;
}

// CONTRAST
// simple contrast
// needs neighboring brightness values for higher accuracy
vec3 Contrast(vec3 _col, float _f){
	vec3 def = rgb2DEF(_col);
	float B = getB(def);
	float C = getC(def);
	float H = getH(def);
	B = B * pow(B*(1.-C), _f);
	def.x = B * cos(C);
	def.y = B * sin(C) * cos(H);
	def.z = B * sin(C) * sin(H);
	_col.rgb = def2RGB(def);
	return _col;
}

vec3 Hue(vec3 _col, float _f){
	vec3 BCH = rgb2BCH(_col);
	BCH.z += _f * 3.1459 * 2.;
	BCH = bch2RGB(BCH);
	return BCH;
}

vec3 Saturation(vec3 _col, float _f){
	vec3 BCH = rgb2BCH(_col);
	BCH.y *= (_f + 1.);
	BCH = bch2RGB(BCH);
	return BCH;
}

///////////////////////////////////////////////////////////////////////
// chromamax colorspace from : https://www.shadertoy.com/view/3lS3Wy
// note that this one uses a four channel represntation
#define max3(a) max(a.x, max(a.y, a.z))

vec4 rgb2cm(vec3 rgb){
	float maximum = max3(rgb);
	return vec4(rgb / max(maximum, 1e-32) - maximum, exp2(-maximum));
}

vec3 cm2rgb(vec4 cm){
	float maximum = -log2(cm.a);
	return clamp(cm.rgb + maximum, 0.0, 1.0) * maximum;
}

///////////////////////////////////////////////////////////////////////
// OKLAB implementation from: https://www.shadertoy.com/view/ttcyRS
// with The MIT License
// Copyright © 2020 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Optimized linear-rgb color mix in oklab space, useful
// when our software operates in rgb space but we still
// we want to have intuitive color mixing.
//
// Now, when mixing linear rgb colors in oklab space, the
// linear transform from cone to Lab space and back can be
// omitted, saving three 3x3 transformation per blend!
//
// oklab was invented by Björn Ottosson: https://bottosson.github.io/posts/oklab
//
// More oklab on Shadertoy: https://www.shadertoy.com/view/WtccD7

// vec3 oklab_mix( vec3 colA, vec3 colB, float h )
// {
//     // https://bottosson.github.io/posts/oklab
//     const mat3 kCONEtoLMS = mat3(
//          0.4121656120,  0.2118591070,  0.0883097947,
//          0.5362752080,  0.6807189584,  0.2818474174,
//          0.0514575653,  0.1074065790,  0.6302613616);
//     const mat3 kLMStoCONE = mat3(
//          4.0767245293, -1.2681437731, -0.0041119885,
//         -3.3072168827,  2.6093323231, -0.7034763098,
//          0.2307590544, -0.3411344290,  1.7068625689);

//     // rgb to cone (arg of pow can't be negative)
//     vec3 lmsA = pow( kCONEtoLMS*colA, vec3(1.0/3.0) );
//     vec3 lmsB = pow( kCONEtoLMS*colB, vec3(1.0/3.0) );
//    // lerp
//    vec3 lms = mix( lmsA, lmsB, h );
//    // gain in the middle (no oaklab anymore, but looks better?)
// // lms *= 1.0+0.2*h*(1.0-h);
//    // cone to rgb
// return kLMStoCONE*(lms*lms*lms);
// }


//////////////////////////////////////////////////////////////////////
// alternative public domain implementation from https://www.shadertoy.com/view/WtccD7
//////////////////////////////////////////////////////////////////////

// oklab transform and inverse from
// https://bottosson.github.io/posts/oklab/

vec3 oklab_from_linear_srgb(vec3 c) {
	const mat3 invB = mat3(0.4121656120, 0.2118591070, 0.0883097947,
							0.5362752080, 0.6807189584, 0.2818474174,
							0.0514575653, 0.1074065790, 0.6302613616);
	const mat3 invA = mat3(0.2104542553, 1.9779984951, 0.0259040371,
							0.7936177850, -2.4285922050, 0.7827717662,
							-0.0040720468, 0.4505937099, -0.8086757660);
	vec3 lms = invB * c;
	return invA * (sign(lms)*pow(abs(lms), vec3(0.3333333333333)));
}

vec3 linear_srgb_from_oklab(vec3 c) {
	const mat3 fwdA = mat3(1.0000000000,  1.0000000000,  1.0000000000,
							0.3963377774, -0.1055613458, -0.0894841775,
							0.2158037573, -0.0638541728, -1.2914855480);
	const mat3 fwdB = mat3(4.0767245293, -1.2681437731, -0.0041119885,
							-3.3072168827, 2.6093323231, -0.7034763098,
							0.2307590544, -0.3411344290,  1.7068625689);
	vec3 lms = fwdA * c;
	return fwdB * (lms * lms * lms);
}


// do some #define statements to make the below switch statements more legible
#define RGB       0
#define SRGB      1
#define XYZ       2
#define XYY       3
#define HSV       4
#define HSL       5
#define HCY       6
#define YPBPR     7
#define YPBPR601  8
#define YCBCR1    9
#define YCBCR2    10
#define YCCBCCRC  11
#define YCOCG     12
#define BCH       13
#define CHROMAMAX 14
#define OKLAB     15

vec4 convert ( uvec4 value, int spaceswitch ){
	vec4 converted = vec4( 0.0, 0.0, 0.0, 1.0 );
	vec4 base_rgbval = vec4( value ) / 255.0;
	switch ( spaceswitch ) {
	case RGB:		converted = base_rgbval;	break;
	case SRGB:		converted.rgb = rgb_to_srgb( base_rgbval.rgb ); break;
	case XYZ:		converted.rgb = rgb_to_xyz( base_rgbval.rgb ); break;
	case XYY:		converted.rgb = rgb_to_xyY( base_rgbval.rgb ); break;
	case HSV:		converted.rgb = rgb_to_hsv( base_rgbval.rgb ); break;
	case HSL:		converted.rgb = rgb_to_hsl( base_rgbval.rgb ); break;
	case HCY:		converted.rgb = rgb_to_hcy( base_rgbval.rgb ); break;
	case YPBPR:		converted.rgb = rgb_ypbpr( base_rgbval.rgb ); break;
	case YPBPR601:	converted.rgb = rgb_ypbpr_bt601( base_rgbval.rgb ); break;
	case YCBCR1:	converted.rgb = rgb_to_ycbcr( base_rgbval.rgb ); break;
	case YCBCR2:	converted.rgb = rgb_ycbcr( base_rgbval.rgb ); break;
	case YCCBCCRC:	converted.rgb = rgb_yccbccrc( base_rgbval.rgb ); break;
	case YCOCG:		converted.rgb = rgb_ycocg( base_rgbval.rgb ); break;
	case BCH:		converted.rgb = rgb2BCH( base_rgbval.rgb ); break;
	case CHROMAMAX:	converted.rgba = rgb2cm( base_rgbval.rgb ); break;
	case OKLAB:		converted.rgb = oklab_from_linear_srgb( base_rgbval.rgb ); break;
	default: break;
	}
	return converted;
}

// returns a uvec4 which is ready to be written as 8-bit RGBA
uvec4 convertBack ( vec4 value, int spaceswitch ){
	uvec4 converted = uvec4( 0, 0, 0, 255 );
	switch ( spaceswitch ) {
	case RGB:		converted = uvec4( vec3( value * 255.0 ), 255 ); break;
	case SRGB:		converted.rgb = uvec3( srgb_to_rgb( value.rgb ) * 255 ); break;
	case XYZ:		converted.rgb = uvec3( xyz_to_rgb( value.rgb ) * 255 ); break;
	case XYY:		converted.rgb = uvec3( xyY_to_rgb( value.rgb ) * 255 ); break;
	case HSV:		converted.rgb = uvec3( hsv_to_rgb( value.rgb ) * 255 ); break;
	case HSL:		converted.rgb = uvec3( hsl_to_rgb( value.rgb ) * 255 ); break;
	case HCY:		converted.rgb = uvec3( hcy_to_rgb( value.rgb ) * 255 ); break;
	case YPBPR:		converted.rgb = uvec3( ypbpr_rgb( value.rgb ) * 255 ); break;
	case YPBPR601:	converted.rgb = uvec3( ypbpr_rgb_bt601( value.rgb ) * 255 ); break;
	case YCBCR1:	converted.rgb = uvec3( ycbcr_to_rgb( value.rgb ) * 255 ); break;
	case YCBCR2:	converted.rgb = uvec3( ycbcr_rgb( value.rgb ) * 255 ); break;
	case YCCBCCRC:	converted.rgb = uvec3( yccbccrc_rgb( value.rgb ) * 255 ); break;
	case YCOCG:		converted.rgb = uvec3( ycocg_rgb( value.rgb ) * 255 ); break;
	case BCH:		converted.rgb = uvec3( bch2RGB( value.rgb ) * 255 ); break;
	case CHROMAMAX:	converted.rgb = uvec3( cm2rgb( value.rgba ) * 255 ); break;
	case OKLAB:		converted.rgb = uvec3( linear_srgb_from_oklab( value.rgb ) * 255 ); break;
	default: break;
	}
	return converted;
}
