//***********************************************************
//
//  File:     colors.h
//
//  Author:   Matthew Beldyk
//  Email:    mb245002@ohiou.edu
//
//  Usage:    I created this file to house some named string
//            constants with escape codes for colors in them
//            this makes it much easier for me to do colors.
//            I can still use the codes if I want, but this
//            works too.  try the statement:
//            cout<<BLUE<<"I like cookies"<<endl;
//
//		  You may use this whereever you want, but if you
//		  make any large improvements or whatever, I am
//		  curious, so email 'em my way, please.
//
//***********************************************************
//
//  all credit given to Matthew Beldyk for writing this file
//  he gave me permission to try out in my programs
//  just wanted to use to make everything look nice
//
//***********************************************************

#ifndef COLORS_H
#define COLORS_H

#include <string>
#include <algorithm>
#include "./paletteLoader.h" // for the palettes from lospec

// some windows specific defines to set this, probably - msvc complains about unrecognized escape sequences
#define USE_COMMAND_LINE_COLOR_SEQUENCES

#ifdef USE_COMMAND_LINE_COLOR_SEQUENCES

	const std::string BLINK		= "\e[5m";
	const std::string BOLD		= "\e[1m";
	const std::string RESET		= "\e[0m";
	const std::string ERROR		= "\e[1;41;37m\a";
	const std::string MENU		= "\e[44;37m";

	// text colors
	const std::string T_BLACK	= "\e[30m";
	const std::string T_RED		= "\e[31m";
	const std::string T_GREEN	= "\e[32m";
	const std::string T_YELLOW	= "\e[33m";
	const std::string T_BLUE	= "\e[34m";
	const std::string T_MAGENTA	= "\e[35m";
	const std::string T_CYAN	= "\e[36m";
	const std::string T_WHITE	= "\e[37m";

	// background colors
	const std::string B_BLACK	= "\e[40m";
	const std::string B_RED		= "\e[41m";
	const std::string B_GREEN	= "\e[42m";
	const std::string B_YELLOW	= "\e[43m";
	const std::string B_BLUE	= "\e[44m";
	const std::string B_MAGENTA	= "\e[45m";
	const std::string B_CYAN	= "\e[46m";
	const std::string B_WHITE	= "\e[47m";

#else

	// windows doesn't recognize any of these - skip it
	const std::string BLINK		= "";
	const std::string BOLD		= "";
	const std::string RESET		= "";
	const std::string ERROR		= "";
	const std::string MENU		= "";
	const std::string T_BLACK	= "";
	const std::string T_RED		= "";
	const std::string T_GREEN	= "";
	const std::string T_YELLOW	= "";
	const std::string T_BLUE	= "";
	const std::string T_MAGENTA	= "";
	const std::string T_CYAN	= "";
	const std::string T_WHITE	= "";
	const std::string B_BLACK	= "";
	const std::string B_RED		= "";
	const std::string B_GREEN	= "";
	const std::string B_YELLOW	= "";
	const std::string B_BLUE	= "";
	const std::string B_MAGENTA	= "";
	const std::string B_CYAN	= "";
	const std::string B_WHITE	= "";

#endif

// Function to get color temperature from shadertoy user BeRo
// from the author:
//   Color temperature (sRGB) stuff
//   Copyright (C) 2014 by Benjamin 'BeRo' Rosseaux
//   Because the german law knows no public domain in the usual sense,
//   this code is licensed under the CC0 license
//   http://creativecommons.org/publicdomain/zero/1.0/
// Valid from 1000 to 40000 K (and additionally 0 for pure full white)
inline vec3 GetColorForTemperature ( float temperature ) {
	// Values from:
	// http://blenderartists.org/forum/showthread.php?270332-OSL-Goodness&p=2268693&viewfull=1#post2268693
	mat3 m =
		( temperature <= 6500.0f )
			? mat3( vec3( 0.0f, -2902.1955373783176f, -8257.7997278925690f ),
					vec3( 0.0f, 1669.5803561666639f, 2575.2827530017594f ),
					vec3( 1.0f, 1.3302673723350029f, 1.8993753891711275f ) )
			: mat3( vec3( 1745.0425298314172f, 1216.6168361476490f, -8257.7997278925690f ),
					vec3( -2666.3474220535695f, -2173.1012343082230f, 2575.2827530017594f ),
					vec3( 0.55995389139931482f, 0.70381203140554553f, 1.8993753891711275f ) );

	return glm::mix( glm::clamp( vec3( m[ 0 ] / ( vec3( glm::clamp( temperature, 1000.0f, 40000.0f ) ) +
		m[ 1 ] ) + m[ 2 ] ), vec3( 0.0f ), vec3( 1.0f ) ), vec3( 1.0f ), glm::smoothstep( 1000.0f, 0.0f, temperature ) );
}

