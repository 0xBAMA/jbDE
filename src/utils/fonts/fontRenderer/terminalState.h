#include "../../../engine/includes.h"

struct historyItem_t {
	string commandText;
	string timestamp;

	historyItem_t ( string commandText_in, string timestamp_in ) :
		commandText( commandText_in ), timestamp( timestamp_in ) {}
};

struct terminalState_t {
	// is this taking input, etc
	bool active = true;

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

	void update ( inputHandler_t &currentInputState ) {

	// navigation through history
		// if ( currentInputState.getState( KEY_UP ) ) {
			// cursorY++;
		// }
		// if ( currentInputState.getState( KEY_DOWN ) ) {
			// cursorY--;
		// }

		const bool control = currentInputState.getState( KEY_LEFT_CTRL ) || currentInputState.getState( KEY_RIGHT_CTRL );
		const bool shift = currentInputState.getState( KEY_LEFT_SHIFT ) || currentInputState.getState( KEY_RIGHT_SHIFT );

	// navigation within line
		if ( currentInputState.getState4( KEY_LEFT ) == KEYSTATE_RISING ) {
			cursorLeft( control );
		}
		if ( currentInputState.getState4( KEY_RIGHT ) == KEYSTATE_RISING ) {
			cursorRight( control );
		}
		if ( currentInputState.getState4( KEY_HOME ) == KEYSTATE_RISING ) {
			home();
		}
		if ( currentInputState.getState4( KEY_END ) == KEYSTATE_RISING ) {
			end();
		}


	// control inputs
		if ( currentInputState.getState4( KEY_BACKSPACE ) == KEYSTATE_RISING ) {
			backspace( control );
		}
		if ( currentInputState.getState4( KEY_ENTER ) == KEYSTATE_RISING ) {
			enter();
		}
		if ( currentInputState.getState4( KEY_DELETE ) == KEYSTATE_RISING ) {
			deleteKey( control );
		}
		if ( currentInputState.getState4( KEY_SPACE ) == KEYSTATE_RISING ) {
			addChar( ' ' );
		}

	// char input
		if ( currentInputState.stateBuffer[ currentInputState.currentOffset ].alphasActive() ) { // is this check useful?
			string letterString = string( " abcdefghijklmnopqrstuvwxyz" );
			for ( int i = 1; i <= 26; i++ ) {
				if ( currentInputState.getState4( ( keyName_t ) i ) == KEYSTATE_RISING ) {
					addChar( shift ? toupper( letterString[ i ] ) : letterString[ i ] );
				}
			}
		}

	// numbers
		string numberString = string( "0123456789" );
		string shiftedString = string( ")!@#$%^&*(" );
		for ( int i = 27; i < 37; i++ ) {
			if ( currentInputState.getState4( ( keyName_t ) i ) == KEYSTATE_RISING ) {
				addChar( shift ? shiftedString[ i - 27 ] : numberString[ i - 27 ] );
			}
		}

	// punctuation and assorted other shit
		if ( currentInputState.getState4( KEY_PERIOD ) == KEYSTATE_RISING ) {
			addChar( shift ? '>' : '.' );
		}
		if ( currentInputState.getState4( KEY_COMMA ) == KEYSTATE_RISING ) {
			addChar( shift ? '<': ',' );
		}
		if ( currentInputState.getState4( KEY_SLASH ) == KEYSTATE_RISING ) {
			addChar( shift ? '?': '/' );
		}
		if ( currentInputState.getState4( KEY_SEMICOLON ) == KEYSTATE_RISING ) {
			addChar( shift ? ':': ';' );
		}
		if ( currentInputState.getState4( KEY_BACKSLASH ) == KEYSTATE_RISING ) {
			addChar( shift ? '|' : '\\' );
		}
		if ( currentInputState.getState4( KEY_LEFT_BRACKET ) == KEYSTATE_RISING ) {
			addChar( shift ? '{' : '[' );
		}
		if ( currentInputState.getState4( KEY_RIGHT_BRACKET ) == KEYSTATE_RISING ) {
			addChar( shift ? '}' : ']' );
		}
		if ( currentInputState.getState4( KEY_APOSTROPHE ) == KEYSTATE_RISING ) {
			addChar( shift ? '"' : '\'' );
		}
		if ( currentInputState.getState4( KEY_GRAVE ) == KEYSTATE_RISING ) {
			addChar( shift ? '~' : '`' );
		}
		if ( currentInputState.getState4( KEY_MINUS ) == KEYSTATE_RISING ) {
			addChar( shift ? '_' : '-' );
		}
		if ( currentInputState.getState4( KEY_EQUALS ) == KEYSTATE_RISING ) {
			addChar( shift ? '+' : '=' );
		}
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