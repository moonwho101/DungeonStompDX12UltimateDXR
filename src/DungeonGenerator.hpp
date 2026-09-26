#pragma once

#include <string>

namespace DungeonGen {

struct GeneratorOptions {
	int startX = 5200;
	int startZ = 2600;
	unsigned int seed = 0; // 0 for random seed
	int numObjectsToPlace = 350;
};

// Generates classic dungeon (generate_dungeon.py logic)
bool GenerateDungeonClassic(const std::string &outputPath, const GeneratorOptions &options = GeneratorOptions());

// Generates new objects dungeon (generate_dungeonNewObjects.py logic)
bool GenerateDungeonNewObjects(const std::string &outputPath, const GeneratorOptions &options = GeneratorOptions());

} // namespace DungeonGen
