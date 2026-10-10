#include "DungeonEditor.hpp"
#include "LoadWorld.hpp"
#include "world.hpp"
#include "GameLogic.hpp"
#include "GlobalSettings.hpp"
#include "imgui/imgui.h"
#include <stdio.h>
#include <algorithm>

DungeonEditor gDungeonEditor;

extern CLoadWorld *pCWorld;
extern int oblist_length;
extern OBJECTLIST *oblist;
extern OBJECTDATA *obdata;
extern int obdata_length;
extern PLAYER *monster_list;
extern int num_monsters;
extern PLAYER *item_list;
extern int itemlistcount;
extern PLAYER *player_list2;
extern int num_players2;
extern struct startposition startpos[200];
extern int startposcounter;
extern TEXTUREMAPPING TexMap[MAX_NUM_TEXTURES];
extern int number_of_tex_aliases;
extern MODELLIST *model_list;
extern int countmodellist;
extern char gActionMessage[2048];
extern int UpdateScrollList(int r, int g, int b);
extern int load_level(char *filename);

DungeonEditor::DungeonEditor() {
}

DungeonEditor::~DungeonEditor() {
}

void DungeonEditor::Init() {
	BuildCatalog();
}

void DungeonEditor::ToggleActive() {
	SetActive(!mActive);
}

void DungeonEditor::SetActive(bool active) {
	mActive = active;
	extern bool enableGui;
	enableGui = mActive;

	ImGuiIO &io = ImGui::GetIO();
	if (mActive) {
		io.ClearEventsQueue();
		io.ClearInputKeys();
		io.ClearInputMouse();
		io.MouseDrawCursor = true;
		ShowCursor(TRUE);
		sprintf_s(gActionMessage, "Gary Gygax's Dungeon Builder Active! [F1] to exit.");
		UpdateScrollList(255, 215, 0);
	} else {
		io.MouseDrawCursor = false;
		ShowCursor(FALSE);
		sprintf_s(gActionMessage, "Returned to Dungeon Stomp. [F1] for Editor.");
		UpdateScrollList(0, 255, 255);
	}
}

