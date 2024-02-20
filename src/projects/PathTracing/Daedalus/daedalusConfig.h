struct daedalusConfig_t {

	// tabbed interface for configuring scene/rendering parameters
	bool showConfigWindow = true;

	// sizing for accumulators, etc
	uint32_t targetWidth = 640;
	uint32_t targetHeight = 360;

	// grid zoom
	float outputZoom = 1.0f;
	vec2 outputOffset = vec2( 0.0f );

};