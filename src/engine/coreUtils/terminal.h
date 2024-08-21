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

// variable/type definition
enum type_e {
	BOOL = 0,

	INT = 1,
	IVEC2 = 2,
	IVEC3 = 3,
	IVEC4 = 4,

	FLOAT = 5,
	VEC2 = 6,
	VEC3 = 7,
	VEC4 = 8,

	STRING = 9
};

const static inline string getStringForType ( type_e type_in ) {
	const string typeStrings[] = { "bool", "int", "int2", "int3", "int4", "float", "vec2", "vec3", "vec4", "string" };
	return typeStrings[ int( type_in ) ];
}

struct var_t {
	string label;
	type_e type;
	vec4 data = vec4( 0.0f );
	string stringData;
	string description;

	var_t( string label_in, type_e type_in, string description_in ) :
		label( label_in ), type( type_in ), description( description_in ) {}
};

// command definition
struct command_t {
	std::vector< string > labelAndAliases;
	std::vector< var_t > arguments;
	std::function< void( std::vector< var_t > arguments ) > func;
	string description;

	// does the input string match this command or any of its aliases?
	bool commandStringMatch ( string commandString ) {
		for ( auto& cantidate : labelAndAliases ) {
			if ( commandString == cantidate ) {
				return true;
			}
		}
		return false;
	}

	string getAliasString () {
		string value;
		// we have one or more aliases
		for ( uint i = 0; i < labelAndAliases.size(); i++ ) {
			value += labelAndAliases[ i ] + ", ";
		}
		// last ", " needs to come off
		value.pop_back();
		value.pop_back();
		return value;
	}

