#include "daedalus.h"

void Daedalus::ShowDaedalusConfigWindow() {
	ZoneScoped;
	ImGui::Begin( "Daedalus", NULL, ImGuiWindowFlags_NoScrollWithMouse );

	ImGui::Text( "Daedalus" );
	ImGui::SeparatorText( " Performance " );
	float ts = daedalusConfig.tiles.SecondsSinceLastReset();
	ImGui::Text( "Fullscreen Passes: %d in %.3f seconds ( %.3f samples/sec )", daedalusConfig.tiles.SampleCount(), ts, ( float ) daedalusConfig.tiles.SampleCount() / ts );
	ImGui::SeparatorText( " History " );
	// timing history
	const std::vector< float > timeVector = { daedalusConfig.timeHistory.begin(), daedalusConfig.timeHistory.end() };
	const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
	ImGui::PlotLines( " ", timeVector.data(), daedalusConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 180.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

	if ( ImGui::CollapsingHeader( "Viewer Config" ) ) {
		ImGui::Text( "  Output: " );
		ImGui::Text( "  Zoom: %f, Offset: [%f, %f]", daedalusConfig.view.outputZoom, daedalusConfig.view.outputOffset.x, daedalusConfig.view.outputOffset.y );
		ImGui::Indent();
		if ( ImGui::SmallButton( "Reset View Offset" ) ) daedalusConfig.view.outputOffset = ivec2( 0 );
		if ( ImGui::SmallButton( "Reset Zoom" ) ) daedalusConfig.view.outputZoom = 1.0f;
		ImGui::Unindent();
		ImGui::Text( "  Displaying at: %dx%d", daedalusConfig.outputWidth, daedalusConfig.outputHeight );
		ImGui::Checkbox( "Edge Lines", &daedalusConfig.view.edgeLines );
		ImGui::Checkbox( "Center Lines", &daedalusConfig.view.centerLines );
		ImGui::Checkbox( "Rule of Thirds Lines", &daedalusConfig.view.ROTLines );
		ImGui::Checkbox( "Golden Ratio Lines", &daedalusConfig.view.goldenLines );
		ImGui::Checkbox( "Vignette", &daedalusConfig.view.vignette );
	}

	if ( ImGui::CollapsingHeader( "Rendering Settings" ) ) {

		ImGui::SliderFloat( "Viewer X", &daedalusConfig.render.viewerPosition.x, -40.0f, 40.0f );
		ImGui::SliderFloat( "Viewer Y", &daedalusConfig.render.viewerPosition.y, -40.0f, 40.0f );
		ImGui::SliderFloat( "Viewer Z", &daedalusConfig.render.viewerPosition.z, -40.0f, 40.0f );
		ImGui::Text( "Basis Vectors:" );
		ImGui::Text( " X: %.3f %.3f %.3f", daedalusConfig.render.basisX.x, daedalusConfig.render.basisX.y, daedalusConfig.render.basisX.z );
		ImGui::Text( " Y: %.3f %.3f %.3f", daedalusConfig.render.basisY.x, daedalusConfig.render.basisY.y, daedalusConfig.render.basisY.z );
		ImGui::Text( " Z: %.3f %.3f %.3f", daedalusConfig.render.basisZ.x, daedalusConfig.render.basisZ.y, daedalusConfig.render.basisZ.z );

		ImGui::Separator();

		ImGui::Text( "Subpixel Jitter: " );
		ImGui::RadioButton( "None", &daedalusConfig.render.subpixelJitterMethod, 0 );
		ImGui::SameLine();
		ImGui::RadioButton( "Blue Noise", &daedalusConfig.render.subpixelJitterMethod, 1 );
		ImGui::SameLine();
		ImGui::RadioButton( "Uniform RNG", &daedalusConfig.render.subpixelJitterMethod, 2 );
		ImGui::SameLine();
		ImGui::RadioButton( "Weyl", &daedalusConfig.render.subpixelJitterMethod, 3 );
		ImGui::SameLine();
		ImGui::RadioButton( "Weyl (Integer)", &daedalusConfig.render.subpixelJitterMethod, 4 );

		ImGui::Separator();

		ImGui::Text( "Camera Mode: " );
		ImGui::RadioButton( "Normal", &daedalusConfig.render.cameraType, 0 );
		ImGui::SameLine();
		ImGui::RadioButton( "Spherical", &daedalusConfig.render.cameraType, 1 );
		ImGui::SameLine();
		ImGui::RadioButton( "Spherical2", &daedalusConfig.render.cameraType, 2 );
		ImGui::SameLine();
		ImGui::RadioButton( "SphereBug", &daedalusConfig.render.cameraType, 3 );
		ImGui::SameLine();
		ImGui::RadioButton( "FakeOrtho", &daedalusConfig.render.cameraType, 4 );
		ImGui::SameLine();
		ImGui::RadioButton( "Ortho", &daedalusConfig.render.cameraType, 5 );
		ImGui::SameLine();
		ImGui::RadioButton( "Compound", &daedalusConfig.render.cameraType, 6 );

		ImGui::Separator();

		ImGui::Text( "Resolution" );
		static int x = daedalusConfig.targetWidth;
		static int y = daedalusConfig.targetHeight;
		ImGui::SameLine();
		if ( ImGui::SmallButton( " Resize " ) ) {
			ResizeAccumulators( x, y );
		}
		ImGui::SliderInt( "Accumulator X", &x, 0, 5000 );
		ImGui::SliderInt( "Accumulator Y", &y, 0, 5000 );
		ImGui::Text( "Presets:" );
		ImGui::SameLine(); if ( ImGui::SmallButton( " Preview " ) )	ResizeAccumulators( x = 160, y = 90 );
		ImGui::SameLine(); if ( ImGui::SmallButton( " 360p " ) )		ResizeAccumulators( x = 640, y = 360 );
		ImGui::SameLine(); if ( ImGui::SmallButton( " 720p " ) )		ResizeAccumulators( x = 1280, y = 720 );
		ImGui::SameLine(); if ( ImGui::SmallButton( " 1080p " ) )	ResizeAccumulators( x = 1920, y = 1080 );
		ImGui::SameLine(); if ( ImGui::SmallButton( " 4K " ) )		ResizeAccumulators( x = 3840, y = 2160 );
		ImGui::Separator();
		ImGui::Text( "Tile Config:" );
		ImGui::Text( " Split the %dx%d image into %d tiles", daedalusConfig.targetWidth, daedalusConfig.targetHeight, daedalusConfig.tiles.Count() );
		ImGui::Text( " Tile Size:" );
		ImGui::SameLine();
		if ( ImGui::SmallButton( " - " ) ) {
			daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize / 2, daedalusConfig.targetWidth, daedalusConfig.targetHeight );
		}
		ImGui::SameLine();
		ImGui::Text( "%d", daedalusConfig.tiles.tileSize );
		ImGui::SameLine();
		if ( ImGui::SmallButton( " + " ) ) {
			daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize * 2, daedalusConfig.targetWidth, daedalusConfig.targetHeight );
		}
		ImGui::SliderInt( "Tiles Between Queries", ( int * )&daedalusConfig.tiles.tilesBetweenQueries, 1, 15 );
		ImGui::SliderFloat( "Frame Time Limit (ms)", &daedalusConfig.tiles.tileTimeLimitMS, 16.0f, 100.0f );

		ImGui::Separator();

	}

	if ( ImGui::CollapsingHeader( "Images" ) ) {
		float availableWidth = ImGui::GetContentRegionAvail().x - 20;
		float proportionalHeight = availableWidth * ( float ) config.height / ( float ) config.width;

		ImGui::SeparatorText( "Display Texture" );
		ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Display Texture" ), ImVec2( availableWidth, proportionalHeight ) );

		proportionalHeight = availableWidth * ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;

		ImGui::SeparatorText( "Color Accumulator" );
		ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Color Accumulator" ), ImVec2( availableWidth, proportionalHeight ) );

		ImGui::SeparatorText( "Color Accumulator Scratch" );
		ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Color Accumulator Scratch" ), ImVec2( availableWidth, proportionalHeight ) );

		ImGui::SeparatorText( "Depth/Normals Accumulator" );
		ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Depth/Normals Accumulator" ), ImVec2( availableWidth, proportionalHeight ) );

		ImGui::SeparatorText( "Tonemapped" );
		ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Tonemapped" ), ImVec2( availableWidth, proportionalHeight ) );
	}

	if ( ImGui::CollapsingHeader( "Postprocess" ) ) {
		ImGui::Indent();
		PostProcessImguiMenu();
		ImGui::Unindent();
	}

	ImGui::End();
}