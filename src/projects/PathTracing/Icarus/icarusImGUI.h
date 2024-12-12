void IcarusImguiWindow ( icarusState_t &state ) {
	ImGui::Begin( "Icarus", NULL );

	ImGui::SeparatorText( "Render" );
	ImGui::SliderInt( "Max Bounces", ( int * ) &state.maxBounces, 0, 256 );
	ImGui::Text( "Intersectors:" );
	ImGui::Checkbox( "SDF", &state.runSDF );
	ImGui::Checkbox( "Triangle", &state.runTriangle );
	ImGui::Checkbox( "BVH", &state.runBVH );
	ImGui::Checkbox( "Volume", &state.runVolume );

	const char * resolveModes[] = { "UNIFORM", "GAUSSIAN", "SHUFFLED" };
	ImGui::Combo( "Offset Feed Mode", &state.offsetFeedMode, resolveModes, IM_ARRAYSIZE( resolveModes ) );

	ImGui::SeparatorText( "Camera" );
	ImGui::SliderFloat( "FoV", &state.FoV, 0.0f, 3.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
	ImGui::SliderFloat( "UV Scalar", &state.uvScaleFactor, 0.0f, 5.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

	const char * cameraModeNames[] = { "DEFAULT", "ORTHO", "VORALDO", "SPHERICAL", "HDRI", "COMPOUND" };
	ImGui::Combo( "Camera Mode", &state.cameraMode, cameraModeNames, IM_ARRAYSIZE( cameraModeNames ) );

	const char * bokehModeNames[] = { "NONE", "EDGE BIASED DISK", "UNIFORM DISK", "REJECTION SAMPLED HEXAGON", "UNIFORM SAMPLED HEXAGON", "UNIFORM HEART", "THREE BLADE ROSETTE", "FIVE BLADE ROSETTE", "RING", "PENTAGON", "SEPTAGON", "OCTAGON", "NONAGON", "DECAGON", "11-GON", "5 SIDED STAR", "6 SIDED STAR", "7 SIDED STAR" };
	ImGui::Combo( "Bokeh Mode", &state.DoFBokehMode, bokehModeNames, IM_ARRAYSIZE( bokehModeNames ) );

	ImGui::SliderFloat( "DoF Radius", &state.DoFRadius, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
	ImGui::SliderFloat( "DoF Distance", &state.DoFFocusDistance, 0.0f, 30.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

	ImGui::SliderFloat( "Chromab Scale", &state.chromabScaleFactor, 0.0f, 0.1f, "%.6f", ImGuiSliderFlags_Logarithmic );

	ImGui::SeparatorText( "Resolution" );
	static int x = state.dimensions.x, y = state.dimensions.y;
	ImGui::SliderInt( "X", &x, 0, 8000 );
	ImGui::SliderInt( "Y", &y, 0, 8000 );
	if ( ImGui::Button( "Set" ) ) {
		state.forceUpdate = true;
		state.dimensions.x = x;
		state.dimensions.y = y;
		AllocateTextures( state );
	}

	ImGui::End();
}