#version 430

// there are ( 6 faces * 2 tris * 3 verts ) = 36 verts per primitive ( it is what it is )
// so ideally, this looks something like:
//	bounds = constantAABBBoundsArray[ glVertexID % 36 ]

// adjust this based on the size of the bounds of the given primitive... tbd

// and write the value for primitive ID:
//	id = glVertexID / 36 ( with integer roundoff )


// inputs are just the vertex id
// position, etc buffer, for sizing of bounds

// outputs are like, 

void main () {

}