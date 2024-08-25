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

	STRING = 9,
	CVAR_NAME = 10
};

const static inline string getStringForType ( type_e type_in ) {
	const string typeStrings[] = { "bool", "int", "int2", "int3", "int4", "float", "vec2", "vec3", "vec4", "string", "cvar_name" };
	return typeStrings[ int( type_in ) ];
}

struct var_t {
	string label;
	type_e type;
	vec4 data = vec4( 0.0f );
	string stringData;
	string description;

	var_t() {}
	var_t( string label_in, type_e type_in, string description_in ) :
		label( label_in ), type( type_in ), description( description_in ) {}

	string getStringRepresentation () {
		switch ( type ) {
			case BOOL: return ( data.x == 0.0f ) ? string( "false" ) : string( "true" ); break;

			case INT: return to_string( int( data.x ) ); break;
			case IVEC2: return string( "(" ) + to_string( int( data.x ) ) + string( " " ) + to_string( int( data.y ) ) + string( ")" ); break;
			case IVEC3: return string( "(" ) + to_string( int( data.x ) ) + string( " " ) + to_string( int( data.y ) ) + string( " " ) + to_string( int( data.z ) ) + string( ")" ); break;
			case IVEC4: return string( "(" ) + to_string( int( data.x ) ) + string( " " ) + to_string( int( data.y ) ) + string( " " ) + to_string( int( data.z ) ) + string( " " ) + to_string( int( data.w ) ) + string( ")" ); break;

			case FLOAT: return to_string( data.x ); break;
			case VEC2: return string( "(" ) + to_string( data.x ) + string( " " ) + to_string( data.y ) + string( ")" ); break;
			case VEC3: return string( "(" ) + to_string( data.x ) + string( " " ) + to_string( data.y ) + string( " " ) + to_string( data.z ) + string( ")" ); break;
			case VEC4: return string( "(" ) + to_string( data.x ) + string( " " ) + to_string( data.y ) + string( " " ) + to_string( data.z ) + string( " " ) + to_string( data.w ) + string( ")" ); break;

			case STRING: return stringData; break;
			default: return string(); break;
		}
	}
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

struct cvarManager_t {
	var_t dummyVar;
	std::vector< var_t > vars;

	void add ( var_t var ) {
		vars.push_back( var );
	}

	// operator to get by string
	var_t& operator[] ( string label ) {
		for ( uint i = 0; i < vars.size(); i++ ) {
			if ( vars[ i ].label == label ) {
				return vars[ i ];
			}
		}
		// mostly just for warning supression...
		return dummyVar;
	}

	// operator to get by index
	var_t& operator[] ( int i ) {
		return vars[ i ];
	}

	size_t count () {
		return vars.size();
	}

	// does this string refer to a valid cvar?
	bool isValid ( string label ) {
		for ( uint i = 0; i < vars.size(); i++ ) {
			if ( vars[ i ].label == label ) {
				return true;
			}
		}
		return false;
	}
};

struct terminal_t {

	// eventually move this stuff onto a struct:
	// =========================================

		// location of lower left corner
		uvec2 basePt = uvec2( 10, 5 );

		// size in glyphs
		uvec2 dims = uvec2( 300, 80 );

		// is this terminal active
		bool active = true;

		// navigation cursor
		int cursorX = 0;
		int cursorY = 0;

	// =========================================

	// formatting utility
	coloredStringBuilder csb;

	// tracking if we need to rebuild the list of strings for tab completion
	DictionaryTrie * trie = new DictionaryTrie();
	DictionaryTrie * trieCvar = new DictionaryTrie();

	// corresponding flat vector, all the strings that got added to that acceleration structure
	std::vector< string > allStrings;

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

	cvarManager_t cvars;
	void addCvar ( string label, type_e type, string description ) {
		cvars.add( var_t( label, type, description ) );

		// insert into the main list, and also a second list, for autocompleting arguments
		trie->insert( label, 100 );
		trieCvar->insert( label, 100 );

		// add this also to the list of all strings, and alphabetize (for reporting on tab complete with empty input prompt)
		// allStrings.push_back( string( "$" ) + label );
		allStrings.push_back( label );
		sort( allStrings.begin(), allStrings.end() );
	}

	// how do we want to expose:

		// assignment of value (handling of different types?)
			// assignment specialization for every type, I guess...

		// retrieval of value
			// this should return a copy of the var_t struct, I think

		// ...

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

			// add command names
			for ( uint i = 0; i < commandAndOptionalAliases_in.size(); i++ ) {
				trie->insert( commandAndOptionalAliases_in[ i ], 100 );

				// add this also to the list of all strings, and alphabetize (for reporting on tab complete with empty input prompt)
				allStrings.push_back( commandAndOptionalAliases_in[ i ] );
				sort( allStrings.begin(), allStrings.end() );
			}
	}

