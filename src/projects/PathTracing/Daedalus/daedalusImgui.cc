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

	// placeholder state dump
	ImGui::SeparatorText( "Daedalus State:" );
	ImGui::Text( "  Accumulator Size: %d x %d", daedalusConfig.targetWidth, daedalusConfig.targetHeight );
	ImGui::Text( "  Tile Dispenser: %d tiles", daedalusConfig.tiles.Count() );
	ImGui::SameLine();
	if ( ImGui::Button( " - " ) ) {
		daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize / 2, daedalusConfig.targetWidth, daedalusConfig.targetHeight );
	}
	ImGui::SameLine();
	ImGui::Text( "%d", daedalusConfig.tiles.tileSize );
	ImGui::SameLine();
	if ( ImGui::Button( " + " ) ) {
		daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize * 2, daedalusConfig.targetWidth, daedalusConfig.targetHeight );
	}
	ImGui::Text( "  Output: %f zoom, [%f, %f] offset", daedalusConfig.view.outputZoom, daedalusConfig.view.outputOffset.x, daedalusConfig.view.outputOffset.y );
	ImGui::SameLine();
	if ( ImGui::Button( "Set Zero" ) ) {
		daedalusConfig.view.outputOffset = ivec2( 0 );
	}

	if ( ImGui::CollapsingHeader( "Viewer Config" ) ) {
		ImGui::Checkbox( "Edge Lines", &daedalusConfig.view.edgeLines );
		ImGui::Checkbox( "Center Lines", &daedalusConfig.view.centerLines );
		ImGui::Checkbox( "Rule of Thirds Lines", &daedalusConfig.view.ROTLines );
		ImGui::Checkbox( "Golden Ratio Lines", &daedalusConfig.view.goldenLines );
		ImGui::Checkbox( "Vignette", &daedalusConfig.view.vignette );
	}

	if ( ImGui::CollapsingHeader( "Images" ) ) {

		ImGui::Text( "Resolution" );
		static int x = daedalusConfig.targetWidth;
		static int y = daedalusConfig.targetHeight;
		ImGui::SliderInt( "Accumulator X", &x, 0, 5000 );
		ImGui::SliderInt( "Accumulator Y", &y, 0, 5000 );
		if ( ImGui::Button( " Resize " ) ) {
			ResizeAccumulators( x, y );
		}
		if ( ImGui::Button( " Preview " ) ) {
			x = 160;
			y = 90;
			ResizeAccumulators( x, y );
		}
		ImGui::SameLine();
		if ( ImGui::Button( " 360p " ) ) {
			x = 640;
			y = 360;
			ResizeAccumulators( x, y );
		}
		ImGui::SameLine();
		if ( ImGui::Button( " 720p " ) ) {
			x = 1280;
			y = 720;
			ResizeAccumulators( x, y );
		}
		ImGui::SameLine();
		if ( ImGui::Button( " 1080p " ) ) {
			x = 1920;
			y = 1080;
			ResizeAccumulators( x, y );
		}
		ImGui::SameLine();
		if ( ImGui::Button( " 4K " ) ) {
			x = 3840;
			y = 2160;
			ResizeAccumulators( x, y );
		}


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