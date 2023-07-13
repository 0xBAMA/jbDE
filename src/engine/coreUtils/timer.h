#ifndef TIMER
#define TIMER

#include <chrono>

//=============================================================================
//==== OpenGL Timer Query Wrapper =============================================
//=============================================================================

struct queryPair_GPU {
	queryPair_GPU ( string s ) : label( s ) {}
	string label;
	GLuint queryID[ 2 ];
	float result = 0.0f;
};

struct queryPair_CPU {
	queryPair_CPU ( string s ) : label( s ) {}
	string label;
	std::chrono::time_point< std::chrono::system_clock > tStart;
	std::chrono::time_point< std::chrono::system_clock > tStop;
	float result;
};

class timerManager {
public:
	std::vector < queryPair_GPU > queries_GPU;
	std::vector < queryPair_CPU > queries_CPU;
	void gather () {
		for ( auto& q : queries_GPU ) {
			GLint timeAvailable = 0;
			while ( !timeAvailable ) { // wait on the most recent of the queries to become available
				glGetQueryObjectiv( q.queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
			}

			GLuint64 startTime, stopTime; // get the query results, since they're both ready
			glGetQueryObjectui64v( q.queryID[ 0 ], GL_QUERY_RESULT, &startTime );
			glGetQueryObjectui64v( q.queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
			glDeleteQueries( 2, &q.queryID[ 0 ] ); // and then delete them

			// get final operation time in ms, from difference of nanosecond timestamps
			q.result = ( stopTime - startTime ) / 1000000.0f;
		}
	}

	void clear () { // prepare for next frame
		queries_GPU.clear();
		queries_CPU.clear();
	}
};

inline timerManager* timerQueries;

class scopedTimer {
public:
	queryPair_CPU c;
	queryPair_GPU q;
	scopedTimer ( string label ) : c ( label ), q ( label ) {
		// GPU query prep
		glGenQueries( 2, &q.queryID[ 0 ] );
		glQueryCounter( q.queryID[ 0 ], GL_TIMESTAMP );

		// CPU query prep
		c.tStart = std::chrono::system_clock::now();
	}
	~scopedTimer () {
		// GPU query finish
		glQueryCounter( q.queryID[ 1 ], GL_TIMESTAMP );
		timerQueries->queries_GPU.push_back( q );

		// CPU query finish
		c.tStop = std::chrono::system_clock::now();
		c.result = std::chrono::duration_cast<std::chrono::microseconds>( c.tStop - c.tStart ).count() / 1000.0f;
		timerQueries->queries_CPU.push_back( c );
	}
};

// Timing for the initialization code, similar to scoped timers but outputs to CLI
class Block {
const int reportWidth = 40;
public:
	std::chrono::time_point<std::chrono::steady_clock> tStart;
	Block( string sectionName ) {
		tStart = std::chrono::steady_clock::now();
		cout << T_BLUE << "    " << sectionName << " " << RESET;
		for ( unsigned int i = 0; i < reportWidth - sectionName.size(); i++ ) {
			cout << ".";
		}
		cout << " ";
	}

	~Block() {
		const float timeInMS = std::chrono::duration_cast< std::chrono::microseconds >( std::chrono::steady_clock::now() - tStart ).count() / 1000.0f;
		const float wholeMS = int( std::floor( timeInMS ) );
		const float partialMS = int( ( timeInMS - wholeMS ) * 1000.0f );
		cout << T_GREEN << "done." << T_RED << " ( " << std::setfill( ' ' ) << std::setw( 6 ) << wholeMS << "."
			<< std::setw( 3 ) << std::setfill( '0' ) << partialMS << " ms )" << RESET << newline;
	}
};

#endif
