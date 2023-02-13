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

#endif //COLORS_H
