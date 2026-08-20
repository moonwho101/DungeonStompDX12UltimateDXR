#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
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

	// allocate memory dynamically

	pmdata[pmodel_id].w = new VERT *[header.num_frames];

	for (i = 0; i < header.num_frames; i++)
		pmdata[pmodel_id].w[i] = new VERT[header.num_verts];

	pmdata[pmodel_id].f = new int[total_tri_verts];
	pmdata[pmodel_id].num_vert = new int[glc_cnt];
	pmdata[pmodel_id].poly_cmd = new D3DPRIMITIVETYPE[glc_cnt];
	pmdata[pmodel_id].texture_list = new int[glc_cnt];
	pmdata[pmodel_id].t = new VERT[total_tri_verts];

	int mem = 0;
	mem += (sizeof(VERT) * header.num_verts * header.num_frames);
	mem += (sizeof(short) * total_tri_verts);
	mem += (sizeof(short) * header.num_verts);
	mem += (sizeof(short) * glc_cnt);
	mem += (sizeof(short) * glc_cnt);
	mem += (sizeof(VERT) * total_tri_verts);

	mem = mem / 1024;

	_itoa_s(mem, buffer, _countof(buffer), 10);
	strcat_s(buffer, " KB\n");
	// PrintMessage(NULL, "Memory allocated = ", buffer, LOGFILE_ONLY);

	// load GL Commands into pmdata structure as triangle lists

	cnt = 0;
	int glv_offset = 0;

	for (i = 0; i < glc_cnt; i++) {
		if (glc[i] == 0) {
			// PrintMessage(NULL, "ERROR MD2 GL COMMAND DATA", NULL, LOGFILE_ONLY);
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

				pmdata[pmodel_id].f[cnt] = glv[idx0].index;
				pmdata[pmodel_id].t[cnt].x = glv[idx0].s * header.skinwidth;
				pmdata[pmodel_id].t[cnt].y = glv[idx0].t * header.skinheight;
				cnt++;

				pmdata[pmodel_id].f[cnt] = glv[idx1].index;
				pmdata[pmodel_id].t[cnt].x = glv[idx1].s * header.skinwidth;
				pmdata[pmodel_id].t[cnt].y = glv[idx1].t * header.skinheight;
				cnt++;

				pmdata[pmodel_id].f[cnt] = glv[idx2].index;
				pmdata[pmodel_id].t[cnt].x = glv[idx2].s * header.skinwidth;
				pmdata[pmodel_id].t[cnt].y = glv[idx2].t * header.skinheight;
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

				pmdata[pmodel_id].f[cnt] = glv[idx0].index;
				pmdata[pmodel_id].t[cnt].x = glv[idx0].s * header.skinwidth;
				pmdata[pmodel_id].t[cnt].y = glv[idx0].t * header.skinheight;
				cnt++;

				pmdata[pmodel_id].f[cnt] = glv[idx1].index;
				pmdata[pmodel_id].t[cnt].x = glv[idx1].s * header.skinwidth;
				pmdata[pmodel_id].t[cnt].y = glv[idx1].t * header.skinheight;
				cnt++;

				pmdata[pmodel_id].f[cnt] = glv[idx2].index;
				pmdata[pmodel_id].t[cnt].x = glv[idx2].s * header.skinwidth;
				pmdata[pmodel_id].t[cnt].y = glv[idx2].t * header.skinheight;
				cnt++;

				tri_cnt += 3;
			}
			pmdata[pmodel_id].num_vert[i] = tri_cnt;
		}

		glv_offset += glnum_verts;
	}

	// read vertices for all frames

	fseek(fp, (UINT)header.offset_frames, SEEK_SET);

	for (frame_num = 0; frame_num < header.num_frames; frame_num++) {
		fread(bscale, sizeof(float), 3, fp);
		fread(translate, sizeof(float), 3, fp);
		fread(name, 1, 16, fp);

		// strcpy_s(frame_list[frame_num].framename, name );

		for (j = 0; j < header.num_verts; j++) // VERTS
		{
			fread(&bverts, sizeof(MD2VERTEX), 1, fp);

			pmdata[pmodel_id].w[frame_num][j].x = scale * (bscale[0] * bverts.v[0] + translate[0]);
			pmdata[pmodel_id].w[frame_num][j].y = scale * (bscale[1] * bverts.v[1] + translate[1]);
			pmdata[pmodel_id].w[frame_num][j].z = scale * (bscale[2] * bverts.v[2] + translate[2]);
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

	return TRUE;
}
