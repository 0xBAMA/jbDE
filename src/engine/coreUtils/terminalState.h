#include "../includes.h"

struct historyItem_t {
	string commandText;
	string timestamp;

	historyItem_t ( string commandText_in, string timestamp_in ) :
		commandText( commandText_in ), timestamp( timestamp_in ) {}
};

struct command_t {
	string commandName;
	std::function< void() > func;

	command_t ( string commandName_in, std::function< void() > func_in ) :
		commandName( commandName_in ), func( func_in ) {}

	void invoke () {
		func();
	}
};

enum argType {
	BOOL = 0, INT = 1 // ...
};

struct argStruct_t {
	string label;
	argType type;
	vec4 data = vec4( 0.0f );
};

struct argList_t {
	std::vector< argStruct_t > args;
	argList_t ( std::vector< argStruct_t > args_in ) :
		args( args_in ) {}

	size_t count () { return args.size(); }

	argStruct_t operator [] ( string label ) {
		for ( uint i = 0; i < args.size(); i++ ) {
			if ( args[ i ].label == label ) {
				return args[ i ];
			}
		}
		return argStruct_t();
	}

	argStruct_t operator [] ( int idx ) {
		if ( idx < count() ) {
			return args[ idx ];
		} else {
			return argStruct_t();
		}
	}
};

struct commandWithArgs_t {
	string commandName;
	argList_t args;
	std::function< void( argList_t ) > func;

	commandWithArgs_t ( string commandName_in, std::vector< argStruct_t > args_in, std::function< void( argList_t ) > func_in ) :
		commandName( commandName_in ), args( args_in ), func( func_in ) {}

	void invoke () {
		func( args );
	}
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

	// handling of commands with no arguments
	std::vector< command_t > commands;
	void addCommand ( command_t command ) {
		commands.push_back( command );
	}

	// handling commands with arguments
	std::vector< commandWithArgs_t > commandsWithArgs;
	void addCommand ( commandWithArgs_t command ) {
		commandsWithArgs.push_back( command );
	}

	// init with a welcome message
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

		// command string, as given
		string commandString = currentLine;

		// clear the input line
		currentLine.clear();

		// reset the cursor
		cursorX = 0;

		string commandName = commandString.substr( 0, commandString.find( " " ) );

		// does this match any of the registered commands with no args
		bool matchFound = false;
		for ( size_t i = 0; i < commands.size(); i++ ) {
			if ( commands[ i ].commandName == commandName ) {
				matchFound = true;
				commands[ i ].invoke();
			}
		}

		// how about registered commands with args
		for ( size_t i = 0; i < commands.size(); i++ ) {
			if ( commandsWithArgs[ i ].commandName == commandName ) {
				matchFound = true;

				// get the labels, types
				argList_t args = commandsWithArgs[ i ].args;

				for ( int i = 0; i < args.count(); i++ ) {
					switch ( args[ i ].type ) { // for each arg
						case BOOL:
							// read in a bool
							// put it in args[ i ].data.x
						break;

						case INT:
							// read in an int
							// put it in args[ i ].data.x
						break;

						// more

					}

					// parse the arguments, into the struct
						// input validation needs to happen here, too - report and break if failing
				}

				// commandsWithArgs[ i ].args = args;
				commandsWithArgs[ i ].invoke();
			}
		}

		// made it through the lists without finding a match... say so
		if ( !matchFound ) {
			history.push_back( historyItem_t( "  Command " + commandName + " not found", "" ) );
		}
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