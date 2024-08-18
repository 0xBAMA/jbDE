#include <vector>
#include <string>
#include <algorithm>

struct terminalState_t {
	// display extents
	const int baseX = 10;
	const int baseY = 10;
	const int width = 120;
	const int height = 60;

	// input cursor - 2d would be interesting
	int cursorX = 0;
	int cursorY = 0;

	// lines of input history
	std::vector< string > history;

	// the current input line
	string currentLine;

	bool isDivider ( char c ) {
		// tbd which ones of these we want to use
		string dividers = string( " ,./\\'\"!@#$%^&*()_+{}[]" );
		for ( auto& symbol : dividers )
			if ( c == symbol )
				return true;

		return false;
	}

	void backspace ( bool control ) {
		if ( control ) {
			// erase till you hit whitespace
			do {
				cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
				currentLine.erase( currentLine.begin() + cursorX );
			} while ( !isDivider( currentLine[ cursorX - 1 ] ) && ( cursorX - 1 >= 0 ) );
		} else {
			// remove char before the cursor
			cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
			if ( currentLine.length() > 0 )
				currentLine.erase( currentLine.begin() + cursorX );
		}
	}

	void deleteKey () {
		// remove the char at the cursor
		if ( currentLine.length() > 0 )
			currentLine.erase( currentLine.begin() + cursorX );
	}

	void enter () {
		// push this line onto the history
		history.push_back( currentLine );

		// clear the intput line
		currentLine.clear();

		// reset the cursor
		cursorX = 0;
	}

	void cursorLeft ( bool control ) {
		if ( control ) {
			do {
				cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
			} while ( !isDivider( currentLine[ cursorX - 1 ] ) && ( cursorX - 1 >= 0 ) );
		} else {
			cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
		}
	}

	void cursorRight ( bool control ) {
		if ( control ) {
			do {
				cursorX = std::clamp( cursorX + 1, 0, int( currentLine.length() ) );
			} while ( !isDivider( currentLine[ cursorX + 1 ] ) && ( cursorX + 1 < int( currentLine.length() ) ) );
		} else {
			cursorX = std::clamp( cursorX + 1, 0, int( currentLine.length() ) );
		}
	}

	void addChar ( char c ) {
		// add this char at the cursor
		currentLine.insert( currentLine.begin() + cursorX, c );
		cursorX++;
	}

};