// unknown origin, blue to red fade through green
inline vec3 HeatMapColorRamp ( float input ) {
	input *= pi / 2.0f;
	return vec3( sin( input ), sin( input * 2.0f ), cos( input ) );
}

inline ivec3 magmaRef ( float ref ) {
	glm::ivec3 colorLut[] = {
		{   0,   0,   3 },
		{   0,   0,   4 },
		{   0,   0,   6 },
		{   1,   0,   7 },
		{   1,   1,   9 },
		{   1,   1,  11 },
		{   2,   2,  13 },
		{   2,   2,  15 },
		{   3,   3,  17 },
		{   4,   3,  19 },
		{   4,   4,  21 },
		{   5,   4,  23 },
		{   6,   5,  25 },
		{   7,   5,  27 },
		{   8,   6,  29 },
		{   9,   7,  31 },
		{  10,   7,  34 },
		{  11,   8,  36 },
		{  12,   9,  38 },
		{  13,  10,  40 },
		{  14,  10,  42 },
		{  15,  11,  44 },
		{  16,  12,  47 },
		{  17,  12,  49 },
		{  18,  13,  51 },
		{  20,  13,  53 },
		{  21,  14,  56 },
		{  22,  14,  58 },
		{  23,  15,  60 },
		{  24,  15,  63 },
		{  26,  16,  65 },
		{  27,  16,  68 },
		{  28,  16,  70 },
		{  30,  16,  73 },
		{  31,  17,  75 },
		{  32,  17,  77 },
		{  34,  17,  80 },
		{  35,  17,  82 },
		{  37,  17,  85 },
		{  38,  17,  87 },
		{  41,  17,  91 },
		{  43,  17,  93 },
		{  44,  16,  95 },
		{  46,  16,  97 },
		{  47,  16,  99 },
		{  49,  16, 102 },
		{  51,  16, 103 },
		{  52,  16, 105 },
		{  54,  15, 107 },
		{  56,  15, 109 },
		{  58,  15, 110 },
		{  59,  15, 112 },
		{  61,  15, 113 },
		{  63,  15, 114 },
		{  65,  15, 115 },
		{  66,  15, 116 },
		{  68,  15, 117 },
		{  70,  15, 118 },
		{  71,  15, 119 },
		{  72,  16, 120 },
		{  74,  16, 121 },
		{  75,  16, 121 },
		{  77,  17, 122 },
		{  79,  17, 123 },
		{  80,  18, 123 },
		{  82,  18, 124 },
		{  83,  19, 124 },
		{  85,  19, 125 },
		{  87,  20, 125 },
		{  88,  21, 126 },
		{  90,  21, 126 },
		{  91,  22, 126 },
		{  93,  23, 126 },
		{  94,  23, 127 },
		{  97,  24, 127 },
		{  99,  25, 127 },
		{ 101,  26, 128 },
		{ 102,  26, 128 },
		{ 104,  27, 128 },
		{ 105,  28, 128 },
		{ 106,  28, 128 },
		{ 108,  29, 128 },
		{ 109,  30, 129 },
		{ 111,  30, 129 },
		{ 112,  31, 129 },
		{ 114,  31, 129 },
		{ 116,  32, 129 },
		{ 117,  33, 129 },
		{ 119,  33, 129 },
		{ 120,  34, 129 },
		{ 122,  34, 129 },
		{ 123,  35, 129 },
		{ 125,  36, 129 },
		{ 127,  36, 129 },
		{ 128,  37, 129 },
		{ 130,  37, 129 },
		{ 131,  37, 129 },
		{ 132,  38, 129 },
		{ 134,  38, 129 },
		{ 136,  39, 129 },
		{ 137,  40, 129 },
		{ 139,  40, 129 },
		{ 140,  41, 128 },
		{ 142,  41, 128 },
		{ 144,  42, 128 },
		{ 145,  42, 128 },
		{ 147,  43, 128 },
		{ 148,  43, 128 },
		{ 150,  44, 128 },
		{ 152,  44, 127 },
		{ 154,  45, 127 },
		{ 156,  46, 127 },
		{ 158,  46, 126 },
		{ 159,  47, 126 },
		{ 161,  47, 126 },
		{ 163,  48, 126 },
		{ 164,  48, 125 },
		{ 166,  49, 125 },
		{ 167,  49, 125 },
		{ 169,  50, 124 },
		{ 171,  51, 124 },
		{ 172,  51, 123 },
		{ 174,  52, 123 },
		{ 176,  52, 123 },
		{ 177,  53, 122 },
		{ 179,  53, 122 },
		{ 181,  54, 121 },
		{ 182,  54, 121 },
		{ 184,  55, 120 },
		{ 185,  55, 120 },
		{ 187,  56, 119 },
		{ 189,  57, 119 },
		{ 190,  57, 118 },
		{ 192,  58, 117 },
		{ 193,  58, 117 },
		{ 195,  59, 116 },
		{ 196,  60, 116 },
		{ 198,  60, 115 },
		{ 199,  61, 114 },
		{ 201,  62, 114 },
		{ 203,  62, 113 },
		{ 204,  63, 112 },
		{ 206,  64, 112 },
		{ 207,  65, 111 },
		{ 209,  66, 110 },
		{ 211,  66, 109 },
		{ 212,  67, 109 },
		{ 214,  68, 108 },
		{ 215,  69, 107 },
		{ 217,  70, 106 },
		{ 218,  71, 105 },
		{ 220,  72, 105 },
		{ 221,  73, 104 },
		{ 222,  74, 103 },
		{ 224,  75, 102 },
		{ 225,  76, 102 },
		{ 226,  77, 101 },
		{ 228,  78, 100 },
		{ 229,  80,  99 },
		{ 230,  81,  98 },
		{ 231,  82,  98 },
		{ 232,  84,  97 },
		{ 234,  85,  96 },
		{ 235,  86,  96 },
		{ 236,  88,  95 },
		{ 237,  89,  95 },
		{ 238,  91,  94 },
		{ 238,  93,  93 },
		{ 239,  94,  93 },
		{ 240,  96,  93 },
		{ 241,  97,  92 },
		{ 242,  99,  92 },
		{ 243, 101,  92 },
		{ 243, 103,  91 },
		{ 244, 104,  91 },
		{ 245, 106,  91 },
		{ 245, 108,  91 },
		{ 246, 110,  91 },
		{ 246, 112,  91 },
		{ 247, 113,  91 },
		{ 247, 115,  92 },
		{ 248, 117,  92 },
		{ 248, 119,  92 },
		{ 249, 121,  92 },
		{ 249, 123,  93 },
		{ 249, 125,  93 },
		{ 250, 127,  94 },
		{ 250, 128,  94 },
		{ 250, 130,  95 },
		{ 251, 132,  96 },
		{ 251, 134,  96 },
		{ 251, 136,  97 },
		{ 251, 138,  98 },
		{ 252, 140,  99 },
		{ 252, 142,  99 },
		{ 252, 144, 100 },
		{ 252, 146, 101 },
		{ 252, 147, 102 },
		{ 253, 149, 103 },
		{ 253, 151, 104 },
		{ 253, 153, 105 },
		{ 253, 155, 106 },
		{ 253, 157, 107 },
		{ 253, 159, 108 },
		{ 253, 161, 110 },
		{ 253, 162, 111 },
		{ 253, 164, 112 },
		{ 254, 166, 113 },
		{ 254, 168, 115 },
		{ 254, 170, 116 },
		{ 254, 172, 117 },
		{ 254, 174, 118 },
		{ 254, 175, 120 },
		{ 254, 178, 122 },
		{ 254, 180, 123 },
		{ 254, 182, 124 },
		{ 254, 184, 126 },
		{ 254, 186, 127 },
		{ 254, 187, 129 },
		{ 254, 189, 130 },
		{ 254, 190, 131 },
		{ 254, 192, 133 },
		{ 254, 194, 134 },
		{ 254, 196, 136 },
		{ 254, 198, 137 },
		{ 254, 199, 139 },
		{ 254, 201, 141 },
		{ 254, 203, 142 },
		{ 253, 205, 144 },
		{ 253, 207, 146 },
		{ 253, 209, 147 },
		{ 253, 210, 149 },
		{ 253, 212, 151 },
		{ 253, 214, 152 },
		{ 253, 216, 154 },
		{ 253, 218, 156 },
		{ 253, 220, 157 },
		{ 253, 221, 159 },
		{ 253, 223, 161 },
		{ 253, 225, 163 },
		{ 252, 227, 165 },
		{ 252, 229, 166 },
		{ 252, 230, 168 },
		{ 252, 232, 170 },
		{ 252, 235, 173 },
		{ 252, 237, 175 },
		{ 252, 239, 177 },
		{ 252, 241, 178 },
		{ 252, 242, 180 },
		{ 252, 244, 182 },
		{ 251, 246, 184 },
		{ 251, 248, 186 },
		{ 251, 250, 188 },
		{ 251, 251, 190 },
		{ 251, 252, 196 } };

	int refIdx = std::clamp( int( ref * 255.0f ), 0, 255 );
	return colorLut[ refIdx ];
}

// TODO: palette wrapper
	// paletteRef ( 0-1 index, some kind of tag to switch between different palettes ( enum class? ) )


#endif //COLORS_H
