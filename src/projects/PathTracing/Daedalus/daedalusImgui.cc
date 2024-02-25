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
		ImGui::Text( "  Output: %f zoom, [%f, %f] offset", daedalusConfig.view.outputZoom, daedalusConfig.view.outputOffset.x, daedalusConfig.view.outputOffset.y );
		ImGui::SameLine();
		if ( ImGui::SmallButton( "Set Zero" ) ) {
			daedalusConfig.view.outputOffset = ivec2( 0 );
		}

		ImGui::Checkbox( "Edge Lines", &daedalusConfig.view.edgeLines );
		ImGui::Checkbox( "Center Lines", &daedalusConfig.view.centerLines );
		ImGui::Checkbox( "Rule of Thirds Lines", &daedalusConfig.view.ROTLines );
		ImGui::Checkbox( "Golden Ratio Lines", &daedalusConfig.view.goldenLines );
		ImGui::Checkbox( "Vignette", &daedalusConfig.view.vignette );
	}

	if ( ImGui::CollapsingHeader( "Rendering Settings" ) ) {
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
		ImGui::Text( "Tile Dispenser:" );
		ImGui::Text( " Split image into %d tiles", daedalusConfig.tiles.Count() );
		ImGui::Text( " Size:" );
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

		ImGui::Text( "Tiles Between Queries" ); ImGui::SameLine();
		ImGui::SliderInt( " ##tiles", ( int * )&daedalusConfig.tiles.tilesBetweenQueries, 1, 15 );
		
		ImGui::Text( "Frame Time Limit (ms)" ); ImGui::SameLine();
		ImGui::SliderFloat( " ##timeLimit", &daedalusConfig.tiles.tileTimeLimitMS, 16.0f, 100.0f );

		ImGui::Separator();

		ImGui::Text( "Subpixel Jitter: " );
		ImGui::RadioButton( "None", &daedalusConfig.subpixelJitterMethod, 0 );
		ImGui::SameLine();
		ImGui::RadioButton( "Blue Noise", &daedalusConfig.subpixelJitterMethod, 1 );
		ImGui::SameLine();
		ImGui::RadioButton( "Uniform RNG", &daedalusConfig.subpixelJitterMethod, 2 );
		ImGui::SameLine();
		ImGui::RadioButton( "Weyl", &daedalusConfig.subpixelJitterMethod, 3 );
		ImGui::SameLine();
		ImGui::RadioButton( "Weyl (Integer)", &daedalusConfig.subpixelJitterMethod, 4 );

		ImGui::Separator();
		ImGui::Text( "Camera Mode: " );
		ImGui::RadioButton( "Normal", &daedalusConfig.cameraMode, 0 );
		ImGui::SameLine();
		ImGui::RadioButton( "Spherical", &daedalusConfig.cameraMode, 1 );
		ImGui::SameLine();
		ImGui::RadioButton( "Spherical2", &daedalusConfig.cameraMode, 2 );
		ImGui::SameLine();
		ImGui::RadioButton( "SphereBug", &daedalusConfig.cameraMode, 3 );
		ImGui::SameLine();
		ImGui::RadioButton( "FakeOrtho", &daedalusConfig.cameraMode, 4 );
		ImGui::SameLine();
		ImGui::RadioButton( "Ortho", &daedalusConfig.cameraMode, 5 );

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