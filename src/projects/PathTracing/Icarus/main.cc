#include "../../../engine/engine.h"

#include "icarus.h"

int main ( int argc, char *argv[] ) {
	Icarus engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