	// adding some default set of commands
	void addTerminalLocalCommands () {

		// cannot do e.g. quit here, because of lack of access to pQuit
			// will need to add another step to init, where engine commands are added

		// setting color presets
		addCommand( { "colorPreset" }, {
				{ "select", INT, "Which palette you want to use." }
			}, [=] ( std::vector< var_t > arguments ) {
				// I would like to have the string indexing back...
				csb.selectedPalette = int( arguments[ 0 ].data.x );
			}, "Numbered presets (0-3)." );

		addCommand( { "echo", "e" },
			{ // parameters list
				{ "string", STRING, "The string in question." }
			},
			[=] ( std::vector< var_t > arguments ) {
				history.push_back( csb.append( arguments[ 0 ].stringData, 4 ).flush() );
			}, "Report back the given string argument." );

		// clearing history
		addCommand( { "clear" }, {},
			[=] ( std::vector< var_t > arguments ) {
				history.clear();
			}, "Clear the terminal history." );

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
			{ "test", "ts" },
			{ // arguments
				var_t( "arg1", BOOL, "This is a first test bool." ),
				var_t( "arg2", BOOL, "This is a second test bool." ),
				var_t( "arg3", BOOL, "This is a third test bool." ),
				var_t( "arg4", BOOL, "This is a fourth test bool." )
			},
			[=] ( std::vector< var_t > arguments ) {
				// if this function did anything, it would be here
			}, "I am a test command and I don't do a whole lot." );

		// testing some cvar stuff
		addCvar( "testCvarString", STRING, "This is a test cvar, it's a string." );
		addCvar( "testCvarFloat", FLOAT, "This is a test cvar, it's a float." );

		// setting the values of these test cvars
		cvars[ "testCvarString" ].stringData = string( "testCvarStringValue" );
		cvars[ "testCvarFloat" ].data.x = 1.0f;

		addCommand( { "cvars" }, {}, [=] ( std::vector< var_t > arguments ) {
			addHistoryLine( csb.flush() );
			for ( uint i = 0; i < cvars.count(); i++ ) {
				addHistoryLine( csb.append( "  " + cvars[ i ].label + " ( ", 2 ).append( getStringForType( cvars[ i ].type ), 1 ).append( " )", 2 ).flush() );
				addHistoryLine( csb.append( "    Description: ", 4 ).append( cvars[ i ].description, 1 ).flush() );
				addHistoryLine( csb.append( "    Value: ", 4 ).append( cvars[ i ].getStringRepresentation(), 3 ).flush() );
				addHistoryLine( csb.flush() );
			}
		}, "List all the active cvars, as well as their types and values." );
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

