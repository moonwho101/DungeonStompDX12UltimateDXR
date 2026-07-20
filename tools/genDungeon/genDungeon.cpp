// dungeon_generator.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>
#include <tuple>

struct Vec2 {
	float x;
	float z;
	Vec2() : x(0), z(0) {
	}
	Vec2(float xx, float zz) : x(xx), z(zz) {
	}
};

struct ExitPoint {
	float wx, wy, wz;
	int wdx, wdz;
	std::string source_name;
};

struct Piece {
	std::string name;
	float x, y, z;
	int rot;
};

struct Entity {
	std::string type;
	std::string name;
	float x, y, z;
	int rot;
	int id;
	int state;
	bool hasColor;
	float r, g, b;

	Entity() : type(""), name(""), x(0), y(0), z(0), rot(0),
	           id(0), state(0), hasColor(false), r(1.0f), g(1.0f), b(1.0f) {
	}
};

static const Vec2 DIR_N(0, 1);
static const Vec2 DIR_S(0, -1);
static const Vec2 DIR_W(-1, 0);
static const Vec2 DIR_E(1, 0);

// OBJECTS equivalent
struct ExitDef {
	Vec2 pos;
	Vec2 out;
	float y; // optional; 0 if not used
	ExitDef(Vec2 p, Vec2 o, float yy = 0.0f) : pos(p), out(o), y(yy) {
	}
};

static std::map<std::string, std::vector<ExitDef>> OBJECTS = {
	{ "ROOM2", { ExitDef(Vec2(0, -160), DIR_S), ExitDef(Vec2(0, 160), DIR_N) } },
	{ "crossroads", { ExitDef(Vec2(80, 0), DIR_S), ExitDef(Vec2(80, 240), DIR_N), ExitDef(Vec2(-40, 120), DIR_W), ExitDef(Vec2(200, 120), DIR_E) } },
	{ "t_junction", { ExitDef(Vec2(80, 0), DIR_S), ExitDef(Vec2(-40, 120), DIR_W), ExitDef(Vec2(200, 120), DIR_E) } },
	{ "left_corner", { ExitDef(Vec2(80, 0), DIR_S), ExitDef(Vec2(-40, 120), DIR_W) } },
	{ "ROOM_SQUARE", { ExitDef(Vec2(240, 0), DIR_E) } },
	{ "ROOMEDIUM", { ExitDef(Vec2(-160, 0), DIR_W), ExitDef(Vec2(160, 0), DIR_E) } },
	{ "slope_stairs", { ExitDef(Vec2(0, -160), DIR_S, 80.0f), ExitDef(Vec2(0, 160), DIR_N, -60.0f) } },
	{ "right_curve", { ExitDef(Vec2(-140, -140), DIR_S), ExitDef(Vec2(140, 140), DIR_E) } }
};

// BOUNDING_BOXES equivalent
struct Bounds {
	float minx, minz, maxx, maxz;
	Bounds() : minx(0), minz(0), maxx(0), maxz(0) {
	}
	Bounds(float a, float b, float c, float d) : minx(a), minz(b), maxx(c), maxz(d) {
	}
};

static std::map<std::string, Bounds> BOUNDING_BOXES = {
	{ "ROOM2", Bounds(-78, -158, 78, 158) },
	{ "crossroads", Bounds(-38, 2, 198, 238) },
	{ "t_junction", Bounds(-38, 2, 198, 238) },
	{ "left_corner", Bounds(-38, 2, 158, 198) },
	{ "ROOM_SQUARE", Bounds(-238, -238, 238, 238) },
	{ "ROOMEDIUM", Bounds(-158, -238, 158, 238) },
	{ "slope_stairs", Bounds(-78, -158, 78, 158) },
	{ "right_curve", Bounds(-218, -138, 138, 218) }
};

Vec2 rotate(float x, float z, int angle) {
	switch (angle) {
	case 0:
		return Vec2(x, z);
	case 90:
		return Vec2(-z, x);
	case 180:
		return Vec2(-x, -z);
	case 270:
		return Vec2(z, -x);
	default:
		return Vec2(x, z);
	}
}

Vec2 rotate_dir(float dx, float dz, int angle) {
	return rotate(dx, dz, angle);
}

