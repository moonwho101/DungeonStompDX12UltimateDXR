#include "DungeonGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

namespace DungeonGen {

namespace {

struct Dir {
	float dx = 0.0f;
	float dz = 0.0f;
};

struct ExitDef {
	float x = 0.0f;
	float z = 0.0f;
	Dir outDir;
	float y = 0.0f;
};

struct PieceDef {
	std::string name;
	std::vector<ExitDef> exits;
};

struct Bounds {
	float minX = 0.0f;
	float minZ = 0.0f;
	float maxX = 0.0f;
	float maxZ = 0.0f;
};

struct PlacedPiece {
	std::string name;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	int rot = 0;
	Bounds bounds;
	std::set<std::pair<int, int>> usedSlots;
};

struct Exit {
	float wx = 0.0f;
	float wy = 0.0f;
	float wz = 0.0f;
	float wdx = 0.0f;
	float wdz = 0.0f;
	std::string sourceName;
	size_t sourcePieceIndex = 0;
};

struct Entity {
	std::string type;
	std::string name;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	int rot = 0;
	int id = 0;
	int state = 0;

	bool hasColor = false;
	float colorR = 0.0f, colorG = 0.0f, colorB = 0.0f;

	bool hasDir = false;
	float dirX = 0.0f, dirY = -1.0f, dirZ = 0.0f;
};

const Dir DIR_N = { 0.0f, 1.0f };
const Dir DIR_S = { 0.0f, -1.0f };
const Dir DIR_W = { -1.0f, 0.0f };
const Dir DIR_E = { 1.0f, 0.0f };

class RandomEngine {
  public:
	RandomEngine(unsigned int seed) {
		if (seed == 0) {
			std::random_device rd;
			engine.seed(rd());
		} else {
			engine.seed(seed);
		}
	}

	float randomFloat() {
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return dist(engine);
	}

	float uniform(float a, float b) {
		std::uniform_real_distribution<float> dist(a, b);
		return dist(engine);
	}

	int randInt(int a, int b) {
		std::uniform_int_distribution<int> dist(a, b);
		return dist(engine);
	}

	template <typename T>
	const T &choice(const std::vector<T> &vec) {
		int idx = randInt(0, static_cast<int>(vec.size()) - 1);
		return vec[idx];
	}

	template <typename T>
	std::vector<T> sample(const std::vector<T> &vec, int k) {
		std::vector<T> copy = vec;
		std::shuffle(copy.begin(), copy.end(), engine);
		if (k < static_cast<int>(copy.size())) {
			copy.resize(k);
		}
		return copy;
	}

	template <typename T>
	void shuffle(std::vector<T> &vec) {
		std::shuffle(vec.begin(), vec.end(), engine);
	}

