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

struct queryPair_GPU {
	queryPair_GPU ( string s ) : label( s ) {}
	string label;
	GLuint queryID[ 2 ];
	float result = 0.0f;
};

struct queryPair_CPU {
	queryPair_CPU ( string s ) : label( s ) {}
	string label;
	std::chrono::time_point<std::chrono::system_clock> tStart;
	std::chrono::time_point<std::chrono::system_clock> tStop;
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

static timerManager timerQueries;
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
		timerQueries.queries_GPU.push_back( q );

		// CPU query finish
		c.tStop = std::chrono::system_clock::now();
		c.result = std::chrono::duration_cast<std::chrono::microseconds>( c.tStop - c.tStart ).count() / 1000.0f;
		timerQueries.queries_CPU.push_back( c );
	}
};

#endif