	void invoke ( std::vector< var_t > arguments_in ) { // use the local arguments?
		func( arguments_in );
	}
};

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

	void addLineBreak () {
		addHistoryLine( csb.flush() );
	}

	// cvars stuff...
		// from before:

	// 	// testing some cvar stuff
	// 		terminal.addCvar( { "testCvarString", STRING, vec4( 0.0f ), "Nut" } );
	// 		terminal.addCvar( { "testCvarFloat", FLOAT, vec4( 1.0f ), "" } );

	// 		terminal.addCommand( { "cvars", [=] () {
	// 			string labels[] = { "(bool)", "(int)", "(int2)", "(int3)", "(int4) ", "(float)", "(vec2)", "(vec3)", "(vec4)", "(string)" };
	// 			for ( uint i = 0; i < terminal.cvars.size(); i++ ) {
	// 				cCharString temp;
	// 				temp.append( "  " + terminal.cvars[ i ].label + " " + labels[ terminal.cvars[ i ].type ] + " " + terminal.cvars[ i ].getStringRepresentation() + " " );
	// 				terminal.history.push_back( temp );
	// 			}
	// 		}, "List all the active cvars, as well as their types and values." } );

	std::vector< command_t > commands;
	void addCommand ( std::vector< string > commandAndOptionalAliases_in,
		std::vector< var_t > argumentList_in,
		std::function< void( std::vector< var_t > arguments ) > func_in,
		string description_in ) {

			// fill out the struct
			command_t command;
			command.labelAndAliases = commandAndOptionalAliases_in;
			command.arguments = argumentList_in;
			command.func = func_in;
			command.description = description_in;

			commands.push_back( command );
	}

	// adding some default set of commands
	void addTerminalLocalCommands () {

		// cannot do e.g. quit here, because of lack of access to pQuit
			// will need to add another step to init, where engine commands are added

		addCommand(
			{ "list", "ls" }, {}, // command and aliases, arguments ( if any, none here )
			[=] ( std::vector< var_t > arguments ) {
				addLineBreak();
				for ( uint i = 0; i < commands.size(); i++ ) {
					addHistoryLine( csb.append( "  ============================================" ).flush() );
					addHistoryLine( csb.append( "  > Command Aliases: " ).append( commands[ i ].getAliasString(), 1 ).flush() );
					addHistoryLine( csb.append( "    Description: " ).append( commands[ i ].description, 1 ).flush() );
					addHistoryLine( csb.append( "    Arguments:" ).flush() );
					if ( commands[ i ].arguments.size() > 0 ) {
						for ( auto& var : commands[ i ].arguments ) {
							addHistoryLine( csb.append( "      " + var.label + " ( ", 2 ).append( getStringForType( var.type ), 1 ).append( " ): ", 2 ).append( var.description, 1 ).flush() );
						}
						addLineBreak();
					} else {
						addHistoryLine( csb.append( "    None", 1 ).flush() );
						addLineBreak();
					}
				}
			}, "List all the active commands in this terminal." );

		// test with several different arguments
		addCommand(
			{ "test", "ts", "ts1" },
			{ // arguments
				var_t( "arg1", BOOL, "This is a first test bool." ),
				var_t( "arg2", BOOL, "This is a second test bool." ),
				var_t( "arg3", BOOL, "This is a third test bool." ),
				var_t( "arg4", BOOL, "This is a fourth test bool." )
			},
			[=] ( std::vector< var_t > arguments ) {
				// if this function did anything, it would be here
			}, "I am a test command and I don't do a whole hell of a lot." );

		// clearing history
		addCommand( { "clear" }, {},
			[=] ( std::vector< var_t > arguments ) {
				history.clear();
			}, "Clear the terminal history." );

		// setting color presets
		addCommand( { "colorPreset" }, {
				{ "select", INT, "Which palette you want to use." }
			}, [=] ( std::vector< var_t > arguments ) {
				// I would like to have the string indexing back...
				csb.selectedPalette = int( arguments[ 0 ].data.x );
			}, "Numbered presets (0-3)." );

		addCommand( { "echo" },
			{ // parameters list
				{ "string", STRING, "The string in question." }
			},
			[=] ( std::vector< var_t > arguments ) {
				history.push_back( csb.append( arguments[ 0 ].stringData, 4 ).flush() );
			}, "Report back the given string argument." );
	}

	// is this valid input for this command
	bool parseForCommand ( int commandIdx, string argumentString ) {

		// so we have matched with command number commandIdx

		// we need to see if we can parse its arguments
		std::stringstream argstream( argumentString );
		bool fail = false;

		// for each argument
		for ( uint i = 0; i < commands[ commandIdx ].arguments.size() && !fail; i++ ) {

			// temp buffers to read into
			bool tempb = false;
			int tempi = 0;
			float tempf = 0.0f;
			string temps;

			int count = 0;
			switch ( commands[ commandIdx ].arguments[ i ].type ) { // for each arg
				case BOOL:
					// read in a bool, put it in x
					if ( ( argstream >> std::boolalpha >> tempb ) ) {
						commands[ commandIdx ].arguments[ i ].data[ 0 ] = tempb ? 1.0f : 0.0f;
					} else {
						argstream.clear();
						fail = true;
					}
				break;

				case IVEC4:	count++;
				case IVEC3:	count++;
				case IVEC2:	count++;
				case INT:	count++;
					// read in up to four ints, put it in xyzw
					for ( int j = 0; j < count; j++ ) {
						if ( ( argstream >> tempi ) ) {
							commands[ commandIdx ].arguments[ i ].data[ j ] = tempi;
						} else {
							cout << "int parse error" << endl;
							argstream.clear();
							fail = true;
						}
					}
				break;

				case VEC4:	count++;
				case VEC3:	count++;
				case VEC2:	count++;
				case FLOAT:	count++;
					// read in up to four floats, put it in xyzw
					for ( int j = 0; j < 4; j++ ) {
						if ( ( argstream >> tempf ) ) {
							commands[ commandIdx ].arguments[ i ].data[ j ] = tempf;
						} else {
							cout << "float parse error" << endl;
							argstream.clear();
							fail = true;
						}
					}
				break;

				case STRING:
					if ( ( argstream >> temps ) ) { /* neat */ } else {
						cout << "string parse error" << endl;
						argstream.clear();
						fail = true;
					}
				break;

				default:
				break;
			}
		}

		if ( fail ) {
			return false;
		} else {
			return true;
		}
	}

	// init with startup message
	terminal_t () {
		// some initial setup - set color palette to be used
		csb.selectedPalette = 2;

		// output welcome string
		addHistoryLine( csb.timestamp().append( " Welcome to jbDE", 4 ).flush() );

		// add terminal-local commands and cvars
			// manipulating base point, width, height, etc
		addTerminalLocalCommands();

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

	// scrolling
		if ( currentInputState.getState4( KEY_PAGEUP ) == KEYSTATE_RISING ) { pageup(); }
		if ( currentInputState.getState4( KEY_PAGEDOWN ) == KEYSTATE_RISING ) { pagedown(); }

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

	// scrolling the output up and down, to see further back in time
	int scrollOffset = 0;
	void pageup () { scrollOffset++; }
	void pagedown () { scrollOffset--; }

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

		// clear the input, reset cursor, scroll offset
		currentLine.clear();
		cursorX = 0;
		scrollOffset = 0;

		if ( userInputString.length() ) {

			// now breaking this up a little bit - we consider this sequence up to the first space as the command
			string commandText = userInputString.substr( 0, userInputString.find( " " ) );
			string argumentText = userInputString.erase( 0, commandText.length() + 1 );

			// todo - try to match a cvar
				// if you do, report the type and value of that cvar

			// try to match a command
			bool commandFound = false;
			for ( uint i = 0; i < commands.size(); i++ ) {
				// if you do, try to parse the rest of the input string for the arguments to that command
				if ( commands[ i ].commandStringMatch( commandText ) ) {
					commandFound = true;
					if ( parseForCommand( i, argumentText ) ) {

						// we successfully parsed the arguments for the command, so we can go ahead and run it
						commands[ i ].invoke( commands[ i ].arguments );

					} else {

						// do the full report...
						addLineBreak();
						addHistoryLine( csb.append( "  Error: ", 4 ).append( "command \"" + commandText + "\" argument parse failed...", 3 ).flush() );
						addLineBreak();
						if ( !commands[ i ].arguments.empty() ) {
							addHistoryLine( csb.append( "  Command Aliases: " ).append( commands[ i ].getAliasString(), 1 ).flush() );
							addHistoryLine( csb.append( "  Description: " ).append( commands[ i ].description, 1 ).flush() );
							addHistoryLine( csb.append( "  Arguments:" ).flush() );
							for ( auto& var : commands[ i ].arguments ) {
								addHistoryLine( csb.append( "    " + var.label + " ( ", 2 ).append( getStringForType( var.type ), 1 ).append( " ): ", 2 ).append( var.description, 1 ).flush() );
							}
							addLineBreak();
						} else {
							// with no arguments... should not be possible to fail this way
						}

					}
				}
			}
			if ( !commandFound ) {
				addHistoryLine( csb.append( "Error: ", 4 ).append( "command " + commandText + " not found", 3 ).flush() );
			}
		}
	}
};