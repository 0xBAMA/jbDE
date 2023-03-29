#pragma once
#ifndef RANDOM
#define RANDOM

#include <random>

// float version
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

// Integer version
class rngi {
public:

	// hardware seeded
	rngi ( int lo, int hi ) :
		distribution( std::uniform_int_distribution< int >( lo, hi ) ) {
			std::random_device r;
			std::seed_seq seed { r(),r(),r(),r(),r(),r(),r(),r(),r() };
			generator = std::mt19937_64( seed );
		}

	// known 32-bit seed value
	rngi ( int lo, int hi, uint32_t seed ) :
		generator( std::mt19937_64( seed ) ),
		distribution( std::uniform_int_distribution< int >( lo, hi ) ) {}

	// get the value
	// int get () { return distribution( generator ); }
	int operator () () { return distribution( generator ); }

private:
	std::mt19937_64 generator;
	std::uniform_int_distribution< int > distribution;
};

// other potential future options... maybe not neccesary, given the seedability of the mt19937_64 object
	// wang hash generator
	// LCG generator

// remapping functions - if we're getting uniform random numbers we want to do something to be able to shift the distribution
	// Bias and Gain Functions https://arxiv.org/abs/2010.09714
	// iq's Usful Functions https://iquilezles.org/articles/functions/
	// Easing Functions https://easings.net/

#endif // RANDOM