			// signalling value for a cvar
			if ( argstream.peek() == '$' ) {

				// read in the string, including this signalling character
				string cantidateString;
				argstream >> cantidateString;

				// get rid of '$'
				cantidateString.erase( 0, 1 );

				// so we want to see that it's an existing cvar
				if ( cvars.isValid( cantidateString ) ) {
					// check for matching type
					if ( commands[ commandIdx ].arguments[ i ].type == cvars[ cantidateString ].type ) {
						// then copy the value(s) from the cvar
						commands[ commandIdx ].arguments[ i ].data = cvars[ cantidateString ].data;
						commands[ commandIdx ].arguments[ i ].stringData = cvars[ cantidateString ].stringData;
					} else {
						// types do not match
						argstream.clear();
						fail = true;
					}
				} else {
					// we had some issue, this isn't a valid cvar
					argstream.clear();
					fail = true;
				}

			} else {
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
		csb.selectedPalette = 0;

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
		if ( currentInputState.getState4( KEY_PAGEUP ) == KEYSTATE_RISING ) { pageup( shift ); }
		if ( currentInputState.getState4( KEY_PAGEDOWN ) == KEYSTATE_RISING ) { pagedown( shift ); }

	// tab
		if ( currentInputState.getState4( KEY_TAB ) == KEYSTATE_RISING ) { tab(); };

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
		resetInputHistoryBrowse();
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

	// helper function for tab complete
	string getLastToken ( string str ) {
		while ( !str.empty() && std::isspace( str.back() ) )
			str.pop_back() ; // remove trailing white space

		const auto pos = str.find_last_of( " \t\n" ) ; // locate the last white space

		// if not found, return the entire string else return the tail after the space
		return pos == std::string::npos ? str : str.substr( pos + 1 ) ;
	}

	// cursor keys - up/down history navigation
	int historyOffset = 0;
	string clicache;
	std::vector< string > userInputs;

	void resetInputHistoryBrowse() {
		// whatever needs to happen to reset
		historyOffset = 0; // this is probably it...
	}

	void cursorUp () {
		if ( userInputs.empty() ) return; // no history, nothing to navigate
		if ( historyOffset == 0 ) { // you are at the original prompt
			clicache = currentLine; // so you need to cache before proceeding
		}
		// offset, clamp, and update currentLine
		historyOffset++;
		historyOffset = std::clamp( historyOffset, 0, int( userInputs.size() ) );
		currentLine = userInputs[ userInputs.size() - historyOffset ];
	}

	void cursorDown () {
		if ( userInputs.empty() ) return; // no history, no op
		if ( historyOffset == 0 ) return; // at the prompt, this is not meaningful
		if ( historyOffset == 1 ) {	// if we are one above the prompt
			// restoring cached line
			currentLine = clicache;
			historyOffset = 0;
		} else {
			// offset, clamp, and update currentLine
			historyOffset--;
			historyOffset = std::clamp( historyOffset, 0, int( userInputs.size() ) );
			currentLine = userInputs[ userInputs.size() - historyOffset ];
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

	// scrolling the output up and down, to see further back in time
	int scrollOffset = 0;
	void pageup ( bool shift ) { scrollOffset += shift ? 10 : 1; }
	void pagedown ( bool shift ) { scrollOffset -= shift ? 10 : 1; if ( scrollOffset < 0 ) scrollOffset = 0; }

	// more navigation
	void home () { cursorX = 0; }
	void end () { cursorX = currentLine.length(); }

	// further manipulation of current input line
	void backspace ( bool control ) {
		resetInputHistoryBrowse();
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
		resetInputHistoryBrowse();
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

	void tab () {

		// couple cases need to be handled:

		// done:
			// empty prompt, gives all strings
			// partial word, give trie completions

		// todo:
			// input prompt ends in " " -> show cvar list
			// input prompt's last token begins with "$" -> show cvar list matching
			// ...

		// get the last token from the current input + report all strings matching the current token
		int count = 0;
		std::stringstream ss;
		string lastToken = getLastToken( currentLine );

		bool didTrimWhitespace = ( lastToken[ lastToken.length() - 1 ] != currentLine[ currentLine.length() - 1 ] );
		bool lastTokenMarkedCvar = ( lastToken[ 0 ] == '$' );

		// we trimmed whitespace... or the lastToken is marked with the leading "$", in either case list cvars
		if ( didTrimWhitespace || lastTokenMarkedCvar ) {

			// holding the list of cvars
			std::vector< string > output;

			// list matching cvars, assuming we have at least one character after the "$"
			if ( lastTokenMarkedCvar && ( lastToken.length() > 1 ) ) {
				// match the partial string
				output = trieCvar->predictCompletions( lastToken.substr( 1 ), 16 );
			}

			// we didn't get any best matches... and we did trim whitespace, which means we are operating from no data
			// or, we have just a "$" at the prompt right now...
			// both cases, we just want to output all the cvars ( as many as are available )
			if ( ( output.empty() && didTrimWhitespace ) || lastToken == string( "$" ) ) {
				for ( uint i = 0; i < cvars.count(); i++ ) {
					output.push_back( cvars[ i ].label );
				}
			}

			// output the list of all the cvars, with leading dollar signs
			for ( uint i = 0; i < output.size(); i++ ) {
				count++;
				if ( count < 10 ) { // we only want to show a maximum of 10
					csb.append( string( "$" ), 2 ).append( output[ i ] + " ", 4 );
				} else if ( count < 11 ) { // append an elipsis if we have a list that would be continuing past 10
					csb.append( "...", 4 );
				}
			}

			// if we added any to the list, go ahead and show them
			if ( output.size() > 0 ) {
				addHistoryLine( csb.flush() );
			}

		} else {
			// we want to consider all possible matching completions, not just matching cvars
			std::vector< string > output = trie->predictCompletions( lastToken, 16 );
			for ( auto& s : output ) {
				count++;
				ss << s << " ";
				if ( count > 10 ) {
					break;
				}
			}

			// handles empty prompt
			if ( currentLine.size() == 0 ) {
				for ( auto& s : allStrings ) {
					count++;
					ss << s << " ";
					if ( count > 10 ) {
						break;
					}
				}
			}

			// if we had more than 10 strings match, indicate this
			if ( count > 10 ) {
				ss << "... ";
			}

			addHistoryLine( csb.append( ss.str(), 2 ).flush() );
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

		// reset input history browsing state, as well
		resetInputHistoryBrowse();

		// nonzero length
		if ( userInputString.length() ) {

			// add this to the input history
			userInputs.push_back( userInputString );

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