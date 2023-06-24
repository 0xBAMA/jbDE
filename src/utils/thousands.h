#pragma once

#ifndef THOUSANDS_SEPARATOR_H
#define THOUSANDS_SEPARATOR_H

#include <string>

inline std::string GetWithThousandsSeparator ( size_t value ) {
	std::string s = std::to_string( value );
	int n = s.length() - 3;
	int end = ( value >= 0 ) ? 0 : 1; // Support for negative numbers
	while ( n > end ) {
		s.insert( n, "," );
		n -= 3;
	}
	return s;
}

#endif