void DungeonEditor::BuildCatalog() {
	mCatalog.clear();

	// -------------------------------------------------------------
	// Category 0: Dungeon Geometry (from objects.dat)
	// -------------------------------------------------------------
	const char *geomPieces[] = {
		"CORRIDOR01", "CORRIDOR02", "CORRIDOR03",
		"CROSSING01", "CROSSING02", "CROSSING03",
		"ROOM02", "ROOM05", "ROOM06",
		"door55", "cdoorclosedchest",
		"block01", "block02", "!wall0-240-320",
		"lamp_post", "torch1", "torch2"
	};
	for (const char *name : geomPieces) {
		SpawnableItem item;
		item.displayName = name;
		item.objectName = name;
		item.category = 0;
		item.ability = (strstr(name, "door") != nullptr) ? -1 : 0;
		item.lightType = (strstr(name, "lamp") != nullptr) ? 900 : 0;
		mCatalog.push_back(item);
	}

	// -------------------------------------------------------------
	// Category 1: MD2 Monsters
	// -------------------------------------------------------------
	const char *monsters[] = {
		"GOBLIN", "OGRE", "TENTACLE", "WRAITH", "DEMON", "DEMONESS",
		"SORCERER", "NECROMANCER", "KNIGHT", "MUMMY", "WOLF", "SNAKE",
		"COBRA", "PHANTOM", "DRAGON", "SLAVE", "CORPSE"
	};
	for (const char *mon : monsters) {
		SpawnableItem item;
		item.displayName = mon;
		item.objectName = "!monster1";
		item.modelName = mon;
		item.texName = mon;
		// convert to lowercase for skin alias
		std::string tex = mon;
		std::transform(tex.begin(), tex.end(), tex.begin(), ::tolower);
		item.texName = tex;
		item.category = 1;
		mCatalog.push_back(item);
	}

	// -------------------------------------------------------------
	// Category 2: 3DS Props & Furniture
	// -------------------------------------------------------------
	struct PropDef {
		const char *disp;
		const char *mdl;
		const char *tex;
	} props[] = {
		{ "Bed", "bed", "-1" },
		{ "Stool", "stool", "-1" },
		{ "Table", "table", "-1" },
		{ "Crate", "crate", "-1" },
		{ "Chest Closed", "cdoorclosedchest", "0" },
		{ "Chest Open", "cdooropenchest", "0" },
		{ "Pot Small", "pot1", "-1" },
		{ "Pot Medium", "pot2", "-1" },
		{ "Pot Large", "pot3", "-1" },
		{ "Rock 1", "rock1", "0" },
		{ "Rock 2", "rock2", "0" },
		{ "Skull", "skull", "0" },
		{ "Goblet", "GOBLET", "-1" },
		{ "Spellbook", "spellbook", "-1" }
	};
	for (const auto &p : props) {
		SpawnableItem item;
		item.displayName = p.disp;
		item.objectName = "!monster1";
		item.modelName = p.mdl;
		item.texName = p.tex;
		item.category = 2;
		mCatalog.push_back(item);
	}

	// -------------------------------------------------------------
	// Category 3: Items & Pickups
	// -------------------------------------------------------------
	struct ItemDef {
		const char *disp;
		const char *mdl;
		const char *tex;
	} items[] = {
		{ "Health Potion", "POTION", "-1" },
		{ "Cheese", "cheese1", "-1" },
		{ "Bread", "bread1", "-1" },
		{ "Mushroom", "mushroom", "-1" },
		{ "Gold Coin", "COIN", "-1" },
		{ "Diamond", "diamond", "-1" },
		{ "Key", "KEY2", "-1" },
		{ "Armor Upgrade", "armour", "-1" },
		{ "Axe", "AXE", "-1" },
		{ "Battleaxe", "BATTLEAXE", "-1" },
		{ "Bastard Sword", "BASTARDSWORD", "-1" },
		{ "Flame Sword", "FLAMESWORD", "-1" },
		{ "Ice Sword", "ICESWORD", "-1" },
		{ "Morningstar", "MORNINGSTAR", "-1" },
		{ "Spiked Flail", "SPIKEDFLAIL", "-1" },
		{ "Super Flame Sword", "SUPERFLAMESWORD", "-1" },
		{ "Lightning Sword", "LIGHTNINGSWORD", "-1" },
		{ "Scroll Magic Missile", "SCROLL-MAGICMISSLE-", "-1" },
		{ "Scroll Fireball", "SCROLL-FIREBALL-", "-1" },
		{ "Scroll Lightning", "SCROLL-LIGHTNING-", "-1" },
		{ "Scroll Healing", "SCROLL-HEALING-", "-1" }
	};
	for (const auto &it : items) {
		SpawnableItem item;
		item.displayName = it.disp;
		item.objectName = "!monster1";
		item.modelName = it.mdl;
		item.texName = it.tex;
		item.category = 3;
		mCatalog.push_back(item);
	}

	// -------------------------------------------------------------
	// Category 4: Lights & Special Markers
	// -------------------------------------------------------------
	{
		SpawnableItem item;
		item.displayName = "Start Position Marker";
		item.objectName = "startpos";
		item.category = 4;
		mCatalog.push_back(item);
	}
	{
		SpawnableItem item;
		item.displayName = "Flicker Light Torch Post";
		item.objectName = "lamp_post";
		item.category = 4;
		item.lightType = 900; // Flicker
		mCatalog.push_back(item);
	}
	{
		SpawnableItem item;
		item.displayName = "Spotlight Post";
		item.objectName = "lamp_post";
		item.category = 4;
		item.lightType = SPOT_LIGHT_SOURCE;
		mCatalog.push_back(item);
	}
}

