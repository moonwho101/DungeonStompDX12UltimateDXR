#pragma once

#include <string>

namespace DungeonGen {

struct GeneratorOptions {
	int startX = 5200;
	int startZ = 2600;
	unsigned int seed = 0; // 0 for random seed
	int numObjectsToPlace = 350;

	// Validation / fail-safes. A generated dungeon is rejected and regenerated if
	// fewer than half of numObjectsToPlace pieces were placed, or if the exit
	// spiral ended up closer than minExitDistance units to the start position.
	float minExitDistance = 1000.0f;
	int maxAttempts = 32;
};

struct GenerationReport {
	int attempts = 0;          // number of generation attempts performed
	int piecesPlaced = 0;      // pieces placed (excluding the start piece) in the accepted dungeon
	float exitDistance = 0.0f; // straight-line distance from startpos to the exit spiral
	bool valid = false;        // false if every attempt failed validation and the best one was kept
};

struct GeneratorSettings {
	int seed = 0;
	int tileCount = 350;
	bool enhanced = true;
};

GeneratorSettings &GetGeneratorSettings();
bool GenerateDungeonWithSettings(const std::string &outputPath, GenerationReport *report = nullptr);

// Generates classic dungeon (generate_dungeon.py logic)
bool GenerateDungeonClassic(const std::string &outputPath, const GeneratorOptions &options = GeneratorOptions(), GenerationReport *report = nullptr);

// Generates new objects dungeon (generate_dungeonNewObjects.py logic)
bool GenerateDungeonNewObjects(const std::string &outputPath, const GeneratorOptions &options = GeneratorOptions(), GenerationReport *report = nullptr);

} // namespace DungeonGen
