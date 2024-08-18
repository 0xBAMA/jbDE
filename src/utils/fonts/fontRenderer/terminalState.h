#include "../../../engine/includes.h"

struct historyItem_t {
	string commandText;
	string timestamp;

	historyItem_t ( string commandText_in, string timestamp_in ) :
		commandText( commandText_in ), timestamp( timestamp_in ) {}
};

struct terminalState_t {
	// display extents
	const int baseX = 10;
	const int baseY = 5;
	const int width = 160;
	const int height = 75;

	// input cursor - 2d would be interesting
	int cursorX = 0;
	int cursorY = 0;

	// lines of input history
	std::vector< historyItem_t > history;

	// the current input line
	string currentLine;

	// command prompt presented to the user
	string promptString = string( "> " );

	terminalState_t () {
		history.push_back( historyItem_t( "Welcome to jbDE", fixedWidthTimeString() + string( ": " ) ) );
	}

	bool isDivider ( char c ) {
		// tbd, which ones of these to use
		string dividers = string( " ,./\\'\"!@#$%^&*()_+{}[]/?-=<>:;|`~" );
		for ( auto& symbol : dividers )
			if ( c == symbol )
				return true;

		return false;
	}

	void backspace ( bool control ) {
		if ( control ) {
			do { // erase till you hit whitespace
				cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
				currentLine.erase( currentLine.begin() + cursorX );
			} while ( !isDivider( currentLine[ cursorX - 1 ] ) && ( cursorX - 1 >= 0 ) );
		} else { // remove char before the cursor
			cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
			if ( currentLine.length() > 0 )
				currentLine.erase( currentLine.begin() + cursorX );
		}
	}

	void deleteKey ( bool control ) {
		if ( control ) {
			do {
				if ( currentLine.length() > 0 && cursorX < int( currentLine.length() ) ) {
					currentLine.erase( currentLine.begin() + cursorX );
				}
			} while ( !isDivider( currentLine[ cursorX ] ) && cursorX < int( currentLine.length() ) );
		} else {
			// remove the char at the cursor
			if ( currentLine.length() > 0 && cursorX < int( currentLine.length() ) )
				currentLine.erase( currentLine.begin() + cursorX );
		}
	}

	void enter () {
		// push this line onto the history
		history.push_back( historyItem_t( currentLine, fixedWidthTimeString() + string( ": " ) ) );

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

	void home () {
		cursorX = 0;
	}

	void end () {
		cursorX = currentLine.length();
	}

	void addChar ( char c ) {
		// add this char at the cursor
		currentLine.insert( currentLine.begin() + cursorX, c );
		cursorX++;
	}

};