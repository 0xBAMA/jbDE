#include "../includes.h"

struct cChar {
	unsigned char data[ 4 ] = { 255, 255, 255, 0 };
	cChar() {}
	cChar( unsigned char c ) {
		data[ 3 ] = c;
	}
	cChar( glm::ivec3 color, unsigned char c ) {
		data[ 0 ] = color.x;
		data[ 1 ] = color.y;
		data[ 2 ] = color.z;
		data[ 3 ] = c;
	}
};

struct coloredStringBuilder {
	// cChar data directly
	std::vector< cChar > data;

	static inline int selectedPalette;
	static constexpr ivec3 colorsets[ 4 ][ 5 ] = {
	{ // ===================
		{  17,  35,  24 },
		{  72, 120,  40 },
		{ 137, 162,  87 },
		{ 191, 146,  23 },
		{ 166, 166, 166 } },
	{ // ===================
		{  58,   0,   0 },
		{ 124,   3,   0 },
		{ 224,  66,  23 },
		{ 242, 109,  31 },
		{ 166, 166, 166 } },
	{ // ===================
		{   7,   8,  16 },
		{  82, 165, 222 },
		{ 172, 214, 246 },
		{ 235, 249, 255 },
		{ 166, 166, 166 } },
	{ // ===================
		{  19,   2,   8 },
		{  70,  14,  43 },
		{ 213,  60, 106 },
		{ 255, 130, 116 },
		{ 166, 166, 166 } } };

	// clear out the data
	void reset () { data.clear(); }

// chainable operations:

	// add a colored timestamp
	coloredStringBuilder& timestamp () {
		string timestamp = fixedWidthTimeString();
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 0 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 1 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 2 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 3 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 4 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 5 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 6 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 7 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 3 ], timestamp[ 8 ] ) );
		append( cChar( colorsets[ selectedPalette ][ 1 ], timestamp[ 9 ] ) );
		return *this;
	}

	// trim N cChars off of the vector
	coloredStringBuilder& trim ( int amt ) {
		for ( int i = 0; i < amt; i++ ) {
			data.pop_back();
		}
		return *this;
	}

	// operate in cChars directly
	coloredStringBuilder& append ( cChar val ) {
		data.push_back( val );
		return *this;
	}

	// basic append with color select
	coloredStringBuilder& append ( string string_in, ivec3 color = colorsets[ selectedPalette ][ 4 ] ) {
		for ( auto& letter : string_in ) {
			cChar val = cChar( color, letter );
			append( val );
		}
		return *this;
	}

	// integer reference of the current palette
	coloredStringBuilder& append ( string string_in, int color ) {
		append( string_in, colorsets[ selectedPalette ][ color ] );
		return *this;
	}

	// other append options?

	// this comes at the end of a chain of operations, where it will return the data and then reset the state
	std::vector< cChar > flush () {

		// this is a hack, I'm not sure why I'm having to pad the string... last character flickers, otherwise
		append( " " ); // probably something to do with some bounds checking somewhere...

		// make a copy of the data
		std::vector<cChar> dataCopy( data );

		// clear the held data
		reset();

		// return the copy
		return dataCopy;
	}
};

// variable definition

// command definition

struct terminal_t {

	// eventually move this stuff onto a struct:
	// =========================================

		// location of lower left corner
		uvec2 basePt = uvec2( 10, 5 );

		// size in glyphs
		uvec2 dims = uvec2( 120, 42 );

		// is this terminal active
		bool active = true;

		// navigation cursor
		int cursorX = 0;
		int cursorY = 0;

	// =========================================

	// formatting utility
	coloredStringBuilder csb;

	// lines of history, renderered directly
	std::vector< std::vector< cChar > > history;

	// current input buffer
	string currentLine;

	// add a line to the history
	void addHistoryLine( std::vector< cChar > line ) {
		history.push_back( line );
	}

	// init with startup message
	terminal_t () {
		csb.selectedPalette = 0;
		addHistoryLine( csb.timestamp().append( " Welcome to jbDE", 4 ).flush() );

		// add terminal-local commands and cvars
			// manipulating base point, width, height, etc

		// some of these things changing will involve clearing the entire buffer in the text renderer
			// so either: signal that... or be clearing every time already anyways because it's not a major perf hit
	}


