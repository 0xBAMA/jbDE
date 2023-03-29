#pragma once
#ifndef RANDOM
#define RANDOM

#include <random>

// simplifying the use of std::random, so I don't have to look it up every single time
class rng {
public:

	// hardware seeded
	rng ( float lo, float hi ) :
		distribution( std::uniform_real_distribution< float >( lo, hi ) ) {
			std::random_device r;
			std::seed_seq seed { r(),r(),r(),r(),r(),r(),r(),r(),r() };
			generator = std::mt19937_64( seed );
		}

	// known 32-bit seed value
	rng ( float lo, float hi, uint32_t seed ) :
		generator( std::mt19937_64( seed ) ),
		distribution( std::uniform_real_distribution< float >( lo, hi ) ) {}

	// get the value
	// float get () { return distribution( generator ); }
	float operator () () { return distribution( generator ); }

private:
	std::mt19937_64 generator;
	std::uniform_real_distribution< float > distribution;
};

// potentially add an integer version? std::random distributions have stupid asserts
	// about is_integral whatever whatever, will make this a fair bit more complicated
	// if I want to allow it to be more flexible. I think the above float version will
	// be sufficient and you can cast the output to whatever type given the specified
	// input range

// other potential future options... maybe not neccesary, given the seedability of the mt19937_64 object
	// wang hash generator
	// LCG generator

// remapping functions - if we're getting uniform random numbers we want to do something to be able to shift the distribution
	// Bias and Gain Functions https://arxiv.org/abs/2010.09714
	// iq's Usful Functions https://iquilezles.org/articles/functions/
	// Easing Functions https://easings.net/

#endif // RANDOM