int DungeonEditor::PerformRaycast(const XMFLOAT3 &rayOrigin, const XMFLOAT3 &rayDir, float maxDist) {
	int bestIdx = -1;
	float bestDist = maxDist;

	XMVECTOR origin = XMLoadFloat3(&rayOrigin);
	XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&rayDir));

	for (int i = 0; i < oblist_length; i++) {
		XMFLOAT3 pos = { oblist[i].x, oblist[i].y, oblist[i].z };
		XMVECTOR objPos = XMLoadFloat3(&pos);
		XMVECTOR diff = objPos - origin;

		float proj = XMVectorGetX(XMVector3Dot(diff, dir));
		if (proj < 0.0f || proj > maxDist)
			continue;

		XMVECTOR closestPoint = origin + dir * proj;
		float distToLine = XMVectorGetX(XMVector3Length(objPos - closestPoint));

		// Hit radius
		float hitRadius = 120.0f;
		if (distToLine < hitRadius && proj < bestDist) {
			bestDist = proj;
			bestIdx = i;
		}
	}

	return bestIdx;
}

void DungeonEditor::Update(float dt, const XMFLOAT3 &camPos, float camYaw, float camPitch, bool mouseClicked) {
	if (!mActive)
		return;

	// Calculate camera direction vector
	float radYaw = camYaw * (XM_PI / 180.0f);
	float radPitch = camPitch * (XM_PI / 180.0f);

	XMFLOAT3 dirF;
	dirF.x = sinf(radYaw) * cosf(radPitch);
	dirF.y = sinf(radPitch);
	dirF.z = cosf(radYaw) * cosf(radPitch);

	// Raycast placement position on floor / target distance
	float placeDist = 400.0f;
	mPlacementPos.x = camPos.x + dirF.x * placeDist;
	mPlacementPos.y = camPos.y + dirF.y * placeDist + mPlacementYOffset;
	mPlacementPos.z = camPos.z + dirF.z * placeDist;

	if (mGridSnap && mGridSize > 1.0f) {
		mPlacementPos.x = floorf((mPlacementPos.x + mGridSize * 0.5f) / mGridSize) * mGridSize;
		mPlacementPos.z = floorf((mPlacementPos.z + mGridSize * 0.5f) / mGridSize) * mGridSize;
	}

	// Object selection via raycast mouse pick or click
	ImGuiIO &io = ImGui::GetIO();
	if (!io.WantCaptureMouse && mouseClicked) {
		int pickIdx = PerformRaycast(camPos, dirF);
		if (pickIdx >= 0) {
			mSelectedIndex = pickIdx;
		}
	}

	// Hotkeys
	if (!io.WantCaptureKeyboard) {
		// [Space] or [P] to spawn object
		if (ImGui::IsKeyPressed(ImGuiKey_P) || ImGui::IsKeyPressed(ImGuiKey_Space)) {
			SpawnSelected();
		}
		// [Delete] to remove selected object
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
			DeleteSelected();
		}
		// [Ctrl+D] to duplicate
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
			DuplicateSelected();
		}
	}
}

void DungeonEditor::SpawnSelected() {
	std::vector<SpawnableItem> itemsInCat;
	for (const auto &item : mCatalog) {
		if (item.category == mSelectedCategory) {
			itemsInCat.push_back(item);
		}
	}

	if (mSelectedItemInCat >= 0 && mSelectedItemInCat < (int)itemsInCat.size()) {
		SpawnItem(itemsInCat[mSelectedItemInCat], mPlacementPos, mPlacementRot);
	}
}

