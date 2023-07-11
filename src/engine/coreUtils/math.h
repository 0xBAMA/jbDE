#pragma once
#ifndef MATH_H
#define MATH_H

// remap the value from [inLow..inHigh] to [outLow..outHigh]
inline float RangeRemap ( float value, float inLow, float inHigh, float outLow, float outHigh ) {
	return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
}

#endif // MATH_H