/* usage is like this:
	float ms; // updated in the destructor of the scopedTimer
	{
		scopedTimer( &ms );
		// do some shit that takes some time
	}
*/

class scopedTimer {
public:
	GLuint64 startTime, stopTime;
	GLuint queryID[ 2 ];
	float * result;

	scopedTimer ( float * r ) : result ( r ) {
		glGenQueries( 2, &queryID[ 0 ] );
		glQueryCounter( queryID[ 0 ], GL_TIMESTAMP );
	}

	~scopedTimer () {
		glQueryCounter( queryID[ 1 ], GL_TIMESTAMP );
		GLint timeAvailable = 0;
		while ( !timeAvailable ) { // busy wait loop - we can do better
			glGetQueryObjectiv( queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
		}
		glGetQueryObjectui64v( queryID[ 0 ], GL_QUERY_RESULT, &startTime );
		glGetQueryObjectui64v( queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
		*result = ( stopTime - startTime ) / 1000000.0f; // operation time in ms
	}
};