/*
usage is like this:

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

	scopedTimer ( float * r ) {
		glGenQueries( 1, &queryID[ 0 ] );
		glQueryCounter( queryID[ 0 ], GL_TIMESTAMP );
		result = r;
	}

	~scopedTimer () {
		glGenQueries( 1, &queryID[ 1 ] );
		glQueryCounter( queryID[ 1 ], GL_TIMESTAMP );
		GLint timeAvailable = 0;
		while ( !timeAvailable ) {
			glGetQueryObjectiv( queryID[ 1 ], GL_QUERY_RESULT_AVAILABLE, &timeAvailable );
		}
		glGetQueryObjectui64v( queryID[ 0 ], GL_QUERY_RESULT, &startTime );
		glGetQueryObjectui64v( queryID[ 1 ], GL_QUERY_RESULT, &stopTime );
		*result = ( stopTime - startTime ) / 1000000.0f; // operation time in ms
	}
};