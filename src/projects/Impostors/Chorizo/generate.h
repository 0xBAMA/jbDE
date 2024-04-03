#include "../../../engine/includes.h"

void recursiveTreeBuild( std::vector< mat4 > &transforms, mat4 currentTransform, int levelsDeep, int maxLevelsDeep ) {
	vec3 x = ( currentTransform * vec4( 1.0f, 0.0f, 0.0f, 0.0f ) ).xyz();
	vec3 y = ( currentTransform * vec4( 0.0f, 1.0f, 0.0f, 0.0f ) ).xyz();
	vec3 z = ( currentTransform * vec4( 0.0f, 0.0f, 1.0f, 0.0f ) ).xyz();

	if ( levelsDeep == maxLevelsDeep ) {
		return;
	} else { // recurse
		transforms.push_back( currentTransform );
		transforms.push_back( glm::inverse( currentTransform ) );

		currentTransform = glm::rotate( 0.3f, x ) * currentTransform;
		recursiveTreeBuild( transforms, glm::scale( vec3( 0.9f ) ) * glm::translate( y * 3.0f ) * currentTransform, levelsDeep + 1, maxLevelsDeep );
		currentTransform = glm::rotate( float( 2.0f * pi / 3.0f ), y ) * currentTransform;
		recursiveTreeBuild( transforms, glm::scale( vec3( 0.9f ) ) * glm::translate( y * 3.0f ) * currentTransform, levelsDeep + 1, maxLevelsDeep );
		currentTransform = glm::rotate( float( 2.0f * pi / 3.0f ), y ) * currentTransform;
		recursiveTreeBuild( transforms, glm::scale( vec3( 0.9f ) ) * glm::translate( y * 3.0f ) * currentTransform, levelsDeep + 1, maxLevelsDeep );

		return;
	}
}

void randomPlacement( std::vector< mat4 > &transforms, int count ) {
	transforms.reserve( count );
	rng rotation( 0.0f, 10.0f );
	rng scale( 0.0001f, 0.01f );
	rng position( -5.5f, 5.5f );
	float t = position();
	rng parameters( 0.0001f, 0.01f );
	vec4 params = vec4( parameters(), parameters(), parameters(), parameters() );
	for ( int i = 0; i < count; i++, t += 0.05f ) {
		mat4 temp;
		// if ( i % 2 == 0 ) {
			// temp = glm::translate( vec3( cos( t * params.x ), sin( t * ( params.y / 3.0f ) ), cos( t * ( params.z / 5.0f ) ) * cos( t * ( params.x / 7.0f ) ) ) ) *
			// 	glm::rotate( t / 4.0f, glm::normalize( vec3( 1.0f, 1.0f, 0.0f ) ) ) *
			// 	glm::scale( vec3( 0.03f ) ) *
			// 	mat4( 1.0f );
		// } else {
			temp = glm::translate( vec3( position(), position(), position() ) ) *
				glm::rotate( rotation(), glm::normalize( vec3( position(), position(), position() ) ) ) *
				glm::scale( vec3( scale() ) ) *
				mat4( 1.0f );
		// }
		transforms.push_back( temp );
		transforms.push_back( glm::inverse( temp ) );
	}
}