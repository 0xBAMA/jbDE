#include "../../../engine/engine.h"

// c file io
#include <stdio.h>
#include <stdlib.h>

struct MNISTImage {
	uint8_t label;
	uint8_t data[ 28 ][ 28 ];

	MNISTImage ( uint8_t inputLabel ) : label( inputLabel ) {}
};

vector< MNISTImage > trainImages;
vector< MNISTImage > testImages;

// referring to https://x.com/konradgajdus/article/1837196363735482396
	// adding also the test set from https://yann.lecun.com/exdb/mnist/index.html
struct MNIST_dataLoader {
	unsigned char *images_train, *labels_train;
	int nImages_train;

	unsigned char *images_test, *labels_test;
	int nImages_test;

	void ReadImages ( const char *filename, unsigned char **images, int *nImages ) {
		FILE *file = fopen( filename, "rb" );
		if ( !file ) exit( 1 );

		int temp, rows, cols;
		temp = fread( &temp, sizeof( int ), 1, file );
		temp = fread( nImages, sizeof( int ), 1, file );
		*nImages = __builtin_bswap32( *nImages );

		// known size, doesn't matter
		temp = fread( &rows, sizeof( int ), 1, file );
		temp = fread( &cols, sizeof( int ), 1, file );

		*images = ( unsigned char * ) malloc( ( *nImages ) * 28 * 28 );
		temp = fread( *images, sizeof(unsigned char), ( *nImages ) * 28 * 28, file );
		fclose( file );
	}
	void ReadLabels ( const char *filename, unsigned char **labels, int *nLabels ) {
		FILE *file = fopen( filename, "rb" );
		if ( !file ) exit( 1 );

		int temp;
		temp = fread( &temp, sizeof( int ), 1, file );
		temp = fread( nLabels, sizeof( int ), 1, file );
		*nLabels = __builtin_bswap32( *nLabels );

		*labels = ( unsigned char * ) malloc( *nLabels );
		temp = fread( *labels, sizeof( unsigned char ), *nLabels, file );
		fclose( file );
	}

	void Load () {
		// training set
		ReadImages( "../MNIST/train-images-idx3-ubyte", &images_train, &nImages_train );
		ReadLabels( "../MNIST/train-labels-idx1-ubyte", &labels_train, &nImages_train );

		// put it in the vector
		for ( int i = 0; i < nImages_train; i++ ) {
			MNISTImage temp( labels_train[ i ] );
			int baseIdx = i * 28 * 28;
			for ( int y = 0; y < 28; y++ ) {
				for ( int x = 0; x < 28; x++ ) {
					temp.data[ x ][ y ] = images_train[ baseIdx ];
					baseIdx++;
				}
			}
			trainImages.push_back( temp );
		}

		// free memory
		free( images_train );
		free( labels_train );

		// testing set
		ReadImages( "../MNIST/t10k-images-idx3-ubyte", &images_test, &nImages_test );
		ReadLabels( "../MNIST/t10k-labels-idx1-ubyte", &labels_test, &nImages_test );

		// put it in the vector
		for ( int i = 0; i < nImages_test; i++ ) {
			MNISTImage temp( labels_test[ i ] );
			int baseIdx = i * 28 * 28;
			for ( int y = 0; y < 28; y++ ) {
				for ( int x = 0; x < 28; x++ ) {
					temp.data[ x ][ y ] = images_test[ baseIdx ];
					baseIdx++;
				}
			}
			testImages.push_back( temp );
		}

		// free memory
		free( images_test );
		free( labels_test );
	}

	void ShowTrain ( uint i ) {
		cout << "Label: " << int( trainImages[ i ].label ) << endl;
		for ( int y = 0; y < 28; y++ ) {
			for ( int x = 0; x < 28; x++ ) {
				cout << ( trainImages[ i ].data[ x ][ y ] ? 1 : 0 );
			}
			cout << endl;
		}
		cout << endl;
	}

	void ShowTest ( uint i ) {
		cout << "Label: " << int( testImages[ i ].label ) << endl;
		for ( int y = 0; y < 28; y++ ) {
			for ( int x = 0; x < 28; x++ ) {
				cout << ( testImages[ i ].data[ x ][ y ] ? 1 : 0 );
			}
			cout << endl;
		}
		cout << endl;
	}

	void Show () {
		// enumerate images in the training set
		for ( uint i = 0; i < trainImages.size(); i++ ) {
			ShowTrain( i );
		}

		// enumerate images in the testing set
		for ( uint i = 0; i < testImages.size(); i++ ) {
			ShowTest( i );
		}
	}
};

struct NNLayer {
	vector< float > weights; // flattened weight array
	vector< float > weightMomentum; // + momentum

