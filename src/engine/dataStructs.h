struct configData {
	uint32_t windowFlags = 0;
	string windowTitle = string( "NQADE" );
	int32_t width = 0;
	int32_t height = 0;
	bool linearFilter = false;
	ivec2 windowOffset = ivec2( 0, 0 );
	uint8_t startOnScreen = 0;

	uint8_t MSAACount = 0;
	vec4 clearColor = vec4( 0.0f );
	bool vSyncEnable = true;
	int numMsDelayAfterCallback = 10;
	uint8_t OpenGLVersionMajor = 4;
	uint8_t OpenGLVersionMinor = 3;
	bool OpenGLVerboseInit = false;
	bool SDLDisplayModeDump = false;

	bool reportPlatformInfo = true;
	bool loadDataResources = true;

	bool enableDepthTesting = false;
	bool SRGBFramebuffer = false;

	bool oneShot = false;

	// anything else ... ?
};

struct colorGradeParameters {
	bool showTonemapWindow = false;
	int tonemapMode = 6; // todo: write an enum for this
	float gamma = 1.1f;
	float postExposure = 1.0f;
	float saturation = 1.0f;
	bool saturationImprovedWeights = true;
	float colorTemp = 6500.0f;
};