#pragma once
#ifndef RANDOM
#define RANDOM

#include <random>

// simplifying the use of std::random, so I don't have to look it up every single time
template < typename T > class rng {
public:

	// generate a seed from the system's random device
	rng ( T min, T max ) : minVal( min ), maxVal( max ) {
		std::random_device r;
		std::seed_seq seed{ r(), r(), r(), r(), r(), r(), r(), r(), r() };
		generator = std::mt19937_64( seed );
		distribution = std::uniform_real_distribution< T >( minVal, maxVal );
	}

	// take 32 bit input seed value
	rng ( T min, T max, uint32_t seed ) : minVal( min ), maxVal( max ) {
		generator = std::mt19937_64( seed );
		distribution = std::uniform_real_distribution< T >( minVal, maxVal );
	}

	T get () {
		return T( distribution( generator ) );
	}

private:
	std::mt19937_64 generator;
	std::uniform_real_distribution< T > distribution;
	T minVal, maxVal; // not sure if these need to be kept...
};

// seedable, deterministic versions ( mt19937 is seedable+deterministic, just need to figure out how to seed )
	// wang hash generator
	// LCG generator

#endif // RANDOM
