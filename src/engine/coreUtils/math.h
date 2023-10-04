#pragma once
#ifndef MATH_H
#define MATH_H

// for formatted output
inline std::string fixedWidthNumberString ( int32_t value, int32_t width = 5, const char fill = '0' ) {
	return string( width - std::min( width, ( int ) to_string( value ).length() ), fill ) + to_string( value );
}

// remap the value from [inLow..inHigh] to [outLow..outHigh]
inline float RangeRemap ( float value, float inLow, float inHigh, float outLow, float outHigh ) {
	return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
}

// other curves? there were some things
	// Bias and Gain Functions - https://arxiv.org/abs/2010.09714
	// iq's Usful Functions - https://iquilezles.org/articles/functions/
	// Easing Functions - https://easings.net/

#endif // MATH_H