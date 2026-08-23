#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <vector>
#include <math.h>
#include "world.hpp"
#include "ImportMD2.hpp"

PLAYERMODELDATA *pmdata;
MODELLIST *model_list;
GUNLIST *your_gun;

BOOL ImportMD2_GLCMD(char *filename, int texture_alias, int pmodel_id, float scale) {
	FILE *fp;
	MD2HEADER header;
	MD2VERTEX bverts;
	float bscale[3];
	float translate[3];
	char name[16];
	int i, j;
	int frame_num;
	int cnt;
	int N;
	int id = 1;
	int glnum_verts;
	int num_glverts_per_command;
	float f;
	char buf[256], buffer[256], buffer2[256];
	GLVERT glv[2000];
	int glc[2000];
	int glv_cnt;
	int glc_cnt;

	// fp = fopen(filename,"rb");

	if (fopen_s(&fp, filename, "rb") != 0) {
		// MessageBox(hwnd, "Can't open md2", NULL, MB_OK);
		// return FALSE;
	}

	// read file header into MD2HEADER structure
	fread(&header, sizeof(MD2HEADER), 1, fp);

	// the glcmd format:
	// a positive integer starts a tristrip command, followed by that many
	// vertex structures.
	// a negative integer starts a trifan command, followed by -x vertexes
	// a zero indicates the end of the command list.
	// a vertex consists of a floating point s, a floating point t,
	// and an integer vertex index.

	fseek(fp, (UINT)header.offset_glcmds, SEEK_SET);

	if (header.offset_end == header.offset_glcmds) {
		// MessageBox(hwnd, "NO GL Commands in this md2", NULL, MB_OK);
		// PrintMessage(NULL, "ERROR : NO GL Commands in this md2", NULL, LOGFILE_ONLY);
		// return FALSE;
	}

	glv_cnt = 0;
	glc_cnt = 0;

	while (id != 0) {
		fread(&id, sizeof(int), 1, fp);

		if (id != 0) {
			glc[glc_cnt] = id;
			glc_cnt++;

			num_glverts_per_command = abs(id);

			for (j = 0; j < num_glverts_per_command; j++) {
				fread(&f, sizeof(float), 1, fp);
				glv[glv_cnt].s = f;
				fread(&f, sizeof(float), 1, fp);
				glv[glv_cnt].t = f;
				fread(&N, sizeof(int), 1, fp);
				glv[glv_cnt].index = N;

				glv_cnt++;
			}
		}
	}

	strcpy_s(buffer, "GL commands = ");
	_itoa_s(glc_cnt, buf, _countof(buf), 10);
	strcat_s(buffer, buf);

	strcpy_s(buffer2, "    GL face indices = ");
	_itoa_s(glv_cnt, buf, _countof(buf), 10);
	strcat_s(buffer2, buf);

	// PrintMessage(NULL, buffer, buffer2, LOGFILE_ONLY);

	_itoa_s(header.num_verts, buffer, _countof(buffer), 10);

	// PrintMessage(NULL, "verts = ", buffer, LOGFILE_ONLY);

	// Calculate total vertices when converted to triangle lists
	int total_tri_verts = 0;
	for (i = 0; i < glc_cnt; i++) {
		int glverts = abs(glc[i]);
		if (glverts >= 3) {
			total_tri_verts += (glverts - 2) * 3;
		}
	}

	pmdata[pmodel_id].f = new int[total_tri_verts];
	pmdata[pmodel_id].num_vert = new int[glc_cnt];
	pmdata[pmodel_id].poly_cmd = new D3DPRIMITIVETYPE[glc_cnt];
	pmdata[pmodel_id].texture_list = new int[glc_cnt];
	pmdata[pmodel_id].t = new VERT[total_tri_verts];

	// Temporary arrays to hold face corner data before UV seam splitting
	std::vector<int> temp_orig_f(total_tri_verts);
	std::vector<VERT> temp_t(total_tri_verts);

	cnt = 0;
	int glv_offset = 0;

	for (i = 0; i < glc_cnt; i++) {
		if (glc[i] == 0) {
			return FALSE;
		}

		pmdata[pmodel_id].poly_cmd[i] = D3DPT_TRIANGLELIST;
		pmdata[pmodel_id].texture_list[i] = texture_alias;

		glnum_verts = abs(glc[i]);

		if (glc[i] > 0) {
			// Triangle Strip decomposition into Triangle List
			int tri_cnt = 0;
			for (j = 0; j < glnum_verts - 2; j++) {
				int idx0, idx1, idx2;
				if (j % 2 == 0) {
					idx0 = glv_offset + j;
					idx1 = glv_offset + j + 1;
					idx2 = glv_offset + j + 2;
				} else {
					idx0 = glv_offset + j + 2;
					idx1 = glv_offset + j + 1;
					idx2 = glv_offset + j;
				}

				temp_orig_f[cnt] = glv[idx0].index;
				temp_t[cnt].x = glv[idx0].s * header.skinwidth;
				temp_t[cnt].y = glv[idx0].t * header.skinheight;
				cnt++;

				temp_orig_f[cnt] = glv[idx1].index;
				temp_t[cnt].x = glv[idx1].s * header.skinwidth;
				temp_t[cnt].y = glv[idx1].t * header.skinheight;
				cnt++;

				temp_orig_f[cnt] = glv[idx2].index;
				temp_t[cnt].x = glv[idx2].s * header.skinwidth;
				temp_t[cnt].y = glv[idx2].t * header.skinheight;
				cnt++;

				tri_cnt += 3;
			}
			pmdata[pmodel_id].num_vert[i] = tri_cnt;
		} else {
			// Triangle Fan decomposition into Triangle List
			int tri_cnt = 0;
			for (j = 0; j < glnum_verts - 2; j++) {
				int idx0 = glv_offset;
				int idx1 = glv_offset + j + 1;
				int idx2 = glv_offset + j + 2;

				temp_orig_f[cnt] = glv[idx0].index;
				temp_t[cnt].x = glv[idx0].s * header.skinwidth;
				temp_t[cnt].y = glv[idx0].t * header.skinheight;
				cnt++;

				temp_orig_f[cnt] = glv[idx1].index;
				temp_t[cnt].x = glv[idx1].s * header.skinwidth;
				temp_t[cnt].y = glv[idx1].t * header.skinheight;
				cnt++;

				temp_orig_f[cnt] = glv[idx2].index;
				temp_t[cnt].x = glv[idx2].s * header.skinwidth;
				temp_t[cnt].y = glv[idx2].t * header.skinheight;
				cnt++;

				tri_cnt += 3;
			}
			pmdata[pmodel_id].num_vert[i] = tri_cnt;
		}

		glv_offset += glnum_verts;
	}

	// Split vertices at UV seams
	struct SplitVertMD2 {
		int orig_idx;
		float u, v;
	};
	std::vector<SplitVertMD2> split_verts;
	std::vector<std::vector<int>> orig_to_splits(header.num_verts);

	for (i = 0; i < total_tri_verts; i++) {
		int orig_idx = temp_orig_f[i];
		float u = temp_t[i].x;
		float v = temp_t[i].y;

		int match = -1;
		if (orig_idx >= 0 && orig_idx < header.num_verts) {
			for (int sv : orig_to_splits[orig_idx]) {
				if (fabsf(split_verts[sv].u - u) < 1e-4f && fabsf(split_verts[sv].v - v) < 1e-4f) {
					match = sv;
					break;
				}
			}
		}

		if (match == -1) {
			match = (int)split_verts.size();
			split_verts.push_back({orig_idx, u, v});
			if (orig_idx >= 0 && orig_idx < header.num_verts) {
				orig_to_splits[orig_idx].push_back(match);
			}
		}

		pmdata[pmodel_id].f[i] = match;
		pmdata[pmodel_id].t[i].x = u;
		pmdata[pmodel_id].t[i].y = v;
	}

	int num_split_verts = (int)split_verts.size();

	// allocate vertex memory per frame based on split vertex count
	pmdata[pmodel_id].w = new VERT *[header.num_frames];

	for (i = 0; i < header.num_frames; i++)
		pmdata[pmodel_id].w[i] = new VERT[num_split_verts];

	// read vertices for all frames and map to split vertices
	fseek(fp, (UINT)header.offset_frames, SEEK_SET);

	std::vector<MD2VERTEX> orig_frame_verts(header.num_verts);

	for (frame_num = 0; frame_num < header.num_frames; frame_num++) {
		fread(bscale, sizeof(float), 3, fp);
		fread(translate, sizeof(float), 3, fp);
		fread(name, 1, 16, fp);

		for (j = 0; j < header.num_verts; j++) {
			fread(&orig_frame_verts[j], sizeof(MD2VERTEX), 1, fp);
		}

		for (j = 0; j < num_split_verts; j++) {
			int orig_idx = split_verts[j].orig_idx;
			if (orig_idx >= 0 && orig_idx < header.num_verts) {
				const auto &bv = orig_frame_verts[orig_idx];
				pmdata[pmodel_id].w[frame_num][j].x = scale * (bscale[0] * bv.v[0] + translate[0]);
				pmdata[pmodel_id].w[frame_num][j].y = scale * (bscale[1] * bv.v[1] + translate[1]);
				pmdata[pmodel_id].w[frame_num][j].z = scale * (bscale[2] * bv.v[2] + translate[2]);
			} else {
				pmdata[pmodel_id].w[frame_num][j].x = 0.0f;
				pmdata[pmodel_id].w[frame_num][j].y = 0.0f;
				pmdata[pmodel_id].w[frame_num][j].z = 0.0f;
			}
			pmdata[pmodel_id].w[frame_num][j].tu = split_verts[j].u;
			pmdata[pmodel_id].w[frame_num][j].tv = split_verts[j].v;
		}
	}

	pmdata[pmodel_id].num_polys_per_frame = glc_cnt;
	pmdata[pmodel_id].num_faces = glc_cnt;
	pmdata[pmodel_id].num_verts = cnt; // glv_cnt; // cnt;
	pmdata[pmodel_id].scale = scale;
	fclose(fp);

	pmdata[pmodel_id].tex_alias = texture_alias;

	pmdata[pmodel_id].skx = (float)1 / header.skinwidth;
	pmdata[pmodel_id].sky = (float)1 / header.skinheight;
	pmdata[pmodel_id].num_frames = header.num_frames;
	pmdata[pmodel_id].use_indexed_primitive = FALSE;

	ComputeMD2ModelNormals(pmodel_id);
	SmoothMD2ModelNormals(pmodel_id);

	return TRUE;
}