void DungeonEditor::SpawnItem(const SpawnableItem &item, const XMFLOAT3 &pos, float rotation) {
	if (oblist_length >= MAX_OBJECTLIST - 1) {
		sprintf_s(gActionMessage, "Cannot spawn: Maximum object limit reached!");
		UpdateScrollList(255, 0, 0);
		return;
	}

	int idx = oblist_length;
	oblist[idx].x = pos.x;
	oblist[idx].y = pos.y;
	oblist[idx].z = pos.z;
	oblist[idx].rot_angle = rotation;
	oblist[idx].castshadow = 1;
	oblist[idx].ability = item.ability;
	strcpy_s(oblist[idx].name, item.objectName.c_str());

	if (pCWorld) {
		oblist[idx].type = pCWorld->CheckObjectId((char *)item.objectName.c_str());
		if (oblist[idx].type == -1)
			oblist[idx].type = 0;
	} else {
		oblist[idx].type = 0;
	}

	// Allocate light source
	oblist[idx].light_source = new LIGHTSOURCE;
	oblist[idx].light_source->command = item.lightType;
	oblist[idx].light_source->position_x = pos.x;
	oblist[idx].light_source->position_y = pos.y + 20.0f;
	oblist[idx].light_source->position_z = pos.z;
	oblist[idx].light_source->direction_x = 0.0f;
	oblist[idx].light_source->direction_y = -1.0f;
	oblist[idx].light_source->direction_z = 0.0f;
	oblist[idx].light_source->rcolour = 12.0f;
	oblist[idx].light_source->gcolour = 10.0f;
	oblist[idx].light_source->bcolour = 8.0f;

	if (item.objectName == "!monster1") {
		int monnum = 1000 + idx;
		oblist[idx].monsterid = monnum;

		int mid = pCWorld ? pCWorld->FindModelID((char *)item.modelName.c_str()) : 0;
		int stex = FindTextureAlias((char *)item.texName.c_str());

		if (item.category == 1) {
			// Monster MD2
			AddMonster(pos.x, pos.y, pos.z, rotation, (float)mid, (float)stex, (float)monnum,
			           0, 0, 0, 0, 5, 2, "1d6", 18, (char *)item.displayName.c_str(), 10.0f, item.ability);
		} else {
			// Item or 3DS prop
			int skinVal = -1;
			if (item.texName == "0")
				skinVal = 0;
			else if (item.texName == "-1")
				skinVal = -1;
			else
				skinVal = stex;

			AddItem(pos.x, pos.y - 10.0f, pos.z, rotation, (float)mid, (float)skinVal, (float)monnum,
			        (char *)item.modelName.c_str(), (char *)item.texName.c_str(), item.ability);
		}
	}

	oblist_length++;
	mSelectedIndex = idx;

	sprintf_s(gActionMessage, "Spawned %s at (%.0f, %.0f, %.0f)", item.displayName.c_str(), pos.x, pos.y, pos.z);
	UpdateScrollList(0, 255, 0);
}

void DungeonEditor::DeleteSelected() {
	if (mSelectedIndex < 0 || mSelectedIndex >= oblist_length)
		return;

	int delId = oblist[mSelectedIndex].monsterid;

	// Clean up light_source
	if (oblist[mSelectedIndex].light_source) {
		delete oblist[mSelectedIndex].light_source;
		oblist[mSelectedIndex].light_source = nullptr;
	}

	// Remove from oblist by shifting
	for (int i = mSelectedIndex; i < oblist_length - 1; i++) {
		oblist[i] = oblist[i + 1];
	}
	oblist_length--;

	// Invalidate corresponding monster or item if delId was set
	if (delId > 0) {
		for (int m = 0; m < num_monsters; m++) {
			if (monster_list[m].monsterid == delId) {
				monster_list[m].bIsPlayerValid = FALSE;
				monster_list[m].bIsPlayerAlive = FALSE;
			}
		}
		for (int it = 0; it < itemlistcount; it++) {
			if (item_list[it].monsterid == delId) {
				item_list[it].bIsPlayerValid = FALSE;
				item_list[it].bIsPlayerAlive = FALSE;
			}
		}
	}

	sprintf_s(gActionMessage, "Deleted selected object (Index %d)", mSelectedIndex);
	UpdateScrollList(255, 100, 100);

	if (mSelectedIndex >= oblist_length) {
		mSelectedIndex = oblist_length - 1;
	}
}