std::tuple<float, float, float, float> get_world_bounds(const std::string &name, float ox, float oz, int rot) {
	Bounds b = BOUNDING_BOXES[name];
	std::vector<Vec2> corners = {
		Vec2(b.minx, b.minz),
		Vec2(b.maxx, b.minz),
		Vec2(b.minx, b.maxz),
		Vec2(b.maxx, b.maxz)
	};
	std::vector<Vec2> rotated;
	rotated.reserve(corners.size());
	for (auto &c : corners) {
		rotated.push_back(rotate(c.x, c.z, rot));
	}
	float min_x = rotated[0].x + ox;
	float max_x = rotated[0].x + ox;
	float min_z = rotated[0].z + oz;
	float max_z = rotated[0].z + oz;
	for (auto &c : rotated) {
		float wx = c.x + ox;
		float wz = c.z + oz;
		if (wx < min_x)
			min_x = wx;
		if (wx > max_x)
			max_x = wx;
		if (wz < min_z)
			min_z = wz;
		if (wz > max_z)
			max_z = wz;
	}
	return std::make_tuple(min_x, min_z, max_x, max_z);
}

bool check_collision(const std::vector<Piece> &placed, float nx, float nz, const std::string &n_name, int n_rot) {
	auto [n_minx, n_minz, n_maxx, n_maxz] = get_world_bounds(n_name, nx, nz, n_rot);
	for (const auto &p : placed) {
		auto [p_minx, p_minz, p_maxx, p_maxz] = get_world_bounds(p.name, p.x, p.z, p.rot);
		if (n_minx < p_maxx && n_maxx > p_minx &&
		    n_minz < p_maxz && n_maxz > p_minz) {
			return true;
		}
	}
	return false;
}

template <typename T>
void shuffle_vec(std::vector<T> &v, std::mt19937 &rng) {
	std::shuffle(v.begin(), v.end(), rng);
}