	vector< float > biases; // biases for each neuron
	vector< float > biasMomentum; // + momentum

	uint inputSize, outputSize;

	NNLayer () {}
	NNLayer ( uint in, uint out ) : inputSize( in ), outputSize( out ) {
		// fully connected, each output has [in] many weights
		int n = in * out;

		weights.resize( n, 0.0f ); // will be overwritten
		weightMomentum.resize( n, 0.0f );

		biases.resize( out, 0.0f ); // leave zero initialized
		biasMomentum.resize( out, 0.0f );

		// "He initialization", initial random weights correlated with input size
		float scale = std::sqrt( 2.0f / in ) * 2.0f;
		rng init = rng( -scale, scale );
		for ( int i = 0; i < n; i++ ) {
			weights[ i ] = init();
		}
	}
};

void SoftMax ( vector< float > &input ) {
	// first iteration - find the max across elements
	float max = input[ 0 ];
	for ( uint i = 1; i < input.size(); i++ ) {
		max = std::max( max, input[ i ] );
	}

	// second iteration - apply the transform, keep a sum
	float sum = 0.0f;
	for ( uint i = 0; i < input.size(); i++ ) {
		input[ i ] = expf( input[ i ] - max );
		sum += input[ i ];
	}

	// third iteration - scale by the sum
	for ( uint i = 0; i < input.size(); i++ ) {
		input[ i ] /= sum;
	}
}

struct NeuralNet {

	// hyperparameters
	const float learningRate = 0.0005f;
	const float momentum = 0.9f;
	const uint epochs = 20;

	vector< float > inputData;		// buffer for inputs
	vector< float > inputGradient;	// placeholder, for consistency
	NNLayer hiddenLayer;			// first fully connected layer
	vector< float > hiddenData;		// buffer for hidden layer outputs
	vector< float > hiddenGradient;	// gradient of values for the hidden layer
	NNLayer outputLayer;			// second fully connected layer
	vector< float > outputData;		// buffer for output neurons
	vector< float > outputGradient;	// gradient of values, with respect to known label

	NeuralNet () : hiddenLayer( 28 * 28, 256 ), outputLayer( 256, 10 ) {
	// allocate space for data buffers

		inputData.resize( 28 * 28, 0.0f );
		inputGradient.resize( 0 );

		hiddenData.resize( 256, 0.0f );
		hiddenGradient.resize( 256, 0.0f );

		outputData.resize( 10, 0.0f );
		outputGradient.resize( 10, 0.0f );
	}

	void Forward ( NNLayer layer, vector< float > &inputData, vector< float > &outputData ) {
		// initialize outputs with the bias values
		for ( uint i = 0; i < layer.outputSize; i++ ) {
			outputData[ i ] = layer.biases[ i ];
		}

		// add weighted inputs to these output totals
		for ( uint j = 0; j < layer.inputSize; j++ ) {
			float input = inputData[ j ];
			for ( uint i = 0; i < layer.outputSize; i++ ) {
				outputData[ i ] += input * layer.weights[ i + j * layer.outputSize ];
			}
		}

		// apply ReLU
		for ( uint i = 0; i < layer.outputSize; i++ ) {
			outputData[ i ] = ( outputData[ i ] > 0 ) ? outputData[ i ] : 0;
		}
	}

	void Backward ( NNLayer layer, vector< float > &inputData, vector< float > &outputGradient, vector< float > &inputGradient ) {
	// calculating gradients, proceeds backwards

		// updating gradients
		if ( inputGradient.size() != 0 ) { // makes no sense to calculate input gradient for input layer... but makes function signature consistent
			for ( uint j = 0; j < layer.inputSize; j++ ) {
				inputGradient[ j ] = 0.0f;
				for ( uint i = 0; i < layer.outputSize; i++ ) {
					inputGradient[ j ] += outputGradient[ i ] * layer.weights[ i + j * layer.outputSize ];
				}
			}
		}

		// updating with momentum heuristic
		for ( uint j = 0; j < layer.inputSize; j++ ) {
			float inputValue = inputData[ j ];
			for ( uint i = 0; i < layer.outputSize; i++ ) {
				float grad = outputGradient[ i ] * inputValue;
				// cout << "weight before: " << layer.weights[ i + j * layer.outputSize ];
				layer.weightMomentum[ i + j * layer.outputSize ] = momentum * layer.weightMomentum[ i + j * layer.outputSize ] + learningRate * grad;
				layer.weights[ i + j * layer.outputSize ] -= layer.weightMomentum[ i + j * layer.outputSize ];
				// cout << " weight after: " << layer.weights[ i + j * layer.outputSize ] << " with momentum: " << layer.weightMomentum[ i + j * layer.outputSize ] << endl;
				if ( inputGradient.size() != 0 ) {
					inputGradient[ j ] += outputGradient[ i ] * layer.weights[ i + j * layer.outputSize ];
				}
			}
		}

		// updating biases
		for ( uint i = 0; i < layer.outputSize; i++ ) {
			layer.biasMomentum[ i ] = momentum * layer.biasMomentum[ i ] + learningRate * outputGradient[ i ];
			layer.biases[ i ] -= layer.biasMomentum[ i ];
		}
	}

