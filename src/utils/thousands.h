#pragma once

#ifndef THOUSANDS_SEPARATOR_H
#define THOUSANDS_SEPARATOR_H

#include <locale>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

class comma_numpunct : public std::numpunct< char > {
protected:
	virtual char do_thousands_sep () const {
		return ',';
	}

	virtual std::string do_grouping () const {
		return "\03";
	}
};

std::string GetWithThousandsSeparator () {
	std::stringstream out;
	std::locale commaLocale( std::locale, new comma_numpunct() );
	out.imbue( commaLocale );
	out << std::setprecision( 2 ) << std::fixed << 100000000.0f;
	return out.str();
}

#endif