void generate(float start_x = 5200.0f, float start_z = 2600.0f, int seed = 0, int num_objects_to_place = 350) {
	std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
	std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

	std::cout << "Starting dungeon generation...\n";

	std::vector<Piece> placed;
	std::vector<Entity> entities;
	std::vector<ExitPoint> open_exits;
	std::vector<ExitPoint> failed_exits;
	int entity_id_idx = 100;

	std::cout << "Placing initial starting ROOM2...\n";
	placed.push_back(Piece{ "ROOM2", start_x, 0.0f, start_z, 0 });

	// initial exits
	for (const auto &ext : OBJECTS["ROOM2"]) {
		Vec2 rp = rotate(ext.pos.x, ext.pos.z, 0);
		Vec2 rd = rotate_dir(ext.out.x, ext.out.z, 0);
		ExitPoint e;
		e.wx = start_x + rp.x;
		e.wy = 0.0f + ext.y;
		e.wz = start_z + rp.z;
		e.wdx = (int)rd.x;
		e.wdz = (int)rd.z;
		e.source_name = "ROOM2";
		open_exits.push_back(e);
	}

	std::vector<std::string> corridor_like = { "left_corner", "right_curve", "t_junction", "crossroads" };

	for (int iter = 0; iter < num_objects_to_place; ++iter) {
		if (open_exits.empty())
			break;

		int exit_idx = (int)(rng() % open_exits.size());
		ExitPoint O = open_exits[exit_idx];
		open_exits.erase(open_exits.begin() + exit_idx);

		float wx = O.wx, wy = O.wy, wz = O.wz;
		int wdx = O.wdx, wdz = O.wdz;
		std::string source_name = O.source_name;

		bool placed_new = false;

		std::vector<std::string> types;
		if (source_name != "ROOM2") {
			types.push_back("ROOM2");
		} else {
			for (auto &kv : OBJECTS) {
				types.push_back(kv.first);
			}
		}
		shuffle_vec(types, rng);

		for (const auto &cand_name : types) {
			if (placed_new)
				break;
			const auto &cand_exits = OBJECTS[cand_name];

			std::vector<int> indices(cand_exits.size());
			for (int i = 0; i < (int)cand_exits.size(); ++i)
				indices[i] = i;
			shuffle_vec(indices, rng);

			for (int ext_idx : indices) {
				if (placed_new)
					break;
				const auto &loc_ext = cand_exits[ext_idx];
				float lx = loc_ext.pos.x;
				float lz = loc_ext.pos.z;
				float ldx = loc_ext.out.x;
				float ldz = loc_ext.out.z;

				std::vector<int> rots = { 0, 90, 180, 270 };
				shuffle_vec(rots, rng);

				for (int ang : rots) {
					Vec2 rdir = rotate_dir(ldx, ldz, ang);
					if ((int)rdir.x == -wdx && (int)rdir.z == -wdz) {
						Vec2 rp = rotate(lx, lz, ang);
						float Ox = wx - rp.x;
						float Oy = wy - loc_ext.y;
						float Oz = wz - rp.z;

						if (!check_collision(placed, Ox, Oz, cand_name, ang)) {
							placed.push_back(Piece{ cand_name, Ox, Oy, Oz, ang });
							placed_new = true;
							std::cout << "Placed " << cand_name << " at (" << Ox << ", " << Oy << ", " << Oz << ") [Rot: " << ang << "]\n";

							// right_curve special spawner
							if (cand_name == "right_curve") {
								std::vector<std::tuple<float, float, int>> rc_pieces = {
									{ -220.00f, -140.00f, 0 },
									{ -207.73f, -46.83f, 345 },
									{ -171.77f, 39.98f, 330 },
									{ -114.56f, 114.54f, 315 },
									{ -40.00f, 171.74f, 300 },
									{ 46.82f, 207.70f, 285 }
								};
								for (auto &t : rc_pieces) {
									float dx, dz;
									int dr;
									std::tie(dx, dz, dr) = t;
									Vec2 r = rotate(dx, dz, ang);
									int tr = (dr + ang) % 360;
									Entity e;
									e.type = "right_curve_road";
									e.name = "";
									e.x = Ox + r.x;
									e.y = Oy;
									e.z = Oz + r.z;
									e.rot = tr;
									e.id = 0;
									e.state = 0;
									entities.push_back(e);
								}
								std::cout << "  -> Spawned right_curve_road segments\n";
							}

							// Spotlights and lamp posts
							if (dist01(rng) < 0.25f) {
								float s_y;
								{
									std::vector<float> choices = { 200.0f, 300.0f, 400.0f };
									s_y = Oy + choices[rng() % choices.size()];
								}

								float hue = dist01(rng);
								float sat = 0.4f + dist01(rng) * (1.0f - 0.4f);
								float val = 0.3f + dist01(rng) * (0.9f - 0.3f);

								// simple HSV->RGB
								float r, g, b;
								{
									float c = val * sat;
									float hprime = hue * 6.0f;
									float x = c * (1.0f - std::fabs(std::fmod(hprime, 2.0f) - 1.0f));
									float m = val - c;
									if (0 <= hprime && hprime < 1) {
										r = c;
										g = x;
										b = 0;
									} else if (1 <= hprime && hprime < 2) {
										r = x;
										g = c;
										b = 0;
									} else if (2 <= hprime && hprime < 3) {
										r = 0;
										g = c;
										b = x;
									} else if (3 <= hprime && hprime < 4) {
										r = 0;
										g = x;
										b = c;
									} else if (4 <= hprime && hprime < 5) {
										r = x;
										g = 0;
										b = c;
									} else {
										r = c;
										g = 0;
										b = x;
									}
									r += m;
									g += m;
									b += m;
								}

								Entity lamp;
								lamp.type = "lamp_post";
								lamp.x = Ox;
								lamp.y = s_y;
								lamp.z = Oz;
								lamp.rot = 0;
								lamp.id = entity_id_idx;
								lamp.state = 0;
								lamp.name = "";
								entities.push_back(lamp);

								Entity light;
								light.type = "LIGHT_SOURCE";
								light.name = "Spotlight";
								light.x = Ox;
								light.y = s_y;
								light.z = Oz;
								light.rot = 0;
								light.id = entity_id_idx;
								light.state = 0;
								light.hasColor = true;
								light.r = r;
								light.g = g;
								light.b = b;
								entities.push_back(light);

								std::cout << "  -> Spawned Spotlight overhead\n";
							}

							// Torch light in ROOM2
							if (cand_name == "ROOM2") {
								if (dist01(rng) < 0.4f) {
									bool is_left = dist01(rng) < 0.5f;
									float z_offset = -100.0f + dist01(rng) * 200.0f;
									float tlx, tlz, tfx, tfz;
									int trot;
									if (is_left) {
										tlx = -80;
										tlz = z_offset;
										tfx = -60;
										tfz = z_offset;
										trot = 270;
									} else {
										tlx = 80;
										tlz = z_offset;
										tfx = 60;
										tfz = z_offset;
										trot = 90;
									}
									Vec2 wlp = rotate(tlx, tlz, ang);
									Vec2 wfp = rotate(tfx, tfz, ang);
									int t_rot = (trot + ang) % 360;

									Entity torch;
									torch.type = "torch";
									torch.x = Ox + wlp.x;
									torch.y = Oy + 60.0f;
									torch.z = Oz + wlp.z;
									torch.rot = t_rot;
									torch.id = entity_id_idx;
									torch.state = 0;
									torch.name = "";
									entities.push_back(torch);

									Entity flames;
									flames.type = "!flamesnohit";
									flames.x = Ox + wfp.x;
									flames.y = Oy + 60.0f;
									flames.z = Oz + wfp.z;
									flames.rot = 0;
									flames.id = entity_id_idx;
									flames.state = 4;
									flames.name = "flame@1";
									entities.push_back(flames);

									Entity lamp;
									lamp.type = "lamp_post";
									lamp.x = Ox + wfp.x;
									lamp.y = Oy + 80.0f;
									lamp.z = Oz + wfp.z;
									lamp.rot = 0;
									lamp.id = entity_id_idx;
									lamp.state = 0;
									lamp.name = "";
									entities.push_back(lamp);

									Entity flicker;
									flicker.type = "LIGHT_SOURCE";
									flicker.x = Ox + wfp.x;
									flicker.y = Oy + 0.0f;
									flicker.z = Oz + wfp.z;
									flicker.rot = 0;
									flicker.id = entity_id_idx;
									flicker.state = 0;
									flicker.name = "flicker";
									entities.push_back(flicker);

									std::cout << "  -> Spawned torch light in ROOM2\n";
								}
							}

							// Door at any opening
							if (dist01(rng) < 0.25f) {
								int doorNum = 1 + (int)(rng() % 21);
								std::string door_type = "door" + std::to_string(doorNum);
								int d_rot;
								if (wdx == 0 && wdz == 1)
									d_rot = 0;
								else if (wdx == -1 && wdz == 0)
									d_rot = 90;
								else if (wdx == 0 && wdz == -1)
									d_rot = 180;
								else
									d_rot = 270;

								float dlx = -40.0f, dlz = 0.0f;
								Vec2 dw = rotate(dlx, dlz, d_rot);

								Entity frame;
								frame.type = "dframe";
								frame.x = wx;
								frame.y = wy;
								frame.z = wz;
								frame.rot = d_rot;
								frame.id = entity_id_idx;
								frame.state = 0;
								frame.name = "";
								entities.push_back(frame);

								Entity door;
								door.type = door_type;
								door.x = wx + dw.x;
								door.y = wy;
								door.z = wz + dw.z;
								door.rot = d_rot;
								door.id = entity_id_idx;
								door.state = 0;
								door.name = "";
								entities.push_back(door);

								std::cout << "  -> Spawned door (" << door_type << ") at " << cand_name << " entrance\n";
							}

							// Dungeon dressings
							if (cand_name == "ROOM_SQUARE" || cand_name == "ROOMEDIUM") {
								int num_dressings = 1 + (int)(rng() % 2);
								for (int di = 0; di < num_dressings; ++di) {
									std::vector<std::string> dressing_types = { "TABLE", "stool", "BED", "TROUGH", "LOGS" };
									std::string dressing_type = dressing_types[rng() % dressing_types.size()];
									std::vector<int> rots = { 0, 90, 180, 270 };
									int d_rot = rots[rng() % rots.size()];

									Bounds bb = BOUNDING_BOXES[cand_name];
									float margin = 40.0f;
									std::uniform_real_distribution<float> dx(bb.minx + margin, bb.maxx - margin);
									std::uniform_real_distribution<float> dz(bb.minz + margin, bb.maxz - margin);
									float lx = dx(rng);
									float lz = dz(rng);

									Vec2 rp = rotate(lx, lz, ang);
									float dxw = Ox + rp.x;
									float dzw = Oz + rp.z;

									Entity e;
									e.type = dressing_type;
									e.name = "0";
									e.x = dxw;
									e.y = Oy - 25.0f;
									e.z = dzw;
									e.rot = d_rot;
									e.id = entity_id_idx++;
									e.state = 0;
									entities.push_back(e);
								}
								std::cout << "  -> Spawned " << num_dressings << " dungeon dressings\n";
							}

							// Monsters / loot
							if (cand_name == "ROOM2" || cand_name == "ROOM_SQUARE" ||
							    cand_name == "ROOMEDIUM" || cand_name == "slope_stairs") {
								float r = dist01(rng);
								std::string ent_type;
								float depth = std::fabs(Oy);

								Entity e;
								e.x = Ox;
								e.z = Oz;
								e.id = entity_id_idx;
								e.state = 0;
								e.rot = 0;
								e.name = "-1";

								if (r < 0.12f) {
									std::vector<std::string> loot = { "POTION", "cheese1" };
									ent_type = loot[rng() % loot.size()];
									e.type = ent_type;
									e.y = Oy - 22.0f;
									entities.push_back(e);
								} else if (r < 0.22f) {
									ent_type = "COIN";
									e.type = "COIN";
									e.y = Oy - 22.0f;
									entities.push_back(e);
								} else if (r < 0.26f) {
									ent_type = "SPELLBOOK";
									e.type = "spellbook";
									e.y = Oy - 22.0f;
									entities.push_back(e);
								} else if (r < 0.30f) {
									std::vector<std::string> scrolls = {
										"SCROLL-HEALING-",
										"SCROLL-MAGICMISSLE-",
										"SCROLL-FIREBALL-",
										"SCROLL-LIGHTNING-"
									};
									ent_type = scrolls[rng() % scrolls.size()];
									e.type = ent_type;
									e.y = Oy - 22.0f;
									entities.push_back(e);
								} else if (r < 0.36f) {
									float top_level = 140.0f * 1;
									float mid_level = 140.0f * 2;
									std::vector<std::string> weapon_types;
									if (depth < top_level) {
										weapon_types = { "BASTARDSWORD", "FLAMESWORD", "BATTLEAXE" };
									} else if (depth < mid_level) {
										weapon_types = { "ICESWORD", "LIGHTNINGSWORD", "MORNINGSTAR" };
									} else {
										weapon_types = { "SPLITSWORD", "SPIKEDFLAIL", "SUPERFLAMESWORD" };
									}
									ent_type = weapon_types[rng() % weapon_types.size()];
									e.type = ent_type;
									e.y = Oy + 22.0f;
									entities.push_back(e);
								} else if (r < 0.44f) {
									ent_type = "CHEST";
									std::vector<std::string> chests = {
										"cdoorclosedwoodbox",
										"cdoorclosedbarrel",
										"cdoorclosedmetalbox"
									};
									e.type = chests[rng() % chests.size()];
									e.name = "0";
									e.y = Oy - 22.0f;
									e.rot = ang;
									entities.push_back(e);
								} else if (r < 0.74f) {
									float top_level = 140.0f * 1;
									float mid_level = 140.0f * 2;
									std::vector<std::string> mobs;
									if (depth < top_level) {
										mobs = { "GOBLIN", "TENTACLE" };
									} else if (depth < mid_level) {
										mobs = { "GOBLIN", "OGRE", "CORPSE", "MUMMY" };
									} else {
										mobs = { "OGRE", "MUMMY", "WRAITH", "PHANTOM" };
									}
									ent_type = mobs[rng() % mobs.size()];
									e.type = ent_type;
									e.name = ent_type;
									e.y = Oy + 10.0f + (depth / 140.0f) * 2.0f;
									entities.push_back(e);
								}

								if (!ent_type.empty()) {
									std::cout << "  -> Spawned " << ent_type << "\n";
									entity_id_idx++;
								}
							}

							// propagate exits
							for (int i = 0; i < (int)cand_exits.size(); ++i) {
								if (i == ext_idx)
									continue;
								const auto &other_ext = cand_exits[i];
								Vec2 op = rotate(other_ext.pos.x, other_ext.pos.z, ang);
								Vec2 od = rotate_dir(other_ext.out.x, other_ext.out.z, ang);
								ExitPoint new_exit;
								new_exit.wx = Ox + op.x;
								new_exit.wy = Oy + other_ext.y;
								new_exit.wz = Oz + op.z;
								new_exit.wdx = (int)od.x;
								new_exit.wdz = (int)od.z;
								new_exit.source_name = cand_name;
								open_exits.push_back(new_exit);
							}

						} // collision
						break;
					} // rots
				} // indices
			} // cand_exits
		} // types

		if (!placed_new) {
			failed_exits.push_back(O);
		}
	}

	// add failed exits back
	for (auto &e : failed_exits)
		open_exits.push_back(e);

	// compute all piece exits
	std::vector<std::tuple<int, int, int, int, int>> all_piece_exits;
	for (const auto &p : placed) {
		const auto &exs = OBJECTS[p.name];
		for (const auto &ext : exs) {
			Vec2 rp = rotate(ext.pos.x, ext.pos.z, p.rot);
			Vec2 rd = rotate_dir(ext.out.x, ext.out.z, p.rot);
			int ey = (int)std::round(p.y + ext.y);
			int ex = (int)std::round(p.x + rp.x);
			int ez = (int)std::round(p.z + rp.z);
			all_piece_exits.push_back(std::make_tuple(ex, ey, ez, (int)rd.x, (int)rd.z));
		}
	}

	std::vector<std::tuple<int, int, int>> connected_positions;
	for (auto &a : all_piece_exits) {
		int ex, ey, ez, edx, edz;
		std::tie(ex, ey, ez, edx, edz) = a;
		for (auto &b : all_piece_exits) {
			int ex2, ey2, ez2, edx2, edz2;
			std::tie(ex2, ey2, ez2, edx2, edz2) = b;
			if (ex == ex2 && ey == ey2 && ez == ez2 &&
			    edx == -edx2 && edz == -edz2) {
				connected_positions.push_back(std::make_tuple(ex, ey, ez));
			}
		}
	}

	auto is_connected = [&](const ExitPoint &o) {
		int ex = (int)std::round(o.wx);
		int ey = (int)std::round(o.wy);
		int ez = (int)std::round(o.wz);
		for (auto &cp : connected_positions) {
			int cx, cy, cz;
			std::tie(cx, cy, cz) = cp;
			if (cx == ex && cy == ey && cz == ez)
				return true;
		}
		return false;
	};

	std::vector<ExitPoint> dead_end_exits;
	for (auto &o : open_exits) {
		if (!is_connected(o)) {
			dead_end_exits.push_back(o);
		}
	}
	int skipped = (int)open_exits.size() - (int)dead_end_exits.size();
	if (skipped) {
		std::cout << "  Skipping " << skipped << " exits that are already connected to placed pieces.\n";
	}

	std::cout << "Generation loop completed. " << placed.size()
	          << " tiles placed. Sealing " << dead_end_exits.size()
	          << " open exits with dead-end walls...\n";

	for (auto &open_ex : dead_end_exits) {
		float wx = open_ex.wx;
		float wy = open_ex.wy;
		float wz = open_ex.wz;
		int wdx = open_ex.wdx;
		int wdz = open_ex.wdz;
		int ndx = -wdx;
		int ndz = -wdz;
		int wall_rot = 0;
		if (ndx == 0 && ndz == -1)
			wall_rot = 270;
		else if (ndx == 1 && ndz == 0)
			wall_rot = 0;
		else if (ndx == 0 && ndz == 1)
			wall_rot = 90;
		else if (ndx == -1 && ndz == 0)
			wall_rot = 180;
		wx += ndz * 80;
		wz += -ndx * 80;

		Entity wall;
		wall.type = "wall";
		wall.name = "cobblestone4";
		wall.x = wx;
		wall.y = wy;
		wall.z = wz;
		wall.rot = wall_rot;
		wall.id = entity_id_idx++;
		wall.state = 0;
		entities.push_back(wall);
	}

	// teleport exit at deepest piece
	if (!placed.empty()) {
		auto deepest_it = std::max_element(
		    placed.begin(), placed.end(),
		    [](const Piece &a, const Piece &b) {
			    return std::fabs(a.y) < std::fabs(b.y);
		    });
		float tx = deepest_it->x;
		float ty = deepest_it->y;
		float tz = deepest_it->z;

		Entity circle;
		circle.type = "circle";
		circle.x = tx;
		circle.y = ty - 41.0f;
		circle.z = tz;
		circle.rot = 0;
		circle.name = "0";
		circle.id = entity_id_idx++;
		circle.state = 2;
		entities.push_back(circle);

		Entity spiral;
		spiral.type = "spiral";
		spiral.x = tx;
		spiral.y = ty;
		spiral.z = tz;
		spiral.rot = 0;
		spiral.name = "-1";
		spiral.id = entity_id_idx++;
		spiral.state = 3;
		entities.push_back(spiral);

		Entity flare;
		flare.type = "!flarenohit";
		flare.x = tx;
		flare.y = ty;
		flare.z = tz;
		flare.rot = 0;
		flare.name = "flare@1";
		flare.id = entity_id_idx++;
		flare.state = 2;
		entities.push_back(flare);

		std::cout << "  -> Teleport exit added at deepest dungeon location ("
		          << tx << ", " << ty << ", " << tz << ")\n";
	}

	// write file
	std::string out_file = "level1.map";
	std::ofstream f(out_file);
	if (!f) {
		std::cerr << "Failed to open " << out_file << " for writing.\n";
		return;
	}

	f << "OBJECT startpos\n";
	f << "CO_ORDINATES " << start_x << " 0.000000 " << start_z << "\n";
	f << "ROT_ANGLE 0\n";

	for (const auto &p : placed) {
		if (p.name == "left_curve" || p.name == "right_curve")
			continue;
		f << "OBJECT " << p.name << "\n";
		f << "CO_ORDINATES " << p.x << " " << p.y << " " << p.z << "\n";
		f << "ROT_ANGLE " << p.rot << "\n";
	}

	std::uniform_real_distribution<float> ddir(-0.4f, 0.4f);

	for (const auto &e : entities) {
		const std::string &t = e.type;
		if (t == "wall") {
			f << "OBJECT !wall0-240-160\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " 0 " << e.name << " " << e.id << " 0\n";
		} else if (t == "torch") {
			f << "OBJECT torch\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
		} else if (t == "!flamesnohit") {
			f << "OBJECT !flamesnohit\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " 0 " << e.name << " " << e.state << " 0\n";
		} else if (t == "lamp_post") {
			f << "OBJECT lamp_post\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
		} else if (t == "LIGHT_SOURCE") {
			float dx = ddir(rng);
			float dz = ddir(rng);
			float dy = -1.0f;
			float length = std::sqrt(dx * dx + dy * dy + dz * dz);
			dx /= length;
			dy /= length;
			dz /= length;

			float lr = e.hasColor ? e.r : 1.0f;
			float lg = e.hasColor ? e.g : 1.0f;
			float lb = e.hasColor ? e.b : 1.0f;

			f << "LIGHT_SOURCE " << e.name << " POS "
			  << e.x << " " << e.y << " " << e.z
			  << " DIR " << dx << " " << dy << " " << dz << " "
			  << "COLOUR " << lr << " " << lg << " " << lb << "\n";
		} else if (t == "dframe") {
			f << "OBJECT dframe\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
		} else if (t.rfind("door", 0) == 0) {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " " << e.state << "\n";
		} else if (t == "slope_stairs") {
			f << "OBJECT slope_stairs\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
		} else if (t == "left_curve_road" || t == "right_curve_road") {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << "\n";
		} else if (t == "!flarenohit") {
			f << "OBJECT " << t << "\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " 3 " << e.name << " " << e.id << " " << e.state << "\n";
		} else {
			f << "OBJECT !monster1\n";
			f << "CO_ORDINATES " << e.x << " " << e.y << " " << e.z << "\n";
			f << "ROT_ANGLE " << e.rot << " " << t << " " << e.name << " " << e.id << " " << e.state << "\n";
		}
	}

	f << "END_FILE\n";
	f.close();

	int num_walls = 0;
	for (const auto &e : entities) {
		if (e.type == "wall")
			num_walls++;
	}
	int num_mobs = (int)entities.size() - num_walls;

	std::cout << "Successfully saved to " << out_file << "! ("
	          << placed.size() << " tiles, "
	          << num_mobs << " entities, "
	          << num_walls << " walls)\n";
}

int main() {
	generate();
	return 0;
}