	float Train ( uint i ) {
		// load image i (from training set)
		MNISTImage currentImage = trainImages[ i ];

		// put it in the first layer
		uint index = 0;
		for ( uint y = 0; y < 28; y++ ) {
			for ( uint x = 0; x < 28; x++ ) {
				inputData[ index ] = currentImage.data[ x ][ y ] / 255.0f;
				index++;
			}
		}

		// forward propagation
		Forward( hiddenLayer, inputData, hiddenData );
		Forward( outputLayer, hiddenData, outputData );
		SoftMax( outputData );

		// calculate output gradients (compared to known label)
		for ( uint i = 0; i < outputLayer.outputSize; i++ ) {
			outputGradient[ i ] = outputData[ i ] - ( i == currentImage.label ? 1.0f : 0.0f );
		}

		// backwards propagation
		Backward( outputLayer, hiddenData, outputGradient, hiddenGradient );

		// apply ReLU on derivatives
		for ( uint i = 0; i < hiddenLayer.outputSize; i++ ) {
			hiddenGradient[ i ] *= ( hiddenData[ i ] > 0.0f ) ? 1.0f : 0.0f;
		}

		Backward( hiddenLayer, inputData, hiddenGradient, inputGradient );

		// calculating loss, from the output layer
		return -log( outputData[ currentImage.label ] + 1e-10f );
	}

	uint8_t Predict ( uint i ) {
		// load in image i (from testing set)
		MNISTImage currentImage = testImages[ i ];

		// put it in the first layer
		uint index = 0;
		for ( uint y = 0; y < 28; y++ ) {
			for ( uint x = 0; x < 28; x++ ) {
				inputData[ index ] = currentImage.data[ x ][ y ] / 255.0f;
				index++;
			}
		}

		// propagate forwards
		Forward( hiddenLayer, inputData, hiddenData );
		Forward( outputLayer, hiddenData, outputData );
		SoftMax( outputData );

		// indicate which output is most likely
		int maxOutput = 0;
		for ( uint i = 1; i < outputLayer.outputSize; i++ ) {
			if ( outputData[ i ] > outputData[ maxOutput ] ) {
				maxOutput = i;
			}
		}

		return maxOutput;
	}

};

class MNIST final : public engineBase {
public:
	MNIST () { Init(); OnInit(); PostInit(); }
	~MNIST () { Quit(); }

	NeuralNet network;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "./src/projects/SignalProcessing/MNIST/shaders/draw.cs.glsl" ).shaderHandle;

			// load the data
			MNIST_dataLoader data;
			data.Load();

			// training + testing
			for ( uint epoch = 0; epoch < network.epochs; epoch++ ) {
				unscopedTimer epochTime;
				epochTime.tick();

				// training iterations
				float totalLoss = 0.0f;
				for ( uint i = 0; i < trainImages.size(); i++ ) {
					totalLoss += network.Train( i );
				}

				// testing iterations
				int correct = 0;
				for ( uint i = 0; i < testImages.size(); i++ ) {
					data.ShowTest( i );

					uint prediction = network.Predict( i );
					cout << "predicted: " << prediction << endl << endl;
					if ( testImages[ i ].label == prediction ) {
						correct++;
					}
				}

				// report epoch results
				epochTime.tock();
				cout << "Epoch " << epoch + 1 << ", Accuracy: " << 100.0f * float( correct ) / float( testImages.size() ) << "% Avg. Loss: " << totalLoss / float( trainImages.size() ) << " Time: " << epochTime.timeCPU / 1000.0f << "s" << endl;
			}

			// quit when done
			config.oneShot = true;
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

	}

	void ImguiPass () {
		ZoneScoped;
		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit - need to add a hello triangle to this, so I have an easier starting point for raster stuff
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Clear();
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw any API geometry desired
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		{
			scopedTimer Start( "ImGUI Pass" );
			ImguiFrameStart();		// start the imgui frame
			ImguiPass();			// do all the gui stuff
			ImguiFrameEnd();		// finish imgui frame and put it in the framebuffer
		}
		window.Swap();				// show what has just been drawn to the back buffer
	}

	bool MainLoop () { // this is what's called from the loop in main
		ZoneScoped;

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

		// event handling
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	MNIST engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}
