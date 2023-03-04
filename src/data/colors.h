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

inline vec3 GetColorForTemperature ( float temperature ) {
// from the author:
	// Color temperature (sRGB) stuff
	// Copyright (C) 2014 by Benjamin 'BeRo' Rosseaux
	// Because the german law knows no public domain in the usual sense,
	// this code is licensed under the CC0 license http://creativecommons.org/publicdomain/zero/1.0/

// Valid from 1000 to 40000 K (and additionally 0 for pure full white)

	// Reference Values from:
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

// TODO on palettes:
// couple new ones to look at from https://www.shadertoy.com/view/cdlSzB
	// https://www.alanzucconi.com/2017/07/15/improving-the-rainbow-2/
	// https://gist.github.com/mikhailov-work/0d177465a8151eb6ede1768d51d476c7
	// https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap-for.html
	// https://gist.github.com/mikhailov-work/6a308c20e494d9e0ccc29036b28faa7a

// restore old ordering on lospec palettes, for interpolated reference

namespace palette {

	// I think the best way to do this is to have any state set as a variable in this namespace
		// e.g. which palette index ( or find the palette by string label ), IQ palette control points, simple gradient endpoints

	enum class type {
//==== lospec / matplotlib indexed set ========================================
		// map input 0-1 to nearest color
		paletteIndexed,

		// take the floor of the input, mod size of the palette
		paletteIndexed_modInt,

		// blend nearest two colors
		paletteIndexed_interpolated,


//==== Analytic Palettes ======================================================
	//==== from GetColorForTemperature() above ================================
		paletteTemperature,
		paletteTemperature_normalized,

	//==== from HeatMapColorRamp() above ======================================
		paletteHeatmapRamp,

	//==== Inigo Quilez-style sinusoidal palettes =============================
		paletteIQSinusoid,

	//==== Simple gradient, interpolating between two colors ==================
		paletteSimpleGradient

	};

	inline int PaletteIndex = 0;
	inline vec3 IQControlPoints[ 4 ] = { vec3( 0.0f ) };
	inline vec3 GradientEndpoints[ 2 ] = { vec3( 0.0f ) };

	inline void SetIQPaletteDefaultSet ( int index ) {
		const vec3 values[ 7 ][ 4 ] = { // to generate more: https://www.shadertoy.com/view/tlSBDw - looks like this set of 7 covers most of the ones of any value
			{ vec3( 0.5f, 0.5f, 0.5f ), vec3( 0.5f, 0.5f, 0.5f ), vec3( 1.0f, 1.0f, 1.0f ), vec3( 0.00f, 0.10f, 0.20f ) },	// steel heat colors ( I like this one )
			{ vec3( 0.5f, 0.5f, 0.5f ), vec3( 0.5f, 0.5f, 0.5f ), vec3( 1.0f, 1.0f, 1.0f ), vec3( 0.00f, 0.33f, 0.67f ) },	// rainbow
			{ vec3( 0.5f, 0.5f, 0.5f ), vec3( 0.5f, 0.5f, 0.5f ), vec3( 1.0f, 1.0f, 1.0f ), vec3( 0.30f, 0.20f, 0.20f ) },	// blue-red
			{ vec3( 0.5f, 0.5f, 0.5f ), vec3( 0.5f, 0.5f, 0.5f ), vec3( 1.0f, 1.0f, 0.5f ), vec3( 0.80f, 0.90f, 0.30f ) },	// yellow-green-blue
			{ vec3( 0.5f, 0.5f, 0.5f ), vec3( 0.5f, 0.5f, 0.5f ), vec3( 1.0f, 0.7f, 0.4f ), vec3( 0.00f, 0.15f, 0.20f ) },	// purple-blue-yellow
			{ vec3( 0.5f, 0.5f, 0.5f ), vec3( 0.5f, 0.5f, 0.5f ), vec3( 2.0f, 1.0f, 0.0f ), vec3( 0.50f, 0.20f, 0.25f ) },	// pink-green-yellow
			{ vec3( 0.8f, 0.5f, 0.4f ), vec3( 0.2f, 0.4f, 0.2f ), vec3( 2.0f, 1.0f, 1.0f ), vec3( 0.00f, 0.25f, 0.25f ) }	// watermelon
		};
		index = std::clamp( index, 0, 6 );
		for ( uint8_t pt{ 0 }; pt < 4; pt++ )
			IQControlPoints[ 0 ] = values[ index ][ pt ];
	}

	inline vec3 iqPaletteRef ( float t, vec3 a, vec3 b, vec3 c, vec3 d ) {
		return a + b * cos( 6.28318f * ( c * t + d ) );
	}

	// lospec / matplotlib palettes
		// set palette by label
		// pick random palette
		// get palette name

	inline vec3 paletteRef ( float input, type inputType ) {
		switch ( inputType ) {

			// indexing into the currently selected palette / palette size ( nearest neighbor )
			case type::paletteIndexed:
				return vec3( paletteList[ PaletteIndex ].colors[ int( input * ( paletteList[ PaletteIndex ].colors.size() - 1 ) ) ] ) / 255.0f;
				break;

			// indexing into the currently selected palette, with the floor of the input, mod by the size of the palette
			case type::paletteIndexed_modInt:
				return vec3( paletteList[ PaletteIndex ].colors[ int( input ) % paletteList[ PaletteIndex ].colors.size() ] ) / 255.0f;
				break;

			// same as paletteIndexed, but interpolate between nearest values - TODO
			case type::paletteIndexed_interpolated:
				return vec3( 0.0f );
				break;

			// use input value raw
			case type::paletteTemperature:
				return GetColorForTemperature( input );
				break;

			// remap 0-1 to 0-40k
			case type::paletteTemperature_normalized:
				return GetColorForTemperature( input * 40000.0f );
				break;

			// reference to the sinusoidal
			case type::paletteHeatmapRamp:
				return HeatMapColorRamp( input );
				break;

			case type::paletteIQSinusoid: // using currently set control points, IQControlPoints[]
				return iqPaletteRef( input, IQControlPoints[ 0 ], IQControlPoints[ 1 ], IQControlPoints[ 2 ], IQControlPoints[ 3 ] );
				break;

			case type::paletteSimpleGradient:
				return glm::mix( GradientEndpoints[ 0 ], GradientEndpoints[ 1 ], input );
				break;

			default: // shouldn't be able to hit this
				return vec3( 0.0f );
				break;

		}
	}
};




#endif //COLORS_H
