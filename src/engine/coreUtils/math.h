#pragma once
#ifndef MATH_H
#define MATH_H

// remap the value from [inLow..inHigh] to [outLow..outHigh]
inline float RangeRemap ( float value, float inLow, float inHigh, float outLow, float outHigh ) {
	return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
}

// other curves? there were some things
	// Bias and Gain Functions - https://arxiv.org/abs/2010.09714
	// iq's Usful Functions - https://iquilezles.org/articles/functions/
	// Easing Functions - https://easings.net/

#endif // MATH_H