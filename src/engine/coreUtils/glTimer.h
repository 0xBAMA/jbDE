#ifndef GLTIMER
#define GLTIMER

#include <vector>
#include <string>

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

#endif // GLTIMER