void DungeonEditor::DuplicateSelected() {
	if (mSelectedIndex < 0 || mSelectedIndex >= oblist_length)
		return;

	OBJECTLIST src = oblist[mSelectedIndex];
	XMFLOAT3 newPos = { src.x + 50.0f, src.y, src.z + 50.0f };

	SpawnableItem item;
	item.displayName = src.name;
	item.objectName = src.name;
	item.category = 0;
	item.ability = src.ability;
	item.lightType = src.light_source ? src.light_source->command : 0;

	if (strstr(src.name, "!") != nullptr) {
		// Find model and skin name
		for (int m = 0; m < num_monsters; m++) {
			if (monster_list[m].monsterid == src.monsterid) {
				item.modelName = monster_list[m].rname;
				item.texName = monster_list[m].rname;
				item.category = 1;
				break;
			}
		}
		for (int it = 0; it < itemlistcount; it++) {
			if (item_list[it].monsterid == src.monsterid) {
				item.modelName = item_list[it].rname;
				item.texName = (item_list[it].skin_tex_id == 0) ? "0" : "-1";
				item.category = 3;
				break;
			}
		}
	}

	SpawnItem(item, newPos, src.rot_angle);
}

void DungeonEditor::ClearLevel() {
	for (int i = 0; i < oblist_length; i++) {
		if (oblist[i].light_source) {
			delete oblist[i].light_source;
			oblist[i].light_source = nullptr;
		}
	}
	oblist_length = 0;
	num_monsters = 0;
	itemlistcount = 0;
	num_players2 = 0;
	mSelectedIndex = -1;

	sprintf_s(gActionMessage, "Cleared all objects from level!");
	UpdateScrollList(255, 255, 0);
}

void DungeonEditor::SelectObject(int index) {
	if (index >= 0 && index < oblist_length) {
		mSelectedIndex = index;
	}
}

void DungeonEditor::Render3DHighlights() {
	if (!mActive)
		return;

	// In 3D rendering, selection highlight is rendered when selected index is valid
}

