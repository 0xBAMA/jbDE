void IcarusImguiWindow ( icarusState_t &state ) {
	ImGui::Begin( "Icarus", NULL );

	ImGui::SliderInt( "Max Bounces", ( int * ) &state.maxBounces, 0, 256 );
	ImGui::Text( "Intersectors:" );
	ImGui::Checkbox( "SDF", &state.runSDF );
	ImGui::Checkbox( "Triangle", &state.runTriangle );
	ImGui::Checkbox( "Volume", &state.runVolume );

	// DoF controls

	ImGui::End();
}