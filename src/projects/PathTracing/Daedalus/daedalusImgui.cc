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
	ImGui::Text( "  Output: %f zoom, [%f, %f] offset", daedalusConfig.outputZoom, daedalusConfig.outputOffset.x, daedalusConfig.outputOffset.y );
	ImGui::SameLine();
	if ( ImGui::Button( "Set Zero" ) ) {
		daedalusConfig.outputOffset = ivec2( 0 );
	}

	if ( ImGui::CollapsingHeader( "Viewer Config" ) ) {
		ImGui::Checkbox( "Edge Lines", &daedalusConfig.edgeLines );
		ImGui::Checkbox( "Center Lines", &daedalusConfig.centerLines );
		ImGui::Checkbox( "Rule of Thirds Lines", &daedalusConfig.ROTLines );
		ImGui::Checkbox( "Golden Ratio Lines", &daedalusConfig.goldenLines );
		ImGui::Checkbox( "Vignette", &daedalusConfig.vignette );
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