  private:
	std::mt19937 engine;
};

void rotate_pt(float x, float z, int angle, float &rx, float &rz) {
	int norm_angle = ((angle % 360) + 360) % 360;
	if (norm_angle == 0) {
		rx = x;
		rz = z;
	} else if (norm_angle == 90) {
		rx = -z;
		rz = x;
	} else if (norm_angle == 180) {
		rx = -x;
		rz = -z;
	} else if (norm_angle == 270) {
		rx = z;
		rz = -x;
	} else {
		rx = x;
		rz = z;
	}
}

Bounds get_world_bounds(const Bounds &localBox, float ox, float oz, int rot) {
	float corners[4][2] = {
		{ localBox.minX, localBox.minZ },
		{ localBox.maxX, localBox.minZ },
		{ localBox.minX, localBox.maxZ },
		{ localBox.maxX, localBox.maxZ }
	};
	float minWpX = 1e9f, minWpZ = 1e9f, maxWpX = -1e9f, maxWpZ = -1e9f;
	for (int i = 0; i < 4; ++i) {
		float rx, rz;
		rotate_pt(corners[i][0], corners[i][1], rot, rx, rz);
		float wx = rx + ox;
		float wz = rz + oz;
		if (wx < minWpX)
			minWpX = wx;
		if (wx > maxWpX)
			maxWpX = wx;
		if (wz < minWpZ)
			minWpZ = wz;
		if (wz > maxWpZ)
			maxWpZ = wz;
	}
	return { minWpX, minWpZ, maxWpX, maxWpZ };
}

void hsv_to_rgb(float h, float s, float v, float &r, float &g, float &b) {
	if (s <= 0.0f) {
		r = v;
		g = v;
		b = v;
		return;
	}
	float h_i = h * 6.0f;
	int i = static_cast<int>(std::floor(h_i)) % 6;
	float f = h_i - std::floor(h_i);
	float p = v * (1.0f - s);
	float q = v * (1.0f - s * f);
	float t = v * (1.0f - s * (1.0f - f));
	switch (i) {
	case 0:
		r = v;
		g = t;
		b = p;
		break;
	case 1:
		r = q;
		g = v;
		b = p;
		break;
	case 2:
		r = p;
		g = v;
		b = t;
		break;
	case 3:
		r = p;
		g = q;
		b = v;
		break;
	case 4:
		r = t;
		g = p;
		b = v;
		break;
	case 5:
	default:
		r = v;
		g = p;
		b = q;
		break;
	}
}

float round_1dp(float val) {
	return std::round(val * 10.0f) / 10.0f;
}

int prefer_exit_index(const std::vector<Exit> &open_exits, const std::vector<PlacedPiece> &placed, float prefer_loop_chance, RandomEngine &rng) {
	if (placed.empty())
		return -1;
	if (rng.randomFloat() >= prefer_loop_chance)
		return -1;
	int best_idx = -1;
	float best_score = -1e9f;
	for (size_t i = 0; i < open_exits.size(); ++i) {
		float score = 0.0f;
		for (const auto &p : placed) {
			float dx = open_exits[i].wx - p.x;
			float dz = open_exits[i].wz - p.z;
			float dist = std::hypot(dx, dz);
			score += std::max(0.0f, 2000.0f - dist);
		}
		if (score > best_score) {
			best_score = score;
			best_idx = static_cast<int>(i);
		}
	}
	return best_idx;
}

std::string to_lower_str(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return str;
}

void write_single_file(const std::string &path, int startX, int startZ, int startRot,
                       const std::vector<PlacedPiece> &placed, const std::vector<Entity> &entities,
                       bool includeLargedungeonText, bool isNewObjects) {
	std::ofstream f(path);
	if (!f.is_open())
		return;

	f << std::fixed << std::setprecision(6);

	struct Link {
		float x, z;
		int rot;
		int id;
	};
	std::vector<Link> links;
	int obj_id = 0;

	f << "OBJECT startpos\n";
	f << "CO_ORDINATES " << static_cast<float>(startX) << " 0.000000 " << static_cast<float>(startZ) << "\n";
	f << "ROT_ANGLE " << startRot << "\n";
	links.push_back({ static_cast<float>(startX), static_cast<float>(startZ), startRot, obj_id++ });

	for (const auto &p : placed) {
		if (p.name == "left_curve" || p.name == "right_curve")
			continue;
		f << "OBJECT " << p.name << "\n";
		f << "CO_ORDINATES " << p.x << " " << p.y << " " << p.z << "\n";
		f << "ROT_ANGLE " << p.rot << "\n";
		links.push_back({ p.x, p.z, p.rot, obj_id++ });
	}

	for (const auto &e : entities) {
		const std::string &t = e.type;

		if (t == "wall") {
			if (isNewObjects) {
				f << "OBJECT !wall0-240-320\n";
			} else {
				f << "OBJECT !wall0-240-160\n";
			}
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " 0 " << e.name << " " << e.id << " 0\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "torch") {
			f << "OBJECT torch\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "!flamesnohit") {
			f << "OBJECT !flamesnohit\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " 0 " << e.name << " " << e.state << " 0\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "lamp_post") {
			f << "OBJECT lamp_post\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "LIGHT_SOURCE") {
			float dx = e.hasDir ? e.dirX : 0.0f;
			float dy = e.hasDir ? e.dirY : -1.0f;
			float dz = e.hasDir ? e.dirZ : 0.0f;
			float r = e.hasColor ? e.colorR : 9.0f;
			float g = e.hasColor ? e.colorG : 9.0f;
			float b = e.hasColor ? e.colorB : 9.0f;
			f << "LIGHT_SOURCE " << e.name << " POS " << e.x << " " << e.y << " " << e.z
			  << " DIR " << dx << " " << dy << " " << dz
			  << " COLOUR " << r << " " << g << " " << b << "\n";
		} else if (t == "dframe" || t == "curve") {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t.find("door") == 0) {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " " << e.state << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "slope_stairs") {
			f << "OBJECT slope_stairs\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "left_curve_road" || t == "right_curve_road") {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "!flarenohit") {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " 3 " << e.name << " " << e.id << " " << e.state << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else if (t == "!monster1") {
			f << "OBJECT !monster1\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " torch2 " << (e.name.empty() ? "0" : e.name) << " " << e.id << " 0\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		} else {
			f << "OBJECT !monster1\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " " << t << " " << e.name << " " << e.id << " " << e.state << "\n";
			links.push_back({ e.x, e.z, e.rot, obj_id++ });
		}
	}

	if (includeLargedungeonText) {
		f << "OBJECT text\n";
		f << "CO_ORDINATES 2020.000000 0.000000 1020.000000\n";
		f << "ROT_ANGLE 0 largedungeon\n";
		links.push_back({ 2020.0f, 1020.0f, 0, obj_id++ });
	}

	for (const auto &link : links) {
		f << "LINK " << link.x << " " << link.z << " " << link.rot << " " << link.id << "\n";
	}

	f << "END_FILE\n";
}

void write_map_file(const std::string &outputPath, int startX, int startZ, int startRot,
                    const std::vector<PlacedPiece> &placed, const std::vector<Entity> &entities,
                    bool includeLargedungeonText, bool isNewObjects) {
	write_single_file(outputPath, startX, startZ, startRot, placed, entities, includeLargedungeonText, isNewObjects);

	// If path doesn't start with bin/, also write to bin/path if bin directory exists
	if (outputPath.find("bin/") != 0 && outputPath.find("bin\\") != 0) {
		std::string binPath = "bin/" + outputPath;
		write_single_file(binPath, startX, startZ, startRot, placed, entities, includeLargedungeonText, isNewObjects);
	}
}

} // namespace

bool GenerateDungeonClassic(const std::string &outputPath, const GeneratorOptions &options) {
	RandomEngine rng(options.seed);

	std::map<std::string, PieceDef> OBJECTS = {
		{ "ROOM2", { "ROOM2", { { 0.0f, -160.0f, DIR_S, 0.0f }, { 0.0f, 160.0f, DIR_N, 0.0f } } } },
		{ "crossroads", { "crossroads", { { 80.0f, 0.0f, DIR_S }, { 80.0f, 240.0f, DIR_N }, { -40.0f, 120.0f, DIR_W }, { 200.0f, 120.0f, DIR_E } } } },
		{ "t_junction", { "t_junction", { { 80.0f, 0.0f, DIR_S }, { -40.0f, 120.0f, DIR_W }, { 200.0f, 120.0f, DIR_E } } } },
		{ "left_corner", { "left_corner", { { 80.0f, 0.0f, DIR_S }, { -40.0f, 120.0f, DIR_W } } } },
		{ "ROOM_SQUARE", { "ROOM_SQUARE", { { 240.0f, 0.0f, DIR_E } } } },
		{ "ROOMEDIUM", { "ROOMEDIUM", { { -160.0f, 0.0f, DIR_W }, { 160.0f, 0.0f, DIR_E } } } },
		{ "slope_stairs", { "slope_stairs", { { 0.0f, -160.0f, DIR_S, 80.0f }, { 0.0f, 160.0f, DIR_N, -60.0f } } } },
		{ "right_curve", { "right_curve", { { -140.0f, -140.0f, DIR_S }, { 140.0f, 140.0f, DIR_E } } } }
	};

	std::map<std::string, Bounds> BOUNDING_BOXES = {
		{ "ROOM2", { -78.0f, -158.0f, 78.0f, 158.0f } },
		{ "crossroads", { -38.0f, 2.0f, 198.0f, 238.0f } },
		{ "t_junction", { -38.0f, 2.0f, 198.0f, 238.0f } },
		{ "left_corner", { -38.0f, 2.0f, 158.0f, 198.0f } },
		{ "ROOM_SQUARE", { -238.0f, -238.0f, 238.0f, 238.0f } },
		{ "ROOMEDIUM", { -158.0f, -238.0f, 158.0f, 238.0f } },
		{ "slope_stairs", { -78.0f, -158.0f, 78.0f, 158.0f } },
		{ "right_curve", { -218.0f, -138.0f, 138.0f, 218.0f } }
	};

	std::map<std::string, std::vector<std::pair<float, float>>> FLOOR_SLOTS = {
		{ "ROOM2", { { 0.0f, -100.0f }, { 0.0f, -50.0f }, { 0.0f, 0.0f }, { 0.0f, 50.0f }, { 0.0f, 100.0f } } },
		{ "ROOM_SQUARE", { { 0.0f, 0.0f }, { -120.0f, -120.0f }, { 120.0f, -120.0f }, { -120.0f, 120.0f }, { 120.0f, 120.0f }, { -160.0f, 0.0f }, { 160.0f, 0.0f }, { 0.0f, -160.0f }, { 0.0f, 160.0f } } },
		{ "ROOMEDIUM", { { 0.0f, 0.0f }, { 0.0f, -120.0f }, { 0.0f, 120.0f }, { -80.0f, -120.0f }, { 80.0f, -120.0f }, { -80.0f, 120.0f }, { 80.0f, 120.0f }, { 0.0f, -200.0f }, { 0.0f, 200.0f } } }
	};

	std::vector<PlacedPiece> placed;
	std::vector<Entity> entities;
	std::vector<Exit> open_exits;
	std::vector<Exit> failed_exits;
	int entity_id_idx = 100;

	float start_x = static_cast<float>(options.startX);
	float start_z = static_cast<float>(options.startZ);

	placed.push_back({ "ROOM2", start_x, 0.0f, start_z, 0, BOUNDING_BOXES["ROOM2"] });

	for (const auto &ext : OBJECTS["ROOM2"].exits) {
		float rx, rz, rdx, rdz;
		rotate_pt(ext.x, ext.z, 0, rx, rz);
		rotate_pt(ext.outDir.dx, ext.outDir.dz, 0, rdx, rdz);
		open_exits.push_back({ start_x + rx, 0.0f + ext.y, start_z + rz, rdx, rdz, "ROOM2", 0 });
	}

	auto check_collision = [&](float nx, float nz, const std::string &n_name, int n_rot) {
		Bounds n_bounds = get_world_bounds(BOUNDING_BOXES[n_name], nx, nz, n_rot);
		for (const auto &p : placed) {
			Bounds p_bounds = get_world_bounds(BOUNDING_BOXES[p.name], p.x, p.z, p.rot);
			if (n_bounds.minX < p_bounds.maxX && n_bounds.maxX > p_bounds.minX &&
			    n_bounds.minZ < p_bounds.maxZ && n_bounds.maxZ > p_bounds.minZ) {
				return true;
			}
		}
		return false;
	};

	std::vector<std::string> objectKeys;
	for (const auto &kv : OBJECTS)
		objectKeys.push_back(kv.first);

	for (int i = 0; i < options.numObjectsToPlace; ++i) {
		if (open_exits.empty())
			break;

		int pref_idx = prefer_exit_index(open_exits, placed, 0.18f, rng);
		int exit_idx = (pref_idx >= 0) ? pref_idx : rng.randInt(0, static_cast<int>(open_exits.size()) - 1);

		Exit O = open_exits[exit_idx];
		open_exits.erase(open_exits.begin() + exit_idx);

		float wx = O.wx, wy = O.wy, wz = O.wz;
		float wdx = O.wdx, wdz = O.wdz;

		bool placed_new = false;
		std::vector<std::string> types = objectKeys;
		rng.shuffle(types);

		for (const auto &cand_name : types) {
			if (cand_name == "ROOM_SQUARE" && rng.randomFloat() > 0.30f)
				continue;

			if (placed_new)
				break;

			const auto &cand_exits = OBJECTS[cand_name].exits;
			std::vector<size_t> indices(cand_exits.size());
			for (size_t idx = 0; idx < indices.size(); ++idx)
				indices[idx] = idx;
			rng.shuffle(indices);

			for (size_t ext_idx : indices) {
				if (placed_new)
					break;

				const auto &loc_ext = cand_exits[ext_idx];
				float lx = loc_ext.x, lz = loc_ext.z;
				float ldx = loc_ext.outDir.dx, ldz = loc_ext.outDir.dz;

				std::vector<int> rots = { 0, 90, 180, 270 };
				rng.shuffle(rots);

				for (int ang : rots) {
					float rdx, rdz;
					rotate_pt(ldx, ldz, ang, rdx, rdz);

					if (std::abs(rdx - (-wdx)) < 1e-4f && std::abs(rdz - (-wdz)) < 1e-4f) {
						float rx, rz;
						rotate_pt(lx, lz, ang, rx, rz);
						float Ox = wx - rx;
						float Oy = wy - loc_ext.y;
						float Oz = wz - rz;

						if (!check_collision(Ox, Oz, cand_name, ang)) {
							placed.push_back({ cand_name, Ox, Oy, Oz, ang, BOUNDING_BOXES[cand_name] });
							placed_new = true;

							std::vector<std::pair<float, float>> room_slots;
							if (FLOOR_SLOTS.count(cand_name)) {
								room_slots = FLOOR_SLOTS[cand_name];
								rng.shuffle(room_slots);
							}

							if (cand_name == "right_curve") {
								struct RcSegment {
									float dx, dz;
									int dr;
								};
								std::vector<RcSegment> rc_pieces = {
									{ -220.00f, -140.00f, 0 },
									{ -207.73f, -46.83f, 345 },
									{ -171.77f, 39.98f, 330 },
									{ -114.56f, 114.54f, 315 },
									{ -40.00f, 171.74f, 300 },
									{ 46.82f, 207.70f, 285 }
								};
								for (const auto &rc : rc_pieces) {
									float rx, rz;
									rotate_pt(rc.dx, rc.dz, ang, rx, rz);
									int tr = (rc.dr + ang) % 360;
									entities.push_back({ "right_curve_road", "", Ox + rx, Oy, Oz + rz, tr, 0, 0 });
								}
							}

							if (rng.randomFloat() < 0.25f) {
								std::vector<float> heights = { 200.0f, 300.0f, 400.0f };
								float s_y = Oy + rng.choice(heights);
								float hue = rng.randomFloat();
								float sat = rng.uniform(0.4f, 1.0f);
								float val = rng.uniform(0.3f, 0.9f);
								float r, g, b;
								hsv_to_rgb(hue, sat, val, r, g, b);

								entities.push_back({ "lamp_post", "", Ox, s_y, Oz, 0, entity_id_idx, 0 });

								float dx = rng.uniform(-0.4f, 0.4f);
								float dz = rng.uniform(-0.4f, 0.4f);
								float dy = -1.0f;
								float len = std::sqrt(dx * dx + dy * dy + dz * dz);
								dx /= len;
								dy /= len;
								dz /= len;

								Entity spotlight;
								spotlight.type = "LIGHT_SOURCE";
								spotlight.name = "Spotlight";
								spotlight.x = Ox;
								spotlight.y = s_y;
								spotlight.z = Oz;
								spotlight.rot = 0;
								spotlight.id = entity_id_idx;
								spotlight.state = 0;
								spotlight.hasColor = true;
								spotlight.colorR = r;
								spotlight.colorG = g;
								spotlight.colorB = b;
								spotlight.hasDir = true;
								spotlight.dirX = dx;
								spotlight.dirY = dy;
								spotlight.dirZ = dz;
								entities.push_back(spotlight);
							}

							if (cand_name == "ROOM2") {
								if (rng.randomFloat() < 0.40f) {
									bool is_left = rng.randomFloat() < 0.50f;
									std::vector<float> z_opts = { -100.0f, 100.0f };
									float z_offset = rng.choice(z_opts);
									float tlx, tlz, tfx, tfz;
									int trot;
									if (is_left) {
										tlx = -80.0f;
										tlz = z_offset;
										tfx = -60.0f;
										tfz = z_offset;
										trot = 270;
									} else {
										tlx = 80.0f;
										tlz = z_offset;
										tfx = 60.0f;
										tfz = z_offset;
										trot = 90;
									}
									float wlx, wlz, wfx, wfz;
									rotate_pt(tlx, tlz, ang, wlx, wlz);
									rotate_pt(tfx, tfz, ang, wfx, wfz);
									int t_rot = (trot + ang) % 360;

									entities.push_back({ "torch", "", Ox + wlx, Oy + 60.0f, Oz + wlz, t_rot, entity_id_idx, 0 });
									entities.push_back({ "!flamesnohit", "flame@1", Ox + wfx, Oy + 60.0f, Oz + wfz, 0, entity_id_idx, 4 });
									entities.push_back({ "lamp_post", "", Ox + wfx, Oy + 80.0f, Oz + wfz, 0, entity_id_idx, 0 });

									Entity flickerLight;
									flickerLight.type = "LIGHT_SOURCE";
									flickerLight.name = "flicker";
									flickerLight.x = Ox + wfx;
									flickerLight.y = Oy + 0.0f;
									flickerLight.z = Oz + wfz;
									flickerLight.rot = 0;
									flickerLight.id = entity_id_idx;
									flickerLight.state = 0;
									entities.push_back(flickerLight);
								}
							}

							if (cand_name == "ROOM2" || cand_name == "ROOM_SQUARE" || cand_name == "ROOMEDIUM" || cand_name == "slope_stairs") {
								if (rng.randomFloat() < 0.45f) {
									float light_y = Oy + 140.0f;
									entities.push_back({ "lamp_post", "", Ox, light_y, Oz, 0, entity_id_idx, 0 });
									entity_id_idx++;

									float center_r = round_1dp(rng.uniform(9.0f, 14.0f));
									float center_g = round_1dp(rng.uniform(9.0f, 14.0f));
									float center_b = round_1dp(rng.uniform(9.0f, 14.0f));

									Entity centerLight;
									centerLight.type = "LIGHT_SOURCE";
									centerLight.name = "flicker";
									centerLight.x = Ox;
									centerLight.y = light_y;
									centerLight.z = Oz;
									centerLight.rot = 0;
									centerLight.id = entity_id_idx;
									centerLight.state = 0;
									centerLight.hasColor = true;
									centerLight.colorR = center_r;
									centerLight.colorG = center_g;
									centerLight.colorB = center_b;
									centerLight.hasDir = true;
									centerLight.dirX = 0.0f;
									centerLight.dirY = -1.0f;
									centerLight.dirZ = 0.0f;
									entities.push_back(centerLight);

									entities.push_back({ "torch1", "0", Ox, Oy + 120.0f, Oz, 0, entity_id_idx, 0 });
									entity_id_idx++;
								}
							}

							if (rng.randomFloat() < 0.25f) {
								std::vector<int> door_ids;
								for (int d = 1; d <= 21; ++d) {
									if (d != 2)
										door_ids.push_back(d);
								}
								std::string door_type = "door" + std::to_string(rng.choice(door_ids));
								int d_rot = 0;
								if (std::abs(wdx - 0.0f) < 1e-4f && std::abs(wdz - 1.0f) < 1e-4f)
									d_rot = 0;
								else if (std::abs(wdx - (-1.0f)) < 1e-4f && std::abs(wdz - 0.0f) < 1e-4f)
									d_rot = 90;
								else if (std::abs(wdx - 0.0f) < 1e-4f && std::abs(wdz - (-1.0f)) < 1e-4f)
									d_rot = 180;
								else
									d_rot = 270;

								float dlx = -40.0f, dlz = 0.0f;
								float dwx, dwz;
								rotate_pt(dlx, dlz, d_rot, dwx, dwz);

								std::vector<std::string> frames = { "dframe", "curve" };
								std::string frame_type = rng.choice(frames);
								float door_wy = wy;
								if (frame_type == "curve")
									door_wy += 110.0f;

								entities.push_back({ frame_type, "", wx, door_wy, wz, d_rot, entity_id_idx, 0 });

								if (frame_type == "dframe" && rng.randomFloat() < 0.50f) {
									entities.push_back({ door_type, "", wx + dwx, door_wy, wz + dwz, d_rot, entity_id_idx, 0 });
								}
							}

							if (cand_name == "ROOM_SQUARE" || cand_name == "ROOMEDIUM") {
								std::vector<std::string> ttypes = { "torch", "torch1" };
								std::string torch_type = rng.choice(ttypes);

								struct TorchSlot {
									float tlx, tlz, tfx, tfz;
									int trot;
								};

								if (torch_type == "torch") {
									std::vector<TorchSlot> slots;
									if (cand_name == "ROOM_SQUARE") {
										slots = {
											{ -240.0f, -120.0f, -220.0f, -120.0f, 270 },
											{ -240.0f, 120.0f, -220.0f, 120.0f, 270 },
											{ 240.0f, -120.0f, 220.0f, -120.0f, 90 },
											{ 240.0f, 120.0f, 220.0f, 120.0f, 90 },
											{ -120.0f, -240.0f, -120.0f, -220.0f, 0 },
											{ 120.0f, -240.0f, 120.0f, -220.0f, 0 },
											{ -120.0f, 240.0f, -120.0f, 220.0f, 180 },
											{ 120.0f, 240.0f, 120.0f, 220.0f, 180 }
										};
									} else {
										slots = {
											{ -160.0f, -120.0f, -140.0f, -120.0f, 270 },
											{ -160.0f, 120.0f, -140.0f, 120.0f, 270 },
											{ 160.0f, -120.0f, 140.0f, -120.0f, 90 },
											{ 160.0f, 120.0f, 140.0f, 120.0f, 90 },
											{ -80.0f, -240.0f, -80.0f, -220.0f, 0 },
											{ 80.0f, -240.0f, 80.0f, -220.0f, 0 },
											{ -80.0f, 240.0f, -80.0f, 220.0f, 180 },
											{ 80.0f, 240.0f, 80.0f, 220.0f, 180 }
										};
									}
									int num_torches = rng.randInt(2, 4);
									std::vector<TorchSlot> chosen_slots = rng.sample(slots, num_torches);
									for (const auto &ts : chosen_slots) {
										float wlx, wlz, wfx, wfz;
										rotate_pt(ts.tlx, ts.tlz, ang, wlx, wlz);
										rotate_pt(ts.tfx, ts.tfz, ang, wfx, wfz);
										int t_rot = (ts.trot + ang) % 360;

										entities.push_back({ "torch", "", Ox + wlx, Oy + 60.0f, Oz + wlz, t_rot, entity_id_idx, 0 });
										entities.push_back({ "!flamesnohit", "flame@1", Ox + wfx, Oy + 60.0f, Oz + wfz, 0, entity_id_idx, 4 });
										entities.push_back({ "lamp_post", "", Ox + wfx, Oy + 80.0f, Oz + wfz, 0, entity_id_idx, 0 });

										Entity flickerLight;
										flickerLight.type = "LIGHT_SOURCE";
										flickerLight.name = "flicker";
										flickerLight.x = Ox + wfx;
										flickerLight.y = Oy + 0.0f;
										flickerLight.z = Oz + wfz;
										flickerLight.rot = 0;
										flickerLight.id = entity_id_idx;
										flickerLight.state = 0;
										entities.push_back(flickerLight);
										entity_id_idx++;
									}
								} else {
									std::vector<TorchSlot> slots;
									if (cand_name == "ROOM_SQUARE") {
										slots = {
											{ -220.0f, -120.0f, -200.0f, -120.0f, 270 },
											{ -220.0f, 120.0f, -200.0f, 120.0f, 270 },
											{ 220.0f, -120.0f, 200.0f, -120.0f, 90 },
											{ 220.0f, 120.0f, 200.0f, 120.0f, 90 },
											{ -120.0f, -220.0f, -120.0f, -200.0f, 0 },
											{ 120.0f, -220.0f, 120.0f, -200.0f, 0 },
											{ -120.0f, 220.0f, -120.0f, 200.0f, 180 },
											{ 120.0f, 220.0f, 120.0f, 200.0f, 180 }
										};
									} else {
										slots = {
											{ -140.0f, -120.0f, -120.0f, -120.0f, 270 },
											{ -140.0f, 120.0f, -120.0f, 120.0f, 270 },
											{ 140.0f, -120.0f, 120.0f, -120.0f, 90 },
											{ 140.0f, 120.0f, 120.0f, 120.0f, 90 },
											{ -80.0f, -220.0f, -80.0f, -200.0f, 0 },
											{ 80.0f, -220.0f, 80.0f, -200.0f, 0 },
											{ -80.0f, 220.0f, -80.0f, 200.0f, 180 },
											{ 80.0f, 220.0f, 80.0f, 200.0f, 180 }
										};
									}
									int num_torches = rng.randInt(2, 4);
									std::vector<TorchSlot> chosen_slots = rng.sample(slots, num_torches);

									struct ColorRGB {
										float r, g, b;
									};
									std::vector<ColorRGB> base_colors = {
										{ 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, { 1.0f, 0.5f, 0.0f }, { 0.5f, 0.0f, 1.0f }, { 1.0f, 0.75f, 0.8f }
									};
									ColorRGB base_color = rng.choice(base_colors);
									float torch_r = round_1dp(base_color.r * rng.uniform(9.0f, 13.0f));
									float torch_g = round_1dp(base_color.g * rng.uniform(9.0f, 13.0f));
									float torch_b = round_1dp(base_color.b * rng.uniform(9.0f, 13.0f));

									for (const auto &ts : chosen_slots) {
										float wlx, wlz, wfx, wfz;
										rotate_pt(ts.tlx, ts.tlz, ang, wlx, wlz);
										rotate_pt(ts.tfx, ts.tfz, ang, wfx, wfz);
										int t_rot = ((ts.trot + ang - 90) % 360 + 360) % 360;

										entities.push_back({ "torch2", "0", Ox + wlx, Oy + 60.0f, Oz + wlz, t_rot, entity_id_idx, 0 });
										entities.push_back({ "lamp_post", "", Ox + wlx, Oy + 80.0f, Oz + wlz, 0, entity_id_idx, 0 });

										Entity colorLight;
										colorLight.type = "LIGHT_SOURCE";
										colorLight.name = "flicker";
										colorLight.x = Ox + wfx;
										colorLight.y = Oy + 0.0f;
										colorLight.z = Oz + wfz;
										colorLight.rot = 0;
										colorLight.id = entity_id_idx;
										colorLight.state = 0;
										colorLight.hasColor = true;
										colorLight.colorR = torch_r;
										colorLight.colorG = torch_g;
										colorLight.colorB = torch_b;
										colorLight.hasDir = true;
										colorLight.dirX = 0.0f;
										colorLight.dirY = -1.0f;
										colorLight.dirZ = 0.0f;
										entities.push_back(colorLight);
										entity_id_idx++;
									}
								}
							}

							if (cand_name == "ROOM_SQUARE" || cand_name == "ROOMEDIUM") {
								int num_dressings = rng.randInt(1, 2);
								for (int d = 0; d < num_dressings; ++d) {
									std::vector<std::string> dtypes = { "TABLE", "stool", "BED", "TROUGH", "LOGS", "BUCKET", "QUARTZ", "rock2" };
									std::string dressing_type = rng.choice(dtypes);
									std::vector<int> drots = { 0, 45, 90, 135, 180, 225, 270, 315 };
									int d_rot = rng.choice(drots);

									float lx, lz;
									if (!room_slots.empty()) {
										auto slot = room_slots.back();
										room_slots.pop_back();
										lx = slot.first;
										lz = slot.second;
									} else {
										Bounds b = BOUNDING_BOXES[cand_name];
										float margin = 40.0f;
										lx = rng.uniform(b.minX + margin, b.maxX - margin);
										lz = rng.uniform(b.minZ + margin, b.maxZ - margin);
									}

									float rx, rz;
									rotate_pt(lx, lz, ang, rx, rz);
									float dx = Ox + rx;
									float dz = Oz + rz;

									entities.push_back({ dressing_type, "0", dx, Oy - 25.0f, dz, d_rot, entity_id_idx++, 0 });

									if (dressing_type == "BED" && rng.randomFloat() < 0.50f) {
										entities.push_back({ "BLANKET", "0", dx, Oy - 28.0f, dz, d_rot, entity_id_idx++, 0 });
									}

									if (dressing_type == "TABLE" && rng.randomFloat() < 0.60f) {
										std::vector<std::string> cups = { "MUG", "GOBLET" };
										std::string cup_type = rng.choice(cups);
										float adjust = (cup_type == "MUG") ? 15.0f : 20.0f;
										entities.push_back({ cup_type, "-1", dx, Oy + adjust, dz, d_rot, entity_id_idx++, 2 });
									}
								}
							}

							if (cand_name == "ROOM2" || cand_name == "ROOM_SQUARE" || cand_name == "ROOMEDIUM" || cand_name == "slope_stairs") {
								float r = rng.randomFloat();
								float depth = std::max(0.0f, -Oy);
								int level = std::min(3, static_cast<int>(depth / 140.0f));
								std::vector<int> drots = { 0, 45, 90, 135, 180, 225, 270, 315 };
								int d_rot = rng.choice(drots);

								if (r < 0.74f) {
									float lx = 0.0f, lz = 0.0f;
									if (!room_slots.empty()) {
										auto slot = room_slots.back();
										room_slots.pop_back();
										lx = slot.first;
										lz = slot.second;
									}
									float rx, rz;
									rotate_pt(lx, lz, ang, rx, rz);
									float wx = Ox + rx;
									float wz = Oz + rz;

									std::string ent_type;
									if (r < 0.12f) {
										std::vector<std::string> items = { "POTION", "cheese1", "GOBLET", "pot1", "pot2", "pot3", "mushroom" };
										ent_type = rng.choice(items);
										entities.push_back({ ent_type, "-1", wx, Oy - 12.0f, wz, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.15f) {
										ent_type = "armour";
										entities.push_back({ ent_type, "-1", wx, Oy + 30.0f, wz, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.22f) {
										ent_type = "COIN";
										entities.push_back({ ent_type, "-1", wx, Oy - 12.0f, wz, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.26f) {
										ent_type = "spellbook";
										entities.push_back({ ent_type, "-1", wx, Oy - 12.0f, wz, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.30f) {
										std::vector<std::string> scrolls = { "SCROLL-HEALING-", "SCROLL-MAGICMISSLE-", "SCROLL-FIREBALL-", "SCROLL-LIGHTNING-" };
										ent_type = rng.choice(scrolls);
										entities.push_back({ ent_type, "-1", wx, Oy - 12.0f, wz, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.36f) {
										std::vector<std::string> weapon_types;
										if (level == 0)
											weapon_types = { "BASTARDSWORD", "FLAMESWORD", "BATTLEAXE" };
										else if (level == 1)
											weapon_types = { "ICESWORD", "LIGHTNINGSWORD", "MORNINGSTAR" };
										else
											weapon_types = { "SPLITSWORD", "SPIKEDFLAIL", "SUPERFLAMESWORD" };
										ent_type = rng.choice(weapon_types);
										entities.push_back({ ent_type, "-1", wx, Oy + 22.0f, wz, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.44f) {
										if (cand_name != "slope_stairs") {
											ent_type = "CHEST";
											std::vector<std::string> chests = { "cdoorclosedwoodbox", "cdoorclosedbarrel", "cdoorclosedmetalbox" };
											std::string chest_choice = rng.choice(chests);
											int st = (level < 2) ? rng.randInt(0, 1) : rng.randInt(0, 2);
											entities.push_back({ chest_choice, "0", wx, Oy - 22.0f, wz, d_rot, entity_id_idx++, st });
										}
									} else {
										std::vector<std::string> possible_mobs;
										if (level == 0)
											possible_mobs = { "GOBLIN", "TENTACLE" };
										else if (level == 1)
											possible_mobs = { "OGRE", "CORPSE", "MUMMY", "WOLF", "COBRA", "OGRO" };
										else if (level == 2)
											possible_mobs = { "NECROMANCER", "SORCERER", "WRAITH", "PHANTOM", "KNIGHT", "SLAVE" };
										else
											possible_mobs = { "FAERIE", "BAUUL", "DEMONESS", "DRAGON" };
										ent_type = rng.choice(possible_mobs);
										std::string name_val = to_lower_str(ent_type);
										std::vector<int> states = { 0, 2 };
										int st = rng.choice(states);
										entities.push_back({ ent_type, name_val, wx, Oy + 10.0f + (depth / 140.0f) * 2.0f, wz, d_rot, entity_id_idx++, st });
									}
								}
							}

							for (size_t other_i = 0; other_i < cand_exits.size(); ++other_i) {
								if (other_i == ext_idx)
									continue;
								const auto &other_ext = cand_exits[other_i];
								float opx, opz, odx, odz;
								rotate_pt(other_ext.x, other_ext.z, ang, opx, opz);
								rotate_pt(other_ext.outDir.dx, other_ext.outDir.dz, ang, odx, odz);
								open_exits.push_back({ Ox + opx, Oy + other_ext.y, Oz + opz, odx, odz, cand_name, placed.size() - 1 });
							}
						}
						break;
					}
				}
			}
		}

		if (!placed_new) {
			failed_exits.push_back(O);
		}
	}

	open_exits.insert(open_exits.end(), failed_exits.begin(), failed_exits.end());

	struct ExitKey {
		long long x, y, z;
		int dx, dz;
		bool operator<(const ExitKey &o) const {
			return std::tie(x, y, z, dx, dz) < std::tie(o.x, o.y, o.z, o.dx, o.dz);
		}
	};

	std::vector<ExitKey> all_piece_exits;
	for (const auto &p : placed) {
		for (const auto &ext : OBJECTS[p.name].exits) {
			float rx, rz, rdx, rdz;
			rotate_pt(ext.x, ext.z, p.rot, rx, rz);
			rotate_pt(ext.outDir.dx, ext.outDir.dz, p.rot, rdx, rdz);
			long long ey = static_cast<long long>(std::round(p.y + ext.y));
			long long ex = static_cast<long long>(std::round(p.x + rx));
			long long ez = static_cast<long long>(std::round(p.z + rz));
			all_piece_exits.push_back({ ex, ey, ez, static_cast<int>(std::round(rdx)), static_cast<int>(std::round(rdz)) });
		}
	}

	std::set<std::tuple<long long, long long, long long>> connected_positions;
	for (const auto &e1 : all_piece_exits) {
		for (const auto &e2 : all_piece_exits) {
			if (e1.x == e2.x && e1.y == e2.y && e1.z == e2.z && e1.dx == -e2.dx && e1.dz == -e2.dz) {
				connected_positions.insert(std::make_tuple(e1.x, e1.y, e1.z));
			}
		}
	}

	std::vector<Exit> dead_end_exits;
	for (const auto &o : open_exits) {
		auto key = std::make_tuple(static_cast<long long>(std::round(o.wx)),
		                           static_cast<long long>(std::round(o.wy)),
		                           static_cast<long long>(std::round(o.wz)));
		if (connected_positions.find(key) == connected_positions.end()) {
			dead_end_exits.push_back(o);
		}
	}

	for (const auto &open_ex : dead_end_exits) {
		float wx = open_ex.wx, wy = open_ex.wy, wz = open_ex.wz;
		float wdx = open_ex.wdx, wdz = open_ex.wdz;
		int ndx = static_cast<int>(std::round(-wdx));
		int ndz = static_cast<int>(std::round(-wdz));

		int wall_rot = 0;
		if (ndx == 0 && ndz == -1)
			wall_rot = 270;
		else if (ndx == 1 && ndz == 0)
			wall_rot = 0;
		else if (ndx == 0 && ndz == 1)
			wall_rot = 90;
		else if (ndx == -1 && ndz == 0)
			wall_rot = 180;

		wx += static_cast<float>(ndz) * 80.0f;
		wz += static_cast<float>(-ndx) * 80.0f;

		entities.push_back({ "wall", "cobblestone4", wx, wy, wz, wall_rot, entity_id_idx++, 0 });
	}

	if (!placed.empty()) {
		size_t deepest_idx = 0;
		float min_y = placed[0].y;
		for (size_t p = 1; p < placed.size(); ++p) {
			if (placed[p].y < min_y) {
				min_y = placed[p].y;
				deepest_idx = p;
			}
		}

		float tx = placed[deepest_idx].x;
		float ty = placed[deepest_idx].y;
		float tz = placed[deepest_idx].z;
		std::string deepest_name = placed[deepest_idx].name;
		int deepest_rot = placed[deepest_idx].rot;

		if (FLOOR_SLOTS.count(deepest_name)) {
			std::set<std::pair<int, int>> used_local_slots;
			for (const auto &e : entities) {
				float dx = e.x - tx;
				float dz = e.z - tz;
				float lx, lz;
				rotate_pt(dx, dz, (360 - deepest_rot) % 360, lx, lz);
				used_local_slots.insert({ static_cast<int>(std::round(lx)), static_cast<int>(std::round(lz)) });
			}

			std::vector<std::pair<float, float>> free_slots;
			for (const auto &slot : FLOOR_SLOTS[deepest_name]) {
				if (slot.first == 0.0f && slot.second == 0.0f)
					continue;
				std::pair<int, int> key = { static_cast<int>(std::round(slot.first)), static_cast<int>(std::round(slot.second)) };
				if (used_local_slots.find(key) == used_local_slots.end()) {
					free_slots.push_back(slot);
				}
			}

			if (!free_slots.empty()) {
				auto target_slot = rng.choice(free_slots);
				float rx, rz;
				rotate_pt(target_slot.first, target_slot.second, deepest_rot, rx, rz);
				float new_wx = tx + rx;
				float new_wz = tz + rz;

				for (auto &e : entities) {
					if (std::abs(e.x - tx) < 1.0f && std::abs(e.z - tz) < 1.0f &&
					    e.type != "circle" && e.type != "spiral" && e.type != "!flarenohit" &&
					    e.type != "lamp_post" && e.type != "LIGHT_SOURCE") {
						e.x = new_wx;
						e.z = new_wz;
					}
				}
			}
		}

		entities.push_back({ "circle", "0", tx, ty - 41.0f, tz, 0, entity_id_idx++, 2 });
		entities.push_back({ "spiral", "-1", tx, ty, tz, 0, entity_id_idx++, 3 });
		entities.push_back({ "!flarenohit", "flare@1", tx, ty, tz, 0, entity_id_idx++, 2 });
	}

	write_map_file(outputPath, options.startX, options.startZ, 0, placed, entities, false, false);
	return true;
}

bool GenerateDungeonNewObjects(const std::string &outputPath, const GeneratorOptions &options) {
	RandomEngine rng(options.seed);

	std::map<std::string, PieceDef> OBJECTS = {
		{ "CORRIDOR01", { "CORRIDOR01", { { -368.0f, 0.0f, DIR_W, 0.0f }, { 355.0f, 0.0f, DIR_E, 0.0f } } } },
		{ "CORRIDOR02", { "CORRIDOR02", { { -284.0f, 0.0f, DIR_W, 72.0f - 74.5f }, { 295.0f, 0.0f, DIR_E, 414.0f - 74.5f } } } },
		{ "CORRIDOR03", { "CORRIDOR03", { { -180.0f, 0.0f, DIR_W, 0.0f }, { 180.0f, 0.0f, DIR_E, 0.0f } } } },
		{ "CROSSING01", { "CROSSING01", { { -60.0f, -340.0f, DIR_S, 0.0f }, { 365.0f, 80.0f, DIR_E, 0.0f } } } },
		{ "CROSSING02", { "CROSSING02", { { 0.0f, 384.0f, DIR_N, 0.0f }, { 0.0f, -369.0f, DIR_S, 0.0f }, { -406.0f, 0.0f, DIR_W, 0.0f }, { 405.0f, 0.0f, DIR_E, 0.0f } } } },
		{ "CROSSING03", { "CROSSING03", { { 0.0f, -334.0f, DIR_S, 0.0f }, { -412.0f, 20.0f, DIR_W, 0.0f }, { 399.0f, 20.0f, DIR_E, 0.0f } } } },
		{ "ROOM02", { "ROOM02", { { 0.0f, -173.0f, DIR_S, 0.0f } } } },
		{ "ROOM05", { "ROOM05", { { 455.0f, -305.0f - 145.0f, DIR_E, 0.0f } } } },
		{ "ROOM06", { "ROOM06", { { 0.0f, -311.0f, DIR_S, 0.0f } } } }
	};

	std::map<std::string, Bounds> BOUNDING_BOXES_FULL = {
		{ "CORRIDOR01", { -372.0f, -297.0f, 358.0f, 303.0f } },
		{ "CORRIDOR02", { -288.0f, -298.0f, 298.0f, 302.0f } },
		{ "CORRIDOR03", { -185.0f, -299.0f, 182.0f, 301.0f } },
		{ "CROSSING01", { -447.0f, -453.0f, 482.0f, 413.0f } },
		{ "CROSSING02", { -486.0f, -458.0f, 491.0f, 475.0f } },
		{ "CROSSING03", { -492.0f, -422.0f, 485.0f, 420.0f } },
		{ "ROOM02", { -255.0f, -173.0f, 249.0f, 165.0f } },
		{ "ROOM06", { -297.0f, -338.0f, 294.0f, 339.0f } },
		{ "ROOM05", { -505.0f, -831.0f, 517.0f, 839.0f } }
	};

	std::map<std::string, std::vector<std::pair<float, float>>> FLOOR_SLOTS = {
		{ "CORRIDOR01", { { 0.0f, -100.0f }, { 0.0f, 0.0f }, { 0.0f, 100.0f } } },
		{ "CORRIDOR02", { { -20.0f, -200.0f }, { -20.0f, -100.0f }, { -20.0f, 0.0f }, { -20.0f, 100.0f }, { -20.0f, 200.0f } } },
		{ "CORRIDOR03", { { 0.0f, -100.0f }, { 0.0f, 0.0f }, { 0.0f, 100.0f } } },
		{ "CROSSING01", { { 0.0f, 0.0f }, { 0.0f, -100.0f }, { 100.0f, 0.0f } } },
		{ "CROSSING02", { { 0.0f, 0.0f }, { 0.0f, -100.0f }, { 100.0f, 0.0f } } },
		{ "CROSSING03", { { 0.0f, 0.0f }, { 0.0f, -100.0f }, { 100.0f, 0.0f } } },
		{ "ROOM02", { { 0.0f, 0.0f }, { -120.0f, -60.0f }, { 120.0f, -60.0f }, { -120.0f, 60.0f }, { 120.0f, 60.0f } } },
		{ "ROOM06", { { 0.0f, 0.0f }, { -140.0f, -120.0f }, { 140.0f, -120.0f }, { -140.0f, 120.0f }, { 140.0f, 120.0f }, { 0.0f, -200.0f }, { 0.0f, 200.0f } } },
		{ "ROOM05", { { 0.0f, 0.0f }, { -150.0f, -200.0f }, { 150.0f, -200.0f }, { -150.0f, 200.0f }, { 150.0f, 200.0f }, { 0.0f, -300.0f }, { 0.0f, 300.0f } } }
	};

	std::vector<PlacedPiece> placed;
	std::vector<Entity> entities;
	std::vector<Exit> open_exits;
	std::vector<Exit> failed_exits;
	int entity_id_idx = 100;

	float start_x = static_cast<float>(options.startX);
	float start_z = static_cast<float>(options.startZ);

	Bounds start_bounds = get_world_bounds(BOUNDING_BOXES_FULL["CORRIDOR01"], start_x, start_z, 0);
	placed.push_back({ "CORRIDOR01", start_x, 0.0f, start_z, 0, start_bounds });

	for (const auto &ext : OBJECTS["CORRIDOR01"].exits) {
		float rx, rz, rdx, rdz;
		rotate_pt(ext.x, ext.z, 0, rx, rz);
		rotate_pt(ext.outDir.dx, ext.outDir.dz, 0, rdx, rdz);
		open_exits.push_back({ start_x + rx, 0.0f + ext.y, start_z + rz, rdx, rdz, "CORRIDOR01", 0 });
	}

	auto check_collision = [&](float nx, float nz, const std::string &n_name, int n_rot, const Exit &attach_ex) {
		Bounds n_bounds = get_world_bounds(BOUNDING_BOXES_FULL[n_name], nx, nz, n_rot);
		size_t source_piece_idx = attach_ex.sourcePieceIndex;

		for (size_t p_idx = 0; p_idx < placed.size(); ++p_idx) {
			if (p_idx == source_piece_idx)
				continue;
			const auto &p_bounds = placed[p_idx].bounds;
			if (n_bounds.minX < p_bounds.maxX && n_bounds.maxX > p_bounds.minX &&
			    n_bounds.minZ < p_bounds.maxZ && n_bounds.maxZ > p_bounds.minZ) {
				return true;
			}
		}
		return false;
	};

	std::vector<std::string> objectKeys;
	for (const auto &kv : OBJECTS)
		objectKeys.push_back(kv.first);

	for (int i = 0; i < options.numObjectsToPlace; ++i) {
		if (open_exits.empty())
			break;

		int pref_idx = prefer_exit_index(open_exits, placed, 0.18f, rng);
		int exit_idx = (pref_idx >= 0) ? pref_idx : rng.randInt(0, static_cast<int>(open_exits.size()) - 1);

		Exit O = open_exits[exit_idx];
		open_exits.erase(open_exits.begin() + exit_idx);

		float wx = O.wx, wy = O.wy, wz = O.wz;
		float wdx = O.wdx, wdz = O.wdz;
		std::string source_name = O.sourceName;

		bool placed_new = false;
		std::vector<std::string> types = objectKeys;
		rng.shuffle(types);

		for (const auto &cand_name : types) {
			if (cand_name == "ROOM05" || cand_name == "ROOM06" || cand_name == "ROOM02") {
				if (rng.randomFloat() > 0.20f)
					continue;
			}

			if (cand_name == "ROOM05") {
				if (source_name.find("CORRIDOR") != 0)
					continue;
			}

			if (cand_name == "CROSSING01" || cand_name == "CROSSING02" || cand_name == "CROSSING03") {
				if (source_name == "ROOM05")
					continue;
				if (source_name.find("CORRIDOR") != 0)
					continue;
			}

			if (placed_new)
				break;

			const auto &cand_exits = OBJECTS[cand_name].exits;
			std::vector<size_t> indices(cand_exits.size());
			for (size_t idx = 0; idx < indices.size(); ++idx)
				indices[idx] = idx;
			rng.shuffle(indices);

			for (size_t ext_idx : indices) {
				if (placed_new)
					break;

				const auto &loc_ext = cand_exits[ext_idx];
				float lx = loc_ext.x, lz = loc_ext.z;
				float ldx = loc_ext.outDir.dx, ldz = loc_ext.outDir.dz;

				std::vector<int> rots = { 0, 90, 180, 270 };
				rng.shuffle(rots);

				for (int ang : rots) {
					float rdx, rdz;
					rotate_pt(ldx, ldz, ang, rdx, rdz);

					if (std::abs(rdx - (-wdx)) < 1e-4f && std::abs(rdz - (-wdz)) < 1e-4f) {
						float rx, rz;
						rotate_pt(lx, lz, ang, rx, rz);
						float Ox = wx - rx;
						float Oy = wy - loc_ext.y;
						float Oz = wz - rz;

						if (!check_collision(Ox, Oz, cand_name, ang, O)) {
							Bounds cand_bounds = get_world_bounds(BOUNDING_BOXES_FULL[cand_name], Ox, Oz, ang);
							placed.push_back({ cand_name, Ox, Oy, Oz, ang, cand_bounds });
							size_t new_piece_idx = placed.size() - 1;
							placed_new = true;

							if (cand_name == "CROSSING01" || cand_name == "CROSSING02" || cand_name == "CROSSING03") {
								for (const auto &c_ext : cand_exits) {
									if (rng.randomFloat() < 0.40f) {
										float clx = c_ext.x, clz = c_ext.z;
										Dir out_dir = c_ext.outDir;
										int base_door_rot = 0;
										if (std::abs(out_dir.dx - 0.0f) < 1e-4f && std::abs(out_dir.dz - (-1.0f)) < 1e-4f)
											base_door_rot = 0;
										else if (std::abs(out_dir.dx - 1.0f) < 1e-4f && std::abs(out_dir.dz - 0.0f) < 1e-4f)
											base_door_rot = 90;
										else if (std::abs(out_dir.dx - 0.0f) < 1e-4f && std::abs(out_dir.dz - 1.0f) < 1e-4f)
											base_door_rot = 180;
										else if (std::abs(out_dir.dx - (-1.0f)) < 1e-4f && std::abs(out_dir.dz - 0.0f) < 1e-4f)
											base_door_rot = 270;

										int door_rot = (base_door_rot + ang) % 360;
										float crx, crz;
										rotate_pt(clx, clz, ang, crx, crz);
										float door_wx = Ox + crx;
										float door_wy = Oy + c_ext.y + 75.0f;
										float door_wz = Oz + crz;

										entities.push_back({ "door55", "0", door_wx, door_wy, door_wz, door_rot, entity_id_idx++, -1 });
									}
								}
							}

							if (cand_name == "ROOM05" || cand_name == "CORRIDOR01" || cand_name == "ROOM06") {
								struct TorchPos {
									float x, z;
									int rot;
								};
								std::vector<TorchPos> all_torch_positions;
								int num_torches = 0;

								if (cand_name == "CORRIDOR01") {
									num_torches = rng.randInt(2, 4);
									all_torch_positions = {
										{ -200.0f, -155.0f, 270 }, { 0.0f, -155.0f, 270 }, { 200.0f, -155.0f, 270 }, { -200.0f, 155.0f, 90 }, { 0.0f, 155.0f, 90 }, { 200.0f, 155.0f, 90 }
									};
								} else if (cand_name == "ROOM05") {
									num_torches = rng.randInt(4, 8);
									all_torch_positions = {
										{ -334.0f - 25.0f, -431.0f, 180 }, { -334.0f - 25.0f, -231.0f, 180 }, { -334.0f - 25.0f, -31.0f, 180 }, { -334.0f - 25.0f, 169.0f, 180 }, { -334.0f - 25.0f, 369.0f, 180 }, { 415.0f, -631.0f, 0 }, { 415.0f, -231.0f, 0 }, { 415.0f, -31.0f, 0 }, { 415.0f, 169.0f, 0 }, { 415.0f, 369.0f, 0 }, { -174.0f, -791.0f + 180.0f, 270 }, { 26.0f, -791.0f + 180.0f, 270 }, { 226.0f, -791.0f + 180.0f, 270 }, { 426.0f, -791.0f + 180.0f, 270 }, { -174.0f, 655.0f + 40.0f, 90 }, { 26.0f, 655.0f + 40.0f, 90 }, { 226.0f, 655.0f + 40.0f, 90 }, { 426.0f, 655.0f + 40.0f, 90 }
									};
								} else if (cand_name == "ROOM06") {
									num_torches = rng.randInt(2, 4);
									all_torch_positions = {
										{ -254.0f + 125.0f, -311.0f + 200.0f, 180 }, { -254.0f + 60.0f, -311.0f + 390.0f, 270 }, { 251.0f - 125.0f, -311.0f + 200.0f, 0 }, { 251.0f - 60.0f, -311.0f + 390.0f, 270 }, { -254.0f + 200.0f, 296.0f - 20.0f, 90 }, { -254.0f + 400.0f, 296.0f - 20.0f, 90 }
									};
								}

								std::vector<TorchPos> torch_positions;
								if (num_torches > 0) {
									torch_positions = rng.sample(all_torch_positions, num_torches);
								}

								struct ColorRGB {
									float r, g, b;
								};
								std::vector<ColorRGB> base_colors = {
									{ 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f }, { 1.0f, 0.5f, 0.0f }, { 0.5f, 0.0f, 1.0f }, { 1.0f, 0.75f, 0.8f }
								};
								ColorRGB base_color = rng.choice(base_colors);
								float torch_r = round_1dp(base_color.r * rng.uniform(9.0f, 13.0f));
								float torch_g = round_1dp(base_color.g * rng.uniform(9.0f, 13.0f));
								float torch_b = round_1dp(base_color.b * rng.uniform(9.0f, 13.0f));

								for (const auto &tp : torch_positions) {
									float rx, rz;
									rotate_pt(tp.x, tp.z, ang, rx, rz);
									float world_x = Ox + rx;
									float world_z = Oz + rz;
									float torch_y = Oy + 100.0f;
									int torch_rot = (tp.rot + ang) % 360;

									entities.push_back({ "!monster1", "0", world_x, torch_y, world_z, torch_rot, entity_id_idx++, 2 });

									float lamp_y = Oy + 120.0f;
									entities.push_back({ "lamp_post", "", world_x, lamp_y, world_z, 0, entity_id_idx++, 0 });

									Entity flickerLight;
									flickerLight.type = "LIGHT_SOURCE";
									flickerLight.name = "flicker";
									flickerLight.x = world_x;
									flickerLight.y = lamp_y;
									flickerLight.z = world_z;
									flickerLight.rot = 0;
									flickerLight.id = entity_id_idx++;
									flickerLight.state = 0;
									flickerLight.hasColor = true;
									flickerLight.colorR = torch_r;
									flickerLight.colorG = torch_g;
									flickerLight.colorB = torch_b;
									flickerLight.hasDir = true;
									flickerLight.dirX = 0.0f;
									flickerLight.dirY = -1.0f;
									flickerLight.dirZ = 0.0f;
									entities.push_back(flickerLight);
								}
							}

							if (cand_name != "CORRIDOR02") {
								float light_y = Oy + 170.0f;
								entities.push_back({ "lamp_post", "", Ox, light_y, Oz, 0, entity_id_idx++, 0 });

								float center_r = round_1dp(rng.uniform(9.0f, 14.0f));
								float center_g = round_1dp(rng.uniform(9.0f, 14.0f));
								float center_b = round_1dp(rng.uniform(9.0f, 14.0f));

								Entity centerLight;
								centerLight.type = "LIGHT_SOURCE";
								centerLight.name = "flicker";
								centerLight.x = Ox;
								centerLight.y = light_y;
								centerLight.z = Oz;
								centerLight.rot = 0;
								centerLight.id = entity_id_idx;
								centerLight.state = 0;
								centerLight.hasColor = true;
								centerLight.colorR = center_r;
								centerLight.colorG = center_g;
								centerLight.colorB = center_b;
								centerLight.hasDir = true;
								centerLight.dirX = 0.0f;
								centerLight.dirY = -1.0f;
								centerLight.dirZ = 0.0f;
								entities.push_back(centerLight);

								entities.push_back({ "torch1", "0", Ox, Oy + 140.0f, Oz, 0, entity_id_idx++, 0 });

								if (rng.randomFloat() < 0.55f) {
									std::vector<float> heights = { 200.0f, 300.0f, 400.0f };
									float s_y = Oy + rng.choice(heights);
									float hue = rng.randomFloat();
									float sat = rng.uniform(0.4f, 1.0f);
									float val = rng.uniform(0.3f, 0.9f);
									float r, g, b;
									hsv_to_rgb(hue, sat, val, r, g, b);

									entities.push_back({ "lamp_post", "", Ox, s_y, Oz, 0, entity_id_idx++, 0 });

									float dx = rng.uniform(-0.4f, 0.4f);
									float dz = rng.uniform(-0.4f, 0.4f);
									float dy = -1.0f;
									float len = std::sqrt(dx * dx + dy * dy + dz * dz);
									dx /= len;
									dy /= len;
									dz /= len;

									Entity spotlight;
									spotlight.type = "LIGHT_SOURCE";
									spotlight.name = "Spotlight";
									spotlight.x = Ox;
									spotlight.y = s_y;
									spotlight.z = Oz;
									spotlight.rot = 0;
									spotlight.id = entity_id_idx++;
									spotlight.state = 0;
									spotlight.hasColor = true;
									spotlight.colorR = r;
									spotlight.colorG = g;
									spotlight.colorB = b;
									spotlight.hasDir = true;
									spotlight.dirX = dx;
									spotlight.dirY = dy;
									spotlight.dirZ = dz;
									entities.push_back(spotlight);
								}

								float depth = std::max(0.0f, -Oy);
								int level = std::min(3, static_cast<int>(depth / 280.0f));
								std::vector<int> drots = { 0, 45, 90, 135, 180, 225, 270, 315 };
								int d_rot = rng.choice(drots);

								int num_items = 1;
								if (cand_name.find("ROOM05") == 0)
									num_items = rng.randInt(4, 6);
								else if (cand_name.find("CROSSING") == 0)
									num_items = rng.randInt(2, 3);
								else
									num_items = rng.randInt(1, 3);

								for (int item_i = 0; item_i < num_items; ++item_i) {
									float r = rng.randomFloat();
									float local_x = 0.0f, local_z = 0.0f;

									if (FLOOR_SLOTS.count(cand_name)) {
										const auto &slots = FLOOR_SLOTS[cand_name];
										std::vector<std::pair<float, float>> avail;
										for (const auto &s : slots) {
											std::pair<int, int> key = { static_cast<int>(std::round(s.first)), static_cast<int>(std::round(s.second)) };
											if (placed.back().usedSlots.find(key) == placed.back().usedSlots.end()) {
												avail.push_back(s);
											}
										}
										if (!avail.empty()) {
											auto chosen_s = rng.choice(avail);
											local_x = chosen_s.first;
											local_z = chosen_s.second;
											placed.back().usedSlots.insert({ static_cast<int>(std::round(local_x)), static_cast<int>(std::round(local_z)) });
										}
									}

									float rx, rz;
									rotate_pt(local_x, local_z, ang, rx, rz);
									float wx_item = Ox + rx;
									float wz_item = Oz + rz;

									if (cand_name == "CORRIDOR02")
										continue;

									std::string ent_type;
									if (r < 0.10f) {
										std::vector<std::string> items = { "POTION", "cheese1", "GOBLET", "pot1", "pot2", "pot3", "mushroom" };
										ent_type = rng.choice(items);
										entities.push_back({ ent_type, "-1", wx_item, Oy + 40.0f, wz_item, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.25f) {
										std::vector<std::string> items = { "rock1", "rock2", "skull", "QUARTZ", "cdoorclosedchest", "block01", "block02" };
										ent_type = rng.choice(items);
										int st = 0;
										if (ent_type == "cdoorclosedchest") {
											st = (level < 2) ? rng.randInt(0, 1) : rng.randInt(0, 2);
										}
										float adjust = 10.0f;
										if (ent_type == "block01" || ent_type == "block02")
											adjust = -11.0f;
										entities.push_back({ ent_type, "0", wx_item, Oy + adjust, wz_item, d_rot, entity_id_idx++, st });
									} else if (r < 0.28f) {
										ent_type = "armour";
										entities.push_back({ ent_type, "-1", wx_item, Oy + 60.0f, wz_item, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.30f) {
										ent_type = "COIN";
										entities.push_back({ ent_type, "-1", wx_item, Oy + 60.0f, wz_item, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.35f) {
										ent_type = "SPELLBOOK";
										entities.push_back({ "spellbook", "-1", wx_item, Oy + 40.0f, wz_item, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.40f) {
										std::vector<std::string> scrolls = { "SCROLL-HEALING-", "SCROLL-MAGICMISSLE-", "SCROLL-FIREBALL-", "SCROLL-LIGHTNING-" };
										ent_type = rng.choice(scrolls);
										entities.push_back({ ent_type, "-1", wx_item, Oy + 40.0f, wz_item, d_rot, entity_id_idx++, 0 });
									} else if (r < 0.45f) {
										std::vector<std::string> weapon_types;
										if (level == 0)
											weapon_types = { "BASTARDSWORD", "FLAMESWORD", "BATTLEAXE" };
										else if (level == 1)
											weapon_types = { "ICESWORD", "LIGHTNINGSWORD", "MORNINGSTAR" };
										else
											weapon_types = { "SPLITSWORD", "SPIKEDFLAIL", "SUPERFLAMESWORD" };
										ent_type = rng.choice(weapon_types);
										entities.push_back({ ent_type, "-1", wx_item, Oy + 80.0f, wz_item, d_rot, entity_id_idx++, 0 });
									} else {
										std::vector<std::string> possible_mobs;
										if (level == 0)
											possible_mobs = { "GOBLIN", "TENTACLE" };
										else if (level == 1)
											possible_mobs = { "OGRE", "CORPSE", "MUMMY", "WOLF", "COBRA", "OGRO" };
										else if (level == 2)
											possible_mobs = { "NECROMANCER", "SORCERER", "WRAITH", "PHANTOM", "KNIGHT", "SLAVE" };
										else
											possible_mobs = { "FAERIE", "BAUUL", "DEMONESS", "DRAGON" };
										ent_type = rng.choice(possible_mobs);
										std::string name_val = to_lower_str(ent_type);
										entities.push_back({ ent_type, name_val, wx_item, Oy + 50.0f + (depth / 140.0f) * 2.0f, wz_item, d_rot, entity_id_idx++, 0 });
									}
								}
							}

							for (size_t other_i = 0; other_i < cand_exits.size(); ++other_i) {
								if (other_i == ext_idx)
									continue;
								const auto &other_ext = cand_exits[other_i];
								float opx, opz, odx, odz;
								rotate_pt(other_ext.x, other_ext.z, ang, opx, opz);
								rotate_pt(other_ext.outDir.dx, other_ext.outDir.dz, ang, odx, odz);
								open_exits.push_back({ Ox + opx, Oy + other_ext.y, Oz + opz, odx, odz, cand_name, new_piece_idx });
							}
						}
						break;
					}
				}
			}
		}

		if (!placed_new) {
			failed_exits.push_back(O);
		}
	}

	open_exits.insert(open_exits.end(), failed_exits.begin(), failed_exits.end());

	struct ExitKey {
		long long x, y, z;
		int dx, dz;
		bool operator<(const ExitKey &o) const {
			return std::tie(x, y, z, dx, dz) < std::tie(o.x, o.y, o.z, o.dx, o.dz);
		}
	};

	std::vector<ExitKey> all_piece_exits;
	for (const auto &p : placed) {
		for (const auto &ext : OBJECTS[p.name].exits) {
			float rx, rz, rdx, rdz;
			rotate_pt(ext.x, ext.z, p.rot, rx, rz);
			rotate_pt(ext.outDir.dx, ext.outDir.dz, p.rot, rdx, rdz);
			long long ey = static_cast<long long>(std::round(p.y + ext.y));
			long long ex = static_cast<long long>(std::round(p.x + rx));
			long long ez = static_cast<long long>(std::round(p.z + rz));
			all_piece_exits.push_back({ ex, ey, ez, static_cast<int>(std::round(rdx)), static_cast<int>(std::round(rdz)) });
		}
	}

	std::set<std::tuple<long long, long long, long long>> connected_positions;
	for (const auto &e1 : all_piece_exits) {
		for (const auto &e2 : all_piece_exits) {
			if (e1.x == e2.x && e1.y == e2.y && e1.z == e2.z && e1.dx == -e2.dx && e1.dz == -e2.dz) {
				connected_positions.insert(std::make_tuple(e1.x, e1.y, e1.z));
			}
		}
	}

	std::vector<Exit> dead_end_exits;
	for (const auto &o : open_exits) {
		auto key = std::make_tuple(static_cast<long long>(std::round(o.wx)),
		                           static_cast<long long>(std::round(o.wy)),
		                           static_cast<long long>(std::round(o.wz)));
		if (connected_positions.find(key) == connected_positions.end()) {
			dead_end_exits.push_back(o);
		}
	}

	for (const auto &open_ex : dead_end_exits) {
		float wx = open_ex.wx, wy = open_ex.wy, wz = open_ex.wz;
		float wdx = open_ex.wdx, wdz = open_ex.wdz;
		int ndx = static_cast<int>(std::round(-wdx));
		int ndz = static_cast<int>(std::round(-wdz));

		int wall_rot = 0;
		if (ndx == 0 && ndz == -1)
			wall_rot = 270;
		else if (ndx == 1 && ndz == 0)
			wall_rot = 0;
		else if (ndx == 0 && ndz == 1)
			wall_rot = 90;
		else if (ndx == -1 && ndz == 0)
			wall_rot = 180;

		wx += static_cast<float>(ndz) * 160.0f;
		wz += static_cast<float>(-ndx) * 160.0f;

		entities.push_back({ "wall", "corridor10", wx, wy, wz, wall_rot, entity_id_idx++, 0 });
	}

	if (!placed.empty()) {
		size_t deepest_idx = 0;
		float min_y = placed[0].y;
		for (size_t p = 1; p < placed.size(); ++p) {
			if (placed[p].y < min_y) {
				min_y = placed[p].y;
				deepest_idx = p;
			}
		}

		float tx = placed[deepest_idx].x;
		float ty = placed[deepest_idx].y;
		float tz = placed[deepest_idx].z;
		std::string deepest_name = placed[deepest_idx].name;
		int deepest_rot = placed[deepest_idx].rot;

		if (FLOOR_SLOTS.count(deepest_name)) {
			std::set<std::pair<int, int>> used_local_slots;
			for (const auto &e : entities) {
				float dx = e.x - tx;
				float dz = e.z - tz;
				float lx, lz;
				rotate_pt(dx, dz, (360 - deepest_rot) % 360, lx, lz);
				used_local_slots.insert({ static_cast<int>(std::round(lx)), static_cast<int>(std::round(lz)) });
			}

			std::vector<std::pair<float, float>> free_slots;
			for (const auto &slot : FLOOR_SLOTS[deepest_name]) {
				if (slot.first == 0.0f && slot.second == 0.0f)
					continue;
				std::pair<int, int> key = { static_cast<int>(std::round(slot.first)), static_cast<int>(std::round(slot.second)) };
				if (used_local_slots.find(key) == used_local_slots.end()) {
					free_slots.push_back(slot);
				}
			}

			if (!free_slots.empty()) {
				auto target_slot = rng.choice(free_slots);
				float rx, rz;
				rotate_pt(target_slot.first, target_slot.second, deepest_rot, rx, rz);
				float new_wx = tx + rx;
				float new_wz = tz + rz;

				for (auto &e : entities) {
					if (std::abs(e.x - tx) < 1.0f && std::abs(e.z - tz) < 1.0f &&
					    e.type != "circle" && e.type != "spiral" && e.type != "!flarenohit" &&
					    e.type != "lamp_post" && e.type != "LIGHT_SOURCE") {
						e.x = new_wx;
						e.z = new_wz;
					}
				}
			}
		}

		entities.push_back({ "circle", "0", tx, ty - 41.0f + 40.0f, tz, 0, entity_id_idx++, 2 });
		entities.push_back({ "spiral", "-1", tx, ty + 40.0f, tz, 0, entity_id_idx++, 2 });
		entities.push_back({ "!flarenohit", "flare@1", tx, ty + 40.0f, tz, 0, entity_id_idx++, 2 });
	}

	write_map_file(outputPath, options.startX, options.startZ, 90, placed, entities, true, true);
	return true;
}

} // namespace DungeonGen