void DungeonEditor::RenderImGui() {
	if (!mActive)
		return;

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(520, 680), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Gary Gygax's Dungeon Builder v1.0", &mActive, ImGuiWindowFlags_MenuBar)) {
		ImGui::End();
		return;
	}

	// Menu Bar
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Save Map", "Ctrl+S")) {
				SaveWorldMap(mMapNameInput);
			}
			if (ImGui::MenuItem("Load Map")) {
				load_level(mMapNameInput);
			}
			if (ImGui::MenuItem("Clear Dungeon")) {
				ClearLevel();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit Editor", "F1")) {
				ToggleActive();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "MODULE CREATION ENGINE - DUNGEON STOMP");
	ImGui::Separator();

	if (ImGui::BeginTabBar("EditorTabs")) {

		// -------------------------------------------------------------
		// TAB 1: OBJECT CATALOG & SPAWNER
		// -------------------------------------------------------------
		if (ImGui::BeginTabItem("Catalog & Spawn")) {
			ImGui::Text("Select Category:");
			const char *categories[] = {
				"Dungeon Geometry", "MD2 Monsters", "3DS Props", "Items & Pickups", "Lights & Markers"
			};
			ImGui::Combo("Category", &mSelectedCategory, categories, IM_ARRAYSIZE(categories));

			ImGui::InputText("Search", mSearchFilter, IM_ARRAYSIZE(mSearchFilter));

			ImGui::Separator();
			ImGui::Text("Available Items:");

			ImGui::BeginChild("CatalogList", ImVec2(0, 220), true);
			std::string filterStr = mSearchFilter;
			std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

			int catItemIdx = 0;
			for (size_t i = 0; i < mCatalog.size(); i++) {
				if (mCatalog[i].category != mSelectedCategory)
					continue;

				std::string nameLower = mCatalog[i].displayName;
				std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

				if (!filterStr.empty() && nameLower.find(filterStr) == std::string::npos) {
					catItemIdx++;
					continue;
				}

				bool isSelected = (mSelectedItemInCat == catItemIdx);
				if (ImGui::Selectable(mCatalog[i].displayName.c_str(), isSelected)) {
					mSelectedItemInCat = catItemIdx;
				}
				catItemIdx++;
			}
			ImGui::EndChild();

			ImGui::Separator();
			ImGui::Text("Placement Parameters:");
			ImGui::Checkbox("Grid Snap", &mGridSnap);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("Grid Size", &mGridSize, 10.0f, 10.0f, 500.0f, "%.0f");

			ImGui::SliderFloat("Rotation (°)", &mPlacementRot, 0.0f, 360.0f, "%.0f°");
			ImGui::SameLine();
			if (ImGui::Button("0°"))
				mPlacementRot = 0.0f;
			ImGui::SameLine();
			if (ImGui::Button("90°"))
				mPlacementRot = 90.0f;
			ImGui::SameLine();
			if (ImGui::Button("180°"))
				mPlacementRot = 180.0f;
			ImGui::SameLine();
			if (ImGui::Button("270°"))
				mPlacementRot = 270.0f;

			ImGui::DragFloat("Height Offset Y", &mPlacementYOffset, 5.0f, -500.0f, 500.0f, "%.0f");

			ImGui::Text("Cursor Target: (%.0f, %.0f, %.0f)", mPlacementPos.x, mPlacementPos.y, mPlacementPos.z);

			if (ImGui::Button("PLACE OBJECT AT CURSOR [P / Space]", ImVec2(-1, 35))) {
				SpawnSelected();
			}

			ImGui::EndTabItem();
		}

		// -------------------------------------------------------------
		// TAB 2: INSPECTOR / SELECTION PROPERTIES
		// -------------------------------------------------------------
		if (ImGui::BeginTabItem("Inspector")) {
			if (mSelectedIndex < 0 || mSelectedIndex >= oblist_length) {
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected in world.");
				ImGui::Text("Click an object in 3D or pick from the 'World Objects' tab.");
			} else {
				OBJECTLIST &obj = oblist[mSelectedIndex];

				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "Selected Object [%d]: %s", mSelectedIndex, obj.name);
				ImGui::Separator();

				// Position Controls
				ImGui::Text("Position:");
				float pos[3] = { obj.x, obj.y, obj.z };
				if (ImGui::DragFloat3("X / Y / Z", pos, 5.0f, -50000.0f, 50000.0f, "%.1f")) {
					obj.x = pos[0];
					obj.y = pos[1];
					obj.z = pos[2];

					// Update light pos if attached
					if (obj.light_source) {
						obj.light_source->position_x = obj.x;
						obj.light_source->position_y = obj.y + 20.0f;
						obj.light_source->position_z = obj.z;
					}
				}

				// Rotation
				ImGui::SliderFloat("Rotation Angle", &obj.rot_angle, 0.0f, 360.0f, "%.0f°");
				if (ImGui::Button("+45°"))
					obj.rot_angle = fmodf(obj.rot_angle + 45.0f, 360.0f);
				ImGui::SameLine();
				if (ImGui::Button("+90°"))
					obj.rot_angle = fmodf(obj.rot_angle + 90.0f, 360.0f);
				ImGui::SameLine();
				if (ImGui::Button("-90°"))
					obj.rot_angle = fmodf(obj.rot_angle + 270.0f, 360.0f);

				// Cast Shadow & Ability
				int shadow = obj.castshadow;
				if (ImGui::Checkbox("Cast Shadow", (bool *)&shadow)) {
					obj.castshadow = shadow;
				}

				ImGui::InputInt("Ability / Key ID", &obj.ability);

				// Light Source properties
				if (obj.light_source && obj.light_source->command > 0) {
					ImGui::Separator();
					ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Light Source Properties:");

					const char *lightTypes[] = { "None", "Spotlight", "Directional", "Point", "Flicker" };
					int lType = 0;
					if (obj.light_source->command == SPOT_LIGHT_SOURCE)
						lType = 1;
					else if (obj.light_source->command == DIRECTIONAL_LIGHT_SOURCE)
						lType = 2;
					else if (obj.light_source->command == POINT_LIGHT_SOURCE)
						lType = 3;
					else if (obj.light_source->command == 900)
						lType = 4;

					if (ImGui::Combo("Light Type", &lType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
						if (lType == 1)
							obj.light_source->command = SPOT_LIGHT_SOURCE;
						else if (lType == 2)
							obj.light_source->command = DIRECTIONAL_LIGHT_SOURCE;
						else if (lType == 3)
							obj.light_source->command = POINT_LIGHT_SOURCE;
						else if (lType == 4)
							obj.light_source->command = 900;
						else
							obj.light_source->command = 0;
					}

					float col[3] = {
						obj.light_source->rcolour / 15.0f,
						obj.light_source->gcolour / 15.0f,
						obj.light_source->bcolour / 15.0f
					};
					if (ImGui::ColorEdit3("Color", col)) {
						obj.light_source->rcolour = col[0] * 15.0f;
						obj.light_source->gcolour = col[1] * 15.0f;
						obj.light_source->bcolour = col[2] * 15.0f;
					}
				}

				ImGui::Separator();
				if (ImGui::Button("DUPLICATE [Ctrl+D]", ImVec2(160, 30))) {
					DuplicateSelected();
				}
				ImGui::SameLine();
				if (ImGui::Button("DELETE [Del]", ImVec2(160, 30))) {
					DeleteSelected();
				}
			}

			ImGui::EndTabItem();
		}

		// -------------------------------------------------------------
		// TAB 3: WORLD OBJECTS LIST
		// -------------------------------------------------------------
		if (ImGui::BeginTabItem("World Objects")) {
			ImGui::Text("Total Objects in Level: %d", oblist_length);

			if (ImGui::BeginTable("WorldObjTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 380))) {
				ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Pos (X, Y, Z)", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < oblist_length; i++) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", i);

					ImGui::TableSetColumnIndex(1);
					bool isSel = (mSelectedIndex == i);
					if (ImGui::Selectable(oblist[i].name, isSel, ImGuiSelectableFlags_SpanAllColumns)) {
						mSelectedIndex = i;
					}

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%.0f, %.0f, %.0f", oblist[i].x, oblist[i].y, oblist[i].z);

					ImGui::TableSetColumnIndex(3);
					char btnLabel[32];
					sprintf_s(btnLabel, "Del##%d", i);
					if (ImGui::Button(btnLabel)) {
						mSelectedIndex = i;
						DeleteSelected();
					}
				}
				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}

		// -------------------------------------------------------------
		// TAB 4: MAP FILE MANAGEMENT
		// -------------------------------------------------------------
		if (ImGui::BeginTabItem("Map File Manager")) {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.8f, 1.0f), "Save / Load Dungeon Maps:");

			ImGui::InputText("Map File Name", mMapNameInput, IM_ARRAYSIZE(mMapNameInput));

			if (ImGui::Button("SAVE CURRENT MAP", ImVec2(200, 35))) {
				SaveWorldMap(mMapNameInput);
			}
			ImGui::SameLine();
			if (ImGui::Button("LOAD MAP", ImVec2(150, 35))) {
				load_level(mMapNameInput);
			}

			ImGui::Separator();
			ImGui::Text("Preset Campaign Maps:");

			const char *presets[] = {
				"level1", "level2", "level3", "level4", "level5",
				"level6", "level7", "level8", "level9", "level10", "custom1"
			};
			for (const char *preset : presets) {
				if (ImGui::Button(preset, ImVec2(90, 25))) {
					strcpy_s(mMapNameInput, preset);
					load_level((char *)preset);
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();

			ImGui::Separator();
			if (ImGui::Button("CLEAR ALL OBJECTS (NEW MAP)", ImVec2(-1, 30))) {
				ClearLevel();
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}
