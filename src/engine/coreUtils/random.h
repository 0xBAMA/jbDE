#pragma once
#ifndef RANDOM
#define RANDOM

#include <random>
#include <type_traits> // for std::is_same - https://en.cppreference.com/w/cpp/types/is_same

// simplifying the use of std::random, so I don't have to look it up every single time
template < typename T > class rng {
public:
	rng ( T min, T max ) : minVal( min ), maxVal( max ) {
		std::random_device r;
		std::seed_seq seed{ r(), r(), r(), r(), r(), r(), r(), r(), r() };
		generator = std::mt19937_64( seed );
	}

	T get () {
		// // pain in the ass needs to know type because the distributions are different for int / real
		// constexpr bool isFloatType = std::is_same< T, float >::value;
		// if ( isFloatType ) {
		// 	std::uniform_real_distribution< T > distribution( minVal, maxVal );
		// 	return distribution( generator );
		// } else {
		// 	std::uniform_int_distribution< T > distribution( minVal, maxVal );
		// 	return distribution( generator );
		// }

		// another solution...
		std::uniform_real_distribution< T > distribution( minVal, maxVal );
		return T( distribution( generator ) );
	}

private:
	std::mt19937_64 generator;
	T minVal, maxVal;
};

// seedable, deterministic
	// wang hash generator
	// LCG generator

#endif // RANDOM
