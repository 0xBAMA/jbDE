#include "engine.h"

int main ( int argc, char *argv[] ) {
	// engine engineInstance;
	engineChild engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