	// handle input, etc
	void update ( inputHandler_t &currentInputState ) {

	// modifier keys
		const bool control = currentInputState.getState( KEY_LEFT_CTRL ) || currentInputState.getState( KEY_RIGHT_CTRL );
		const bool shift = currentInputState.getState( KEY_LEFT_SHIFT ) || currentInputState.getState( KEY_RIGHT_SHIFT );

	// navigation through history
		if ( currentInputState.getState4( KEY_UP ) == KEYSTATE_RISING ) { cursorUp(); }
		if ( currentInputState.getState4( KEY_DOWN ) == KEYSTATE_RISING ) { cursorDown(); }

	// navigation within line
		if ( currentInputState.getState4( KEY_LEFT ) == KEYSTATE_RISING ) { cursorLeft( control ); }
		if ( currentInputState.getState4( KEY_RIGHT ) == KEYSTATE_RISING ) { cursorRight( control ); }
		if ( currentInputState.getState4( KEY_HOME ) == KEYSTATE_RISING ) { home(); }
		if ( currentInputState.getState4( KEY_END ) == KEYSTATE_RISING ) { end(); }

	// control inputs
		if ( currentInputState.getState4( KEY_BACKSPACE ) == KEYSTATE_RISING ) { backspace( control ); }
		if ( currentInputState.getState4( KEY_ENTER ) == KEYSTATE_RISING ) { enter(); }
		if ( currentInputState.getState4( KEY_DELETE ) == KEYSTATE_RISING ) { deleteKey( control ); }
		if ( currentInputState.getState4( KEY_SPACE ) == KEYSTATE_RISING ) { addChar( ' ' ); }

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
		if ( currentInputState.getState4( KEY_PERIOD ) == KEYSTATE_RISING ) { addChar( shift ? '>' : '.' ); }
		if ( currentInputState.getState4( KEY_COMMA ) == KEYSTATE_RISING ) { addChar( shift ? '<': ',' ); }
		if ( currentInputState.getState4( KEY_SLASH ) == KEYSTATE_RISING ) { addChar( shift ? '?': '/' ); }
		if ( currentInputState.getState4( KEY_SEMICOLON ) == KEYSTATE_RISING ) { addChar( shift ? ':': ';' ); }
		if ( currentInputState.getState4( KEY_BACKSLASH ) == KEYSTATE_RISING ) { addChar( shift ? '|' : '\\' ); }
		if ( currentInputState.getState4( KEY_LEFT_BRACKET ) == KEYSTATE_RISING ) { addChar( shift ? '{' : '[' ); }
		if ( currentInputState.getState4( KEY_RIGHT_BRACKET ) == KEYSTATE_RISING ) { addChar( shift ? '}' : ']' ); }
		if ( currentInputState.getState4( KEY_APOSTROPHE ) == KEYSTATE_RISING ) { addChar( shift ? '"' : '\'' ); }
		if ( currentInputState.getState4( KEY_GRAVE ) == KEYSTATE_RISING ) { addChar( shift ? '~' : '`' ); }
		if ( currentInputState.getState4( KEY_MINUS ) == KEYSTATE_RISING ) { addChar( shift ? '_' : '-' ); }
		if ( currentInputState.getState4( KEY_EQUALS ) == KEYSTATE_RISING ) { addChar( shift ? '+' : '=' ); }
	}

	void addChar ( char c ) {
		// add this char at the cursor
		currentLine.insert( currentLine.begin() + cursorX, c );
		cursorX++;
	}

	// helper function for control modifier on nav + backspace/delete
	bool isDivider ( char c ) {
		// tbd, which ones of these to use
		string dividers = string( " ,./\\'\"!@#$%^&*()_+{}[]/?-=<>:;|`~" );
		for ( auto& symbol : dividers )
			if ( c == symbol )
				return true;

		return false;
	}

	// cursor keys - up/down history navigation wip
	int historyOffset = 0;
	std::vector< string > userInputs;
	void cursorUp () {

	}
	void cursorDown () {

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

	// more navigation
	void home () { cursorX = 0; }
	void end () { cursorX = currentLine.length(); }

	// further manipulation of current input line
	void backspace ( bool control ) {
		if ( control ) {
			if ( currentLine.length() > 0 ) {
				do { // erase till you hit whitespace
					cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
					currentLine.erase( currentLine.begin() + cursorX );
				} while ( !isDivider( currentLine[ cursorX - 1 ] ) && ( cursorX - 1 >= 0 ) );
			}
		} else { // remove char before the cursor
			cursorX = std::clamp( cursorX - 1, 0, int( currentLine.length() ) );
			if ( currentLine.length() > 0 ) {
				currentLine.erase( currentLine.begin() + cursorX );
			}
		}
	}

	void deleteKey ( bool control ) {
		if ( control ) {
			if ( currentLine.length() > 0 ) {
				do {
					if ( currentLine.length() > 0 && cursorX < int( currentLine.length() ) ) {
						currentLine.erase( currentLine.begin() + cursorX );
					}
				} while ( !isDivider( currentLine[ cursorX ] ) && cursorX < int( currentLine.length() ) );
			}
		} else {
			// remove the char at the cursor
			if ( currentLine.length() > 0 && cursorX < int( currentLine.length() ) ) {
				currentLine.erase( currentLine.begin() + cursorX );
			}
		}
	}

	void enter () {
		// add a timestamped history element
		addHistoryLine( csb.timestamp().append( " " ).append( currentLine ).flush() );

		// cache the current input line as a string
		string userInputString = currentLine;

		// clear the input, reset cursor
		currentLine.clear();
		cursorX = 0;

		// now breaking this up a little bit - we consider this sequence up to the first space as the command
		string commandText = userInputString.substr( 0, userInputString.find( " " ) );
		string argumentText = userInputString.erase( 0, commandText.length() + 1 );

		// debug currently
		addHistoryLine( csb.append( "  recognized command:" ).append( commandText ).flush() );
		addHistoryLine( csb.append( "  recognized args:" ).append( argumentText ).flush() );

		// try to match a cvar
			// if you do, report the type and value of that cvar

		// try to match a command
			// if you do, try to parse the rest of the input string for the arguments to that command
	}
};