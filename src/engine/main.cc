#include "engine.h"

int main ( int argc, char *argv[] ) {
	engineChild engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
