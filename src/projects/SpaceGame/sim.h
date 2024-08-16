#include "../../engine/includes.h"

rng gen = rng( 0.0f, 1.0f );

struct worker_t {
	float productionCapability;
	float productionChance;
	float lifetimeProduction = 0.0f;
	float getCurrentProduction () {
		if ( gen() > productionChance ) { // chance to produce some resource per update
			float amount = productionCapability * gen();
			lifetimeProduction += amount;
			return amount;
		}
		return 0.0f;
	}
};

const int numWorkers = 4;

struct spaceGameData_t {
	worker_t workers[ numWorkers ];
	float totalProduce = 0.0f;

	spaceGameData_t() {
		rng gen = rng( 0.0f, 0.1f );
		for ( int i = 0; i < numWorkers; i++ ) {
			workers[ i ].productionCapability = gen();
			workers[ i ].productionChance = gen();
		}
	}

	void update () {
		for ( int i = 0; i < numWorkers; i++ ) {
			totalProduce += workers[ i ].getCurrentProduction();
		}
	}

	void reset () {
		totalProduce = 0.0f;
		for ( int i = 0; i < numWorkers; i++ ) {
			workers[ i ].lifetimeProduction = 0.0f;
		}
	}
};