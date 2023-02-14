#ifndef TIMER
#define TIMER

//=============================================================================
//==== std::chrono Wrapper - Simplified Tick() / Tock() Interface =============
//=============================================================================

#include <chrono>
// no nesting, but makes for a very simple interface
	// could probably do something stack based, have Tick() push and Tock() pop
#define NOW std::chrono::high_resolution_clock::now()

// option to report ms/us
#define USE_MS

// if not milliseconds, do microseconds
#ifndef USE_MS
#define USE_US
#endif

// TIMEUNIT for ease in the reporting of time quantities, matching units

#ifdef USE_MS
#define TIMECAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()/1000.0f
#define TIMEUNIT " ms"
#endif

#ifdef USE_US
#define TIMECAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()
#define TIMEUNIT " us"
#endif

static auto tInit = NOW;
static auto t = NOW;

// set base time
static inline void Tick () { t = NOW; }
// get difference between base time and current time, return value in useconds
static inline float Tock () { return TIMECAST( NOW - t ); }
// getting the time since the engine was started
static inline float TotalTime () { return TIMECAST( NOW - tInit ); }

//=============================================================================
//==== OpenGL Timer Query Wrapper =============================================
//=============================================================================

struct queryPair {
	queryPair ( string s ) : label( s ) {}
	string label;
	GLuint queryID[ 2 ];
	float result = 0.0f;
};

class timerManager {
public:
	std::vector < queryPair > queries;
	void gather () {
		for ( auto& q : queries ) {

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
		queries.clear();
	}
};

static timerManager timerQueries;

class scopedTimer {
public:
	queryPair q;
	scopedTimer ( string label ) : q ( label ) {
		glGenQueries( 2, &q.queryID[ 0 ] );
		glQueryCounter( q.queryID[ 0 ], GL_TIMESTAMP );
	}
	~scopedTimer () {
		glQueryCounter( q.queryID[ 1 ], GL_TIMESTAMP );
		timerQueries.queries.push_back( q );
	}
};

#endif
