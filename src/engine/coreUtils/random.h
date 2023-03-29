#pragma once
#ifndef RANDOM
#define RANDOM

#include <random>

std::random_device r;

// simplifying the use of std::random, so I don't have to look it up every single time
template < typename T > class rng {
public:

	// generate a seed from the system's random device
	rng ( T min, T max ) :
		generator( std::mt19937_64( std::seed_seq( r(),r(),r(),r(),r(),r(),r(),r(),r() ) ) ),
		distribution( std::uniform_real_distribution< T >( min, max ) ) {}

	// alternatively, take 32 bit known seed value
	rng ( T min, T max, uint32_t seed ) :
		generator( std::mt19937_64( seed ) ),
		distribution( std::uniform_real_distribution< T >( min, max ) ) {}

	T get () {
		return T( distribution( generator ) );
	}

private:
	std::mt19937_64 generator;
	std::uniform_real_distribution< T > distribution;
};


// other potential future options... maybe not neccesary, given the seedability of the mt19937_64 object
	// wang hash generator
	// LCG generator

#endif // RANDOM
