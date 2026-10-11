#ifndef __DUNGEONEDITOR_HPP__
#define __DUNGEONEDITOR_HPP__

#include <windows.h>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "world.hpp"

using namespace DirectX;

struct SpawnableItem {
	std::string displayName;
	std::string objectName; // Name stored in oblist (e.g., "CORRIDOR01", "!monster1", "lamp_post")
	std::string modelName;  // For !monster1: model identifier (e.g. "GOBLIN", "POTION", "bed")
	std::string texName;    // For !monster1: texture alias or "-1" / "0"
	int category;           // 0: Geometry, 1: Monsters, 2: Props, 3: Items, 4: Lights/Triggers
	int ability;            // Default ability/key
	int lightType;          // 0: None, 1: Spotlight, 2: Directional, 3: Point, 900: Flicker
};

class DungeonEditor {
  public:
	DungeonEditor();
	~DungeonEditor();

	void Init();
	void ToggleActive();
	bool IsActive() const {
		return mActive;
	}
	void SetActive(bool active);

	void Update(float dt, const XMFLOAT3 &camPos, float camYaw, float camPitch, bool mouseClicked);
	void RenderImGui();
	void Render3DHighlights();

	void SpawnSelected();
	void DeleteSelected();
	void DuplicateSelected();
	void ClearLevel();
	void SelectObject(int index);

	int GetSelectedIndex() const {
		return mSelectedIndex;
	}
	XMFLOAT3 GetPlacementPos() const {
		return mPlacementPos;
	}

  private:
	void BuildCatalog();
	int PerformRaycast(const XMFLOAT3 &rayOrigin, const XMFLOAT3 &rayDir, float maxDist = 3000.0f);
	void SpawnItem(const SpawnableItem &item, const XMFLOAT3 &pos, float rotation);

  private:
	bool mActive = false;
	int mSelectedIndex = -1;
	int mHoveredIndex = -1;

	// Placement settings
	int mSelectedCategory = 0;
	int mSelectedItemInCat = 0;
	bool mGridSnap = true;
	float mGridSize = 100.0f;
	float mPlacementRot = 0.0f;
	float mPlacementYOffset = 0.0f;
	XMFLOAT3 mPlacementPos = { 0.0f, 0.0f, 0.0f };

	// Fly camera state
	bool mFreeFlyCam = false;

	// UI state
	char mMapNameInput[128] = "custom1";
	char mSearchFilter[128] = "";

	std::vector<SpawnableItem> mCatalog;
};

extern DungeonEditor gDungeonEditor;

#endif // __DUNGEONEDITOR_HPP__
