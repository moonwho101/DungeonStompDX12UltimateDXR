#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Dungeon.h"
#include <d3dtypes.h>
#include "world.hpp"
#include "GlobalSettings.hpp"
#include "Missle.hpp"
#include "GameLogic.hpp"
#include "DungeonStomp.hpp"
#include "ProcessModel.hpp"
#include "Dice.hpp"
#include "CameraBob.hpp"
#include "DungeonGenerator.hpp"
#include "imgui/imgui.h"

extern int load_level(char *filename);
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

extern int number_of_tex_aliases;
extern int textcounter;
extern char gfinaltext[2048];
extern bool enableSSao;
int numCharacters = 0;
extern ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;
ID3D12PipelineState *textPSO;                    // pso containing a pipeline state
ID3D12PipelineState *rectanglePSO[MaxRectangle]; // pso containing a pipeline state

Font arialFont; // this will store our arial font information
void display_font(float x, float y, char text[1000], int r, int g, int b);
void MakeDamageDice();

D3DVERTEX2 bubble[600];
int countdisplay = 0;

int displayCaptureIndex[1000];
int displayCaptureCount[1000];
int displayCapture = 0;

int displayShadowMap = 0;

extern bool drawingShadowMap;
extern bool enableOverheadMap;
extern bool drawingSSAO;
extern CameraBob bobY;
extern CameraBob bobX;

struct gametext {
	int textnum;
	int type;
	char text[2048];
	int shown;
};

extern gametext gtext[200];
int maxNumTextCharacters = 2048; // the maximum number of characters you can render during a frame. This is just used to make sure
                                 // there is enough memory allocated for the text vertex buffer each frame

int maxNumRectangleCharacters = 1024;

extern FLOAT LevelModTime;
extern int totalmod;
extern LEVELMOD *levelmodify;
void AddTreasureDrop(float x, float y, float z, int raction);
void ScanModJump(int jump);
extern int countmodtime;
extern FLOAT LevelModLastTime;

extern SWITCHMOD *switchmodify;
void SetDiceTexture(bool showroll);
int FindTextureAlias(char *alias);
float gFps = 0;
float gMspf = 0;

extern float currentspeed;
extern int lastcollide;
extern bool enableDXR;
extern int gDXRTriangleCount;
extern int gDXRAliasCount;
extern int gDXRVertexCount;
extern int gDXROutputWidth;
extern int gDXROutputHeight;
extern bool enableVsync;
extern bool enablePlayerHUD;
extern bool enablePlayerCaptions;
extern bool enableOnscreenDebug;
extern bool enableCameraBob;
extern bool enableNormalmap;
extern bool enableShadowmapFeature;
extern bool enableVRS;

// GPU info populated once at device creation (d3dApp.cpp → InitDirect3D)
char gGpuName[256] = "Unknown";
SIZE_T gGpuVramMB = 0;
char gGpuFeatureLevel[16] = "?";
char gGpuShaderModel[16] = "?";
bool gGpuVRSSupported = false;
bool gGpuMeshShaderSupported = false;
bool gGpuSamplerFeedbackSupported = false;
bool gGpuTearingSupported = false;

Font LoadFont(LPCWSTR filename, int windowWidth, int windowHeight) {
	std::wifstream fs;
	fs.open(filename);

	Font font;
	std::wstring tmp;
	int startpos;

	// extract font name
	fs >> tmp >> tmp; // info face="Arial"
	startpos = (int)tmp.find(L"\"") + 1;
	font.name = tmp.substr(startpos, tmp.size() - startpos - 1);

	// get font size
	fs >> tmp; // size=73
	startpos = (int)tmp.find(L"=") + 1;
	font.size = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

	// bold, italic, charset, unicode, stretchH, smooth, aa, padding, spacing
	fs >> tmp >> tmp >> tmp >> tmp >> tmp >> tmp >> tmp; // bold=0 italic=0 charset="" unicode=0 stretchH=100 smooth=1 aa=1

	// get padding
	fs >> tmp; // padding=5,5,5,5
	startpos = (int)tmp.find(L"=") + 1;
	tmp = tmp.substr(startpos, tmp.size() - startpos); // 5,5,5,5

	// get up padding
	startpos = (int)tmp.find(L",") + 1;
	font.toppadding = std::stoi(tmp.substr(0, startpos)) / (float)windowWidth;

	// get right padding
	tmp = tmp.substr(startpos, tmp.size() - startpos);
	startpos = (int)tmp.find(L",") + 1;
	font.rightpadding = std::stoi(tmp.substr(0, startpos)) / (float)windowWidth;

	// get down padding
	tmp = tmp.substr(startpos, tmp.size() - startpos);
	startpos = (int)tmp.find(L",") + 1;
	font.bottompadding = std::stoi(tmp.substr(0, startpos)) / (float)windowWidth;

	// get left padding
	tmp = tmp.substr(startpos, tmp.size() - startpos);
	font.leftpadding = std::stoi(tmp) / (float)windowWidth;

	fs >> tmp; // spacing=0,0

	// get lineheight (how much to move down for each line), and normalize (between 0.0 and 1.0 based on size of font)
	fs >> tmp >> tmp; // common lineHeight=95
	startpos = (int)tmp.find(L"=") + 1;
	font.lineHeight = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)windowHeight;

	// get base height (height of all characters), and normalize (between 0.0 and 1.0 based on size of font)
	fs >> tmp; // base=68
	startpos = (int)tmp.find(L"=") + 1;
	font.baseHeight = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)windowHeight;

	// get texture width
	fs >> tmp; // scaleW=512
	startpos = (int)tmp.find(L"=") + 1;
	font.textureWidth = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

	// get texture height
	fs >> tmp; // scaleH=512
	startpos = (int)tmp.find(L"=") + 1;
	font.textureHeight = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

	// get pages, packed, page id
	fs >> tmp >> tmp; // pages=1 packed=0
	fs >> tmp >> tmp; // page id=0

	// get texture filename
	std::wstring wtmp;
	fs >> wtmp; // file="Arial.png"
	startpos = (int)wtmp.find(L"\"") + 1;
	font.fontImage = wtmp.substr(startpos, wtmp.size() - startpos - 1);

	// get number of characters
	fs >> tmp >> tmp; // chars count=97
	startpos = (int)tmp.find(L"=") + 1;
	font.numCharacters = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

	// initialize the character list
	font.CharList = new FontChar[font.numCharacters];

	for (int c = 0; c < font.numCharacters; ++c) {
		// get unicode id
		fs >> tmp >> tmp; // char id=0
		startpos = (int)tmp.find(L"=") + 1;
		font.CharList[c].id = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

		// get x
		fs >> tmp; // x=392
		startpos = (int)tmp.find(L"=") + 1;
		font.CharList[c].u = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)font.textureWidth;

		// get y
		fs >> tmp; // y=340
		startpos = (int)tmp.find(L"=") + 1;
		font.CharList[c].v = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)font.textureHeight;

		// get width
		fs >> tmp; // width=47
		startpos = (int)tmp.find(L"=") + 1;
		tmp = tmp.substr(startpos, tmp.size() - startpos);
		font.CharList[c].width = (float)std::stoi(tmp) / (float)windowWidth;
		font.CharList[c].twidth = (float)std::stoi(tmp) / (float)font.textureWidth;

		// get height
		fs >> tmp; // height=57
		startpos = (int)tmp.find(L"=") + 1;
		tmp = tmp.substr(startpos, tmp.size() - startpos);
		font.CharList[c].height = (float)std::stoi(tmp) / (float)windowHeight;
		font.CharList[c].theight = (float)std::stoi(tmp) / (float)font.textureHeight;

		// get xoffset
		fs >> tmp; // xoffset=-6
		startpos = (int)tmp.find(L"=") + 1;
		font.CharList[c].xoffset = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)windowWidth;

		// get yoffset
		fs >> tmp; // yoffset=16
		startpos = (int)tmp.find(L"=") + 1;
		font.CharList[c].yoffset = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)windowHeight;

		// get xadvance
		fs >> tmp; // xadvance=65
		startpos = (int)tmp.find(L"=") + 1;
		font.CharList[c].xadvance = (float)std::stoi(tmp.substr(startpos, tmp.size() - startpos)) / (float)windowWidth;

		// get page
		// get channel
		fs >> tmp >> tmp; // page=0    chnl=0
	}

	// get number of kernings
	fs >> tmp >> tmp; // kernings count=96
	startpos = (int)tmp.find(L"=") + 1;
	font.numKernings = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

	// initialize the kernings list
	font.KerningsList = new FontKerning[font.numKernings];

	for (int k = 0; k < font.numKernings; ++k) {
		// get first character
		fs >> tmp >> tmp; // kerning first=87
		startpos = (int)tmp.find(L"=") + 1;
		font.KerningsList[k].firstid = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

		// get second character
		fs >> tmp; // second=45
		startpos = (int)tmp.find(L"=") + 1;
		font.KerningsList[k].secondid = std::stoi(tmp.substr(startpos, tmp.size() - startpos));

		// get amount
		fs >> tmp; // amount=-1
		startpos = (int)tmp.find(L"=") + 1;
		int t = (int)std::stoi(tmp.substr(startpos, tmp.size() - startpos));
		font.KerningsList[k].amount = (float)t / (float)windowWidth;
	}

	return font;
}

void DungeonStompApp::RenderRectangle(Font font, int index, int textureid, XMFLOAT2 pos, XMFLOAT2 scale, XMFLOAT2 padding, XMFLOAT4 color) {
	FontChar *fc = font.GetChar(L'A');
	if (fc == nullptr)
		return;

	float screenX = (pos.x * 2.0f) - 1.0f;
	float screenY = ((1.0f - pos.y) * 2.0f) - 1.0f;

	// Each rectangle has its own buffer — always write to slot [0]
	TextVertex *vert = (TextVertex *)rectangleVBGPUAddress[index];

	vert[0] = TextVertex(color.x,
	                     color.y,
	                     color.z,
	                     color.w,
	                     0.0f,
	                     0.0f,
	                     1.0f,
	                     1.0f,
	                     screenX + (fc->xoffset * scale.x),
	                     screenY - (fc->yoffset * scale.y),
	                     fc->width * scale.x,
	                     fc->height * scale.y);

	rectangleActive[index] = true;
	rectangleTexId[index] = textureid;
}

// pos is the top-left corner and size the extent, both as fractions of the screen (0..1)
void DungeonStompApp::RenderSolidRectangle(int index, XMFLOAT2 pos, XMFLOAT2 size, XMFLOAT4 color, int textureId) {
	TextVertex *vert = (TextVertex *)rectangleVBGPUAddress[index];

	vert[0] = TextVertex(color.x,
	                     color.y,
	                     color.z,
	                     color.w,
	                     0.0f,
	                     0.0f,
	                     1.0f,
	                     1.0f,
	                     (pos.x * 2.0f) - 1.0f,
	                     ((1.0f - pos.y) * 2.0f) - 1.0f,
	                     size.x * 2.0f,
	                     size.y * 2.0f);

	rectangleActive[index] = true;
	rectangleTexId[index] = textureId;
}

void DungeonStompApp::FlushRectangles() {
	for (int i = 0; i < MaxRectangle; ++i) {
		if (!rectangleActive[i])
			continue;

		mCommandList->SetPipelineState(rectanglePSO[i]);
		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		mCommandList->IASetVertexBuffers(0, 1, &rectangleVertexBufferView[i]);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(rectangleTexId[i], mCbvSrvDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(3, tex);

		mCommandList->DrawInstanced(4, 1, 0, 0);

		rectangleActive[i] = false;
	}
}

void DungeonStompApp::FlushText() {
	if (numCharacters == 0)
		return;

	// set the text pipeline state object
	mCommandList->SetPipelineState(textPSO);

	// this way we only need 4 vertices per quad rather than 6 if we were to use a triangle list topology
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// set the text vertex buffer
	mCommandList->IASetVertexBuffers(0, 1, &textVertexBufferView);

	// bind the text srv
	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	tex.Offset(103, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(3, tex);

	mCommandList->DrawInstanced(4, numCharacters, 0, 0);

	numCharacters = 0;
}

void DungeonStompApp::RenderText(Font font, std::wstring text, XMFLOAT2 pos, XMFLOAT2 scale, XMFLOAT2 padding, XMFLOAT4 color) {

	if (drawingShadowMap || drawingSSAO || !enablePlayerHUD)
		return;

	float topLeftScreenX = (pos.x * 2.0f) - 1.0f;
	float topLeftScreenY = ((1.0f - pos.y) * 2.0f) - 1.0f;

	float x = topLeftScreenX;
	float y = topLeftScreenY;

	float horrizontalPadding = (font.leftpadding + font.rightpadding) * padding.x;
	float verticalPadding = (font.toppadding + font.bottompadding) * padding.y;

	// cast the gpu virtual address to a textvertex, so we can directly store our vertices there
	TextVertex *vert = (TextVertex *)textVBGPUAddress;

	wchar_t lastChar = -1; // no last character to start with

	for (int i = 0; i < text.size(); ++i) {
		wchar_t c = text[i];

		FontChar *fc = font.GetChar(c);

		// character not in font char set
		if (fc == nullptr)
			continue;

		// end of string
		if (c == L'\0')
			break;

		// new line
		if (c == L'\n') {
			x = topLeftScreenX;
			y -= (font.lineHeight + verticalPadding) * scale.y;
			continue;
		}

		// don't overflow the buffer. In your app if this is true, you can implement a resize of your text vertex buffer
		if (numCharacters >= maxNumTextCharacters)
			break;

		float kerning = 0.0f;
		if (i > 0)
			kerning = font.GetKerning(lastChar, c);

		vert[numCharacters] = TextVertex(color.x,
		                                 color.y,
		                                 color.z,
		                                 color.w,
		                                 fc->u,
		                                 fc->v,
		                                 fc->twidth,
		                                 fc->theight,
		                                 x + ((fc->xoffset + kerning) * scale.x),
		                                 y - (fc->yoffset * scale.y),
		                                 fc->width * scale.x,
		                                 fc->height * scale.y);

		numCharacters++;

		// remove horrizontal padding and advance to next char position
		x += (fc->xadvance - horrizontalPadding) * scale.x;

		lastChar = c;
	}
}

void DungeonStompApp::DisplayHud() {

	static float hudLogoDisplayElapsed = 0.0f;

	numCharacters = 0;
	for (int i = 0; i < MaxRectangle; ++i)
		rectangleActive[i] = false;

	char junk[255];

	RenderRectangle(arialFont, 0, 355, XMFLOAT2(0.02f, 0.74f), XMFLOAT2(6.00f, 6.00f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));

	SetDiceTexture(false);

	int diceTexture = FindTextureAlias(dice[0].name);
	RenderRectangle(arialFont, 1, diceTexture, XMFLOAT2(0.475f, 0.9f), XMFLOAT2(1.00f, 1.00f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));

	diceTexture = FindTextureAlias(dice[1].name);
	RenderRectangle(arialFont, 2, diceTexture, XMFLOAT2(0.525f, 0.9f), XMFLOAT2(1.00f, 1.00f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));

	if (displayShadowMap) {
		diceTexture = enableSSao ? number_of_tex_aliases + 2 : number_of_tex_aliases + 1;
		RenderRectangle(arialFont, 3, diceTexture, XMFLOAT2(0.75f, 0.55f), XMFLOAT2(7.00f, 7.00f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));
	}

	// Overhead map in the upper right corner: the player is always at the centre, facing up.
	if (enableOverheadMap && mOverheadMap) {
		const float mapH = 0.30f;
		const float mapW = mapH * (float)mClientHeight / (float)mClientWidth; // square on screen
		const float margin = 0.01f;
		const float border = 0.003f;
		const float mapX = 1.0f - margin - mapW;
		const float mapY = margin * 1.5f;

		RenderSolidRectangle(7, XMFLOAT2(mapX - border * 0.5f, mapY - border), XMFLOAT2(mapW + border, mapH + border * 2.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 0.85f));
		RenderSolidRectangle(8, XMFLOAT2(mapX, mapY), XMFLOAT2(mapW, mapH), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), (int)mOverheadMapHeapIndex);

		const float dotH = 0.012f;
		const float dotW = dotH * (float)mClientHeight / (float)mClientWidth;
		RenderSolidRectangle(9, XMFLOAT2(mapX + (mapW - dotW) * 0.5f, mapY + (mapH - dotH) * 0.5f), XMFLOAT2(dotW, dotH), XMFLOAT4(1.0f, 0.9f, 0.1f, 1.0f));
	}

	// only show logo during intro or when player is dead
	if (hudLogoDisplayElapsed < 4.0f || !player_list[trueplayernum].bIsPlayerAlive) {
		hudLogoDisplayElapsed += mTimer.DeltaTime();
		diceTexture = FindTextureAlias("pb0");
		RenderRectangle(arialFont, 4, diceTexture, XMFLOAT2(0.36f, 0.05f), XMFLOAT2(10.00f, 10.00f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f));
	}

	if (!enableVsync) {
		sprintf_s(junk, "fps: %d", (int)gFps);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.125f, 0.98f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

		sprintf_s(junk, "mpf: %.2f", gMspf);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.125f, 0.96f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
	}

	sprintf_s(junk, "Health");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.0f, 0.82f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%d/%d", player_list[trueplayernum].health, player_list[trueplayernum].hp);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.137f, 0.82f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	// health bar: green when healthy, yellow when hurt, red when low
	{
		const float barX = 0.071f;
		const float barY = 0.8240f;
		const float barW = 0.062f;
		const float barH = 0.012f;
		const float border = 0.0015f;

		float healthFrac = 0.0f;
		if (player_list[trueplayernum].hp > 0)
			healthFrac = (float)player_list[trueplayernum].health / (float)player_list[trueplayernum].hp;
		healthFrac = max(0.0f, min(1.0f, healthFrac));

		XMFLOAT4 barColor = XMFLOAT4(0.0f, 0.8f, 0.0f, 1.0f);
		if (healthFrac <= 0.25f)
			barColor = XMFLOAT4(0.9f, 0.0f, 0.0f, 1.0f);
		else if (healthFrac <= 0.5f)
			barColor = XMFLOAT4(0.9f, 0.8f, 0.0f, 1.0f);

		RenderSolidRectangle(5, XMFLOAT2(barX - border, barY - border), XMFLOAT2(barW + border * 2.0f, barH + border * 2.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 0.7f));
		if (healthFrac > 0.0f)
			RenderSolidRectangle(6, XMFLOAT2(barX, barY), XMFLOAT2(barW * healthFrac, barH), barColor);
	}

	sprintf_s(junk, "Weapon");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.84f), XMFLOAT2(0.30f, 0.30f));

	const char *gunname = your_gun[current_gun].gunname;
	int scrollCharges = (int)your_gun[current_gun].x_offset;
	if (strstr(gunname, "SCROLL-MAGICMISSLE") != NULL)
		sprintf_s(junk, "MISSILE %d", scrollCharges);
	else if (strstr(gunname, "SCROLL-FIREBALL") != NULL)
		sprintf_s(junk, "FIREBALL %d", scrollCharges);
	else if (strstr(gunname, "SCROLL-LIGHTNING") != NULL)
		sprintf_s(junk, "LIGHTNING %d", scrollCharges);
	else if (strstr(gunname, "SCROLL-HEALING") != NULL)
		sprintf_s(junk, "HEALING %d", scrollCharges);
	else if (strstr(gunname, "SUPERFLAMESWORD") != NULL)
		sprintf_s(junk, "SUPER SWORD");
	else if (strstr(gunname, "BASTARDSWORD") != NULL)
		sprintf_s(junk, "BASTARDSWORD");
	else if (strstr(gunname, "BATTLEAXE") != NULL)
		sprintf_s(junk, "BATTLE AXE");
	else if (strstr(gunname, "ICESWORD") != NULL)
		sprintf_s(junk, "ICE SWORD");
	else if (strstr(gunname, "MORNINGSTAR") != NULL)
		sprintf_s(junk, "MORNING STAR");
	else if (strstr(gunname, "SPIKEDFLAIL") != NULL)
		sprintf_s(junk, "SPIKED FLAIL");
	else if (strstr(gunname, "SPLITSWORD") != NULL)
		sprintf_s(junk, "SPLIT SWORD");
	else if (strstr(gunname, "FLAMESWORD") != NULL)
		sprintf_s(junk, "FLAME SWORD");
	else if (strstr(gunname, "LIGHTNINGSWORD") != NULL)
		sprintf_s(junk, "LIGHT SWORD");
	else
		sprintf_s(junk, "%s", gunname);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.84f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	sprintf_s(junk, "Damage");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.86f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%dD%d", player_list[trueplayernum].damage1, player_list[trueplayernum].damage2);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.86f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	sprintf_s(junk, "Bonus");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.88f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "+%d/%+d", your_gun[current_gun].sattack, your_gun[current_gun].sdamage);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.88f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	int nextlevelxp = LevelUpXPNeeded(player_list[trueplayernum].xp) + 1;

	sprintf_s(junk, "XP");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.90f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%d", player_list[trueplayernum].xp);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.90f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	sprintf_s(junk, "Level");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.92f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%d (%d)", player_list[trueplayernum].hd, nextlevelxp);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.92f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	sprintf_s(junk, "Armour");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.94f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%d", player_list[trueplayernum].ac);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.94f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	sprintf_s(junk, "Gold");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.96f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%d", player_list[trueplayernum].gold);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.96f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	sprintf_s(junk, "Keys");
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.00f, 0.98f), XMFLOAT2(0.30f, 0.30f));
	sprintf_s(junk, "%d", player_list[trueplayernum].keys);
	RenderText(arialFont, charToWChar(junk), XMFLOAT2(0.07f, 0.98f), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));

	// scroll message list
	char junk2[2048];
	const int scrolllistnum = 6;
	float scrollmessage1 = 0.0f;
	int scount = sliststart;
	for (int count = 0; count < scrolllistnum; count++) {
		sprintf_s(junk2, "%s", scrolllist1[scount].text);
		RenderText(arialFont, charToWChar(junk2), XMFLOAT2(0.0f, 0.1f + scrollmessage1), XMFLOAT2(0.30f, 0.30f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4((float)scrolllist1[scount].r, (float)scrolllist1[scount].g, (float)scrolllist1[scount].b, 1.0f));
		scrollmessage1 -= 0.02f;
		if (--scount < 0)
			scount = scrolllistnum - 1;
	}

	if (enableOnscreenDebug) {
		const float lx = 0.65f;
		const float vx = 0.79f;
		const float rh = 0.022f;
		const XMFLOAT2 sc = { 0.30f, 0.30f };
		const XMFLOAT2 pad = { 0.5f, 0.0f };
		const XMFLOAT4 hdr = { 1.0f, 1.0f, 0.0f, 1.0f }; // yellow  - section headers
		const XMFLOAT4 lbl = { 0.7f, 0.7f, 0.7f, 1.0f }; // gray    - labels
		const XMFLOAT4 val = { 0.0f, 1.0f, 1.0f, 1.0f }; // cyan    - values

		float y = 0.02f;

		// --- RENDER ---
		sprintf_s(junk, "[ RENDER ]");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;

		sprintf_s(junk, "polys");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", number_of_polys_per_frame);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "tris");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", num_triangles_in_scene);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "verts");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", num_verts_in_scene);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "dpcmds");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", num_dp_commands_in_scene);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh * 2;

		// --- SCENE ---
		sprintf_s(junk, "[ SCENE ]");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;

		sprintf_s(junk, "monsters");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", num_monsters);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "items");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", itemlistcount);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "objects");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", oblist_length);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "vis cnt");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", cnt);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh * 2;

		// --- CAMERA ---
		sprintf_s(junk, "[ CAMERA ]");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;

		sprintf_s(junk, "x");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%.1f", mEyePos.x);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "y");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%.1f", mEyePos.y);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "z");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%.1f", mEyePos.z);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "yaw");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%.1f", (float)angy);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "pitch");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%.1f", (float)look_up_ang);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		// sprintf_s(junk, "speed");  RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		// sprintf_s(junk, "%.1f", currentspeed);
		// RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val); y += rh;

		sprintf_s(junk, "collide");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", lastcollide);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh * 2;

		// --- DXR ---
		sprintf_s(junk, "[ DXR ]");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;

		sprintf_s(junk, "status");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", enableDXR ? "ON" : "OFF");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad,
		           enableDXR ? XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) : XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f));
		y += rh;

		sprintf_s(junk, "outdoor");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", outside ? "YES" : "NO");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "output");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%dx%d", gDXROutputWidth, gDXROutputHeight);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "tris");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", gDXRTriangleCount);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "verts");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", gDXRVertexCount);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "aliases");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", gDXRAliasCount);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh * 2;

		// --- PERF ---
		sprintf_s(junk, "[ PERF ]");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;

		sprintf_s(junk, "fps");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%d", (int)gFps);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "ms/frm");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%.2f", gMspf);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "res");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%dx%d", gDXROutputWidth, gDXROutputHeight);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "vsync");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", enableVsync ? "ON" : "OFF");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad,
		           enableVsync ? XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) : XMFLOAT4(1.0f, 0.6f, 0.0f, 1.0f));
		y += rh * 2;

		// --- GPU ---
		sprintf_s(junk, "[ GPU ]");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;

		sprintf_s(junk, "name");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		// Truncate long GPU names to fit the column
		char gpuShort[24];
		strncpy_s(gpuShort, gGpuName, 23);
		gpuShort[23] = '\0';
		RenderText(arialFont, charToWChar(gpuShort), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "vram");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%zu MB", gGpuVramMB);
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "fl");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		RenderText(arialFont, charToWChar(gGpuFeatureLevel), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "shader m");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		RenderText(arialFont, charToWChar(gGpuShaderModel), XMFLOAT2(vx, y), sc, pad, val);
		y += rh;

		sprintf_s(junk, "vrs");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", gGpuVRSSupported ? "YES" : "NO");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad,
		           gGpuVRSSupported ? val : XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f));
		y += rh;

		sprintf_s(junk, "mesh sh");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", gGpuMeshShaderSupported ? "YES" : "NO");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad,
		           gGpuMeshShaderSupported ? val : XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f));
		y += rh;

		sprintf_s(junk, "samp fb");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", gGpuSamplerFeedbackSupported ? "YES" : "NO");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad,
		           gGpuSamplerFeedbackSupported ? val : XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f));
		y += rh;

		sprintf_s(junk, "tearing");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, lbl);
		sprintf_s(junk, "%s", gGpuTearingSupported ? "YES" : "NO");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(vx, y), sc, pad,
		           gGpuTearingSupported ? val : XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f));
		y += rh;

		// stop last char flickers
		sprintf_s(junk, "  ");
		RenderText(arialFont, charToWChar(junk), XMFLOAT2(lx, y), sc, pad, hdr);
		y += rh;
	}
}


void SetStartSpot();

void DungeonStompApp::RenderImGuiTogglePanel() {
	if (drawingShadowMap || drawingSSAO)
		return;

	ImGui::SetNextWindowPos(ImVec2(15.0f, 15.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.0f, 380.0f), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
	if (ImGui::Begin("Engine Settings & Toggles", nullptr, flags)) {

		ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "Dungeon Stomp DX12 Ultimate");
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::TreeNodeEx("Dungeon Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
			static int genSeed = 0;
			static int genCount = 350;
			ImGui::InputInt("Seed (0=Random)", &genSeed);
			if (genSeed < 0)
				genSeed = 0;
			ImGui::SliderInt("Tile Count", &genCount, 50, 1000);

			if (ImGui::Button("Generate Enhanced Dungeon")) {
				DungeonGen::GeneratorOptions options;
				options.seed = static_cast<unsigned int>(genSeed);
				options.numObjectsToPlace = genCount;
				if (DungeonGen::GenerateDungeonNewObjects("level1.map", options)) {
					load_level("level1");
					SetStartSpot();
					sprintf_s(gActionMessage, "Generated New Enhanced Dungeon (%d tiles)", genCount);
					UpdateScrollList(0, 255, 255);
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Generate Classic Dungeon")) {
				DungeonGen::GeneratorOptions options;
				options.seed = static_cast<unsigned int>(genSeed);
				options.numObjectsToPlace = genCount;
				if (DungeonGen::GenerateDungeonClassic("level1.map", options)) {
					load_level("level1");
					// ResetPlayer();
					SetStartSpot();
					sprintf_s(gActionMessage, "Generated Classic Dungeon (%d tiles)", genCount);
					UpdateScrollList(0, 255, 255);
				}
			}
			ImGui::TreePop();
		}
		
		ImGui::Spacing();

		if (ImGui::TreeNodeEx("Graphics & Features", ImGuiTreeNodeFlags_DefaultOpen)) {

			// DXR Raytracing
			if (!mDXRInitialized)
				ImGui::BeginDisabled();
			if (ImGui::Checkbox("DXR Raytracing [R]", &enableDXR)) {
				if (enableDXR && mDXRInitialized) {
					sprintf_s(gActionMessage, "DirectX Raytracing Enabled");
				} else {
					enableDXR = false;
					sprintf_s(gActionMessage, "DXR Not Supported on this GPU");
				}
				UpdateScrollList(0, 255, 255);
			}
			if (!mDXRInitialized) {
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::TextDisabled("(Unsupported)");
			}

			// Normal Mapping
			if (ImGui::Checkbox("Normal Mapping [N]", &enableNormalmap)) {
				if (enableNormalmap) {
					SetTextureNormalMap();
				} else {
					SetTextureNormalMapEmpty();
				}
				sprintf_s(gActionMessage, "Normal Map %s", enableNormalmap ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Shadow Maps
			if (ImGui::Checkbox("Shadow Maps [J]", &enableShadowmapFeature)) {
				sprintf_s(gActionMessage, "Shadowmap Feature %s", enableShadowmapFeature ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Shadow Overlay
			bool shadowOverlay = (displayShadowMap != 0);
			if (ImGui::Checkbox("Shadow Overlay [M]", &shadowOverlay)) {
				displayShadowMap = shadowOverlay ? 1 : 0;
				sprintf_s(gActionMessage, "Shadow Overlay %s", displayShadowMap ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Overhead Map
			if (ImGui::Checkbox("Overhead Map [L]", &enableOverheadMap)) {
				sprintf_s(gActionMessage, "Overhead Map %s", enableOverheadMap ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}
			if (enableOverheadMap) {
				ImGui::Indent();
				ImGui::SliderFloat("Map Half Extent", &mMapHalfExtent, 50.0f, 3000.0f, "%.0f");
				ImGui::SliderFloat("Map Clip Height", &mMapClipHeight, -100.0f, 500.0f, "%.1f");
				ImGui::Unindent();
			}

			// SSAO Effect
			if (ImGui::Checkbox("SSAO Effect [O]", &enableSSao)) {
				sprintf_s(gActionMessage, "SSAO %s", enableSSao ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Variable Rate Shading (VRS)
			bool vrsSupported = mVRSHelper.IsSupported();
			if (!vrsSupported)
				ImGui::BeginDisabled();
			if (ImGui::Checkbox("VRS Shading [T]", &enableVRS)) {
				if (enableVRS && vrsSupported) {
					sprintf_s(gActionMessage, "Variable Rate Shading Enabled");
				} else {
					enableVRS = false;
					sprintf_s(gActionMessage, "VRS Not Supported on this GPU");
				}
				UpdateScrollList(0, 255, 255);
			}
			if (!vrsSupported) {
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::TextDisabled("(Unsupported)");
			}

			ImGui::TreePop();
		}

		ImGui::Spacing();

		if (ImGui::TreeNodeEx("System & Display", ImGuiTreeNodeFlags_DefaultOpen)) {
			// VSync Lock
			if (ImGui::Checkbox("VSync Lock [V]", &enableVsync)) {
				sprintf_s(gActionMessage, "VSync %s", enableVsync ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Camera Headbob
			if (ImGui::Checkbox("Camera Headbob [B]", &enableCameraBob)) {
				sprintf_s(gActionMessage, "Camera Bob %s", enableCameraBob ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Player HUD
			if (ImGui::Checkbox("Player HUD [H]", &enablePlayerHUD)) {
				sprintf_s(gActionMessage, "Player HUD %s", enablePlayerHUD ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Player Captions
			if (ImGui::Checkbox("Player Captions [.]", &enablePlayerCaptions)) {
				sprintf_s(gActionMessage, "Player Captions %s", enablePlayerCaptions ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			// Debug Stats
			if (ImGui::Checkbox("Debug Stats [F8]", &enableOnscreenDebug)) {
				sprintf_s(gActionMessage, "Onscreen Debug %s", enableOnscreenDebug ? "Enabled" : "Disabled");
				UpdateScrollList(0, 255, 255);
			}

			ImGui::TreePop();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("Use mouse or hotkeys [M,L,O,N,J,T,R,V,B,H,.,F8]");
	}
	ImGui::End();
}

double GameClockSeconds();

void DungeonStompApp::ScanMod(float fElapsedTime) {
	int i = 0;
	int j = 0;
	int gotone = 0;

	int counter = 0;
	float qdist = 0;
	LevelModTime = (float)GameClockSeconds();

	for (i = 0; i < totalmod; i++) {

		for (j = 0; j < num_monsters; j++) {
			if (monster_list[j].monsterid == levelmodify[counter].objectid &&
			    levelmodify[counter].active == 1) {
				// DropTreasure
				if (strstr(levelmodify[counter].Function, "DropTreasure") != NULL &&
				    levelmodify[counter].active == 1) {
					if (monster_list[j].bIsPlayerAlive == FALSE) {
						levelmodify[counter].active = 2;
						AddTreasureDrop(monster_list[j].x, monster_list[j].y, monster_list[j].z, atoi(levelmodify[counter].Text1));
					}
				}
				// MonsterActive
				if (strstr(levelmodify[counter].Function, "MonsterActive") != NULL &&
				    levelmodify[counter].active == 1) {
					if (monster_list[j].ability == 0) {
						if (gotone == 0) {
							// DisplayDialogText(levelmodify[counter].Text1, -20.0f);
							int len = (int)strlen(levelmodify[counter].Text1);
							len = len / 2;
							RenderText(arialFont, charToWChar(levelmodify[counter].Text1), XMFLOAT2(0.5f - (len * 0.005f), 0.48f), XMFLOAT2(0.20f, 0.20f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
						}
						gotone = 1;

						ScanModJump(levelmodify[counter].jump);
						if (countmodtime == 0) {
							LevelModLastTime = (float)GameClockSeconds();
							countmodtime = 1;
						}

						if (LevelModTime - LevelModLastTime >= 5.0f) {
							levelmodify[counter].active = 0;
							countmodtime = 0;
						}
					}
				}
				// XP
				if (strstr(levelmodify[counter].Function, "XPPoints") != NULL &&
				    levelmodify[counter].active == 1) {
					if (monster_list[j].bIsPlayerAlive == FALSE) {
						levelmodify[counter].active = 0;
						int xp = atoi(levelmodify[counter].Text1);
						sprintf_s(gActionMessage, "You got %d XP!.", xp);
						UpdateScrollList(255, 0, 255);
						player_list[trueplayernum].xp += xp;
						LevelUp(player_list[trueplayernum].xp);
					}
				}

				// SetHitPoints

				if (strstr(levelmodify[counter].Function, "SetHitPoints") != NULL &&
				    levelmodify[counter].active == 1) {

					levelmodify[counter].active = 0;
					monster_list[j].health = atoi(levelmodify[counter].Text1);
					monster_list[j].hp = atoi(levelmodify[counter].Text1);
				}

				// IsDeadText
				if (strstr(levelmodify[counter].Function, "IsDeadText") != NULL &&
				    levelmodify[counter].active == 1) {
					if (monster_list[j].bIsPlayerAlive == FALSE) {
						if (gotone == 0) {
							// DisplayDialogText(levelmodify[counter].Text1, -20.0f);
							int len = (int)strlen(levelmodify[counter].Text1);
							len = len / 2;
							RenderText(arialFont, charToWChar(levelmodify[counter].Text1), XMFLOAT2(0.5f - (len * 0.005f), 0.5f), XMFLOAT2(0.20f, 0.20f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
						}
						gotone = 1;

						ScanModJump(levelmodify[counter].jump);
						if (countmodtime == 0) {
							LevelModLastTime = (float)GameClockSeconds();
							countmodtime = 1;
						}

						if (LevelModTime - LevelModLastTime >= 5.0f) {
							levelmodify[counter].active = 0;
							countmodtime = 0;
						}
					}
				}
				// isNear
				if (strstr(levelmodify[counter].Function, "IsNear") != NULL &&
				    levelmodify[counter].active == 1) {
					qdist = FastDistance(
					    player_list[trueplayernum].x - monster_list[j].x,
					    player_list[trueplayernum].y - monster_list[j].y,
					    player_list[trueplayernum].z - monster_list[j].z);

					if (qdist < 300.0f) {
						if (gotone == 0) {
							// DisplayDialogText(levelmodify[counter].Text1, -20.0f);
							int len = (int)strlen(levelmodify[counter].Text1);
							len = len / 2;
							RenderText(arialFont, charToWChar(levelmodify[counter].Text1), XMFLOAT2(0.5f - (len * 0.005f), 0.5f), XMFLOAT2(0.20f, 0.20f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
						}
						gotone = 1;

						ScanModJump(levelmodify[counter].jump);
						if (countmodtime == 0) {
							LevelModLastTime = (float)GameClockSeconds();
							countmodtime = 1;
						}

						if (LevelModTime - LevelModLastTime >= 5.0f) {
							levelmodify[counter].active = 0;
							countmodtime = 0;
						}
					}
				}
			}
			// 	break;
		}

		for (j = 0; j < num_players2; j++) {
			if (player_list2[j].monsterid == levelmodify[counter].objectid &&
			    levelmodify[counter].active == 1) {
				// Moveup
				if (strstr(levelmodify[counter].Function, "MoveUp") != NULL &&
				    levelmodify[counter].active == 1) {

					if (levelmodify[counter].currentheight == -9999)
						levelmodify[counter].currentheight = player_list2[j].y;

					ScanModJump(levelmodify[counter].jump);
					if (player_list2[j].y - levelmodify[counter].currentheight <= atoi(levelmodify[counter].Text1)) {
						// portcullis speed
						player_list2[j].y = player_list2[j].y + (50.0f * fElapsedTime);
						qdist = FastDistance(
						    player_list[trueplayernum].x - player_list2[j].x,
						    player_list[trueplayernum].y - player_list2[j].y,
						    player_list[trueplayernum].z - player_list2[j].z);
						// closesoundid[3] = qdist;
					} else {
						levelmodify[counter].active = 0;
					}
				}
				// isswitch
				if (strstr(levelmodify[counter].Function, "IsSwitch") != NULL) {
					for (int t = 0; t < countswitches; t++) {
						if (j == switchmodify[t].num) {
							if (switchmodify[t].active == 2) {
								levelmodify[levelmodify[counter].jump - 1].active = 1;
							}
						}
					}
				}

				// isNear
				if (strstr(levelmodify[counter].Function, "IsNear") != NULL &&
				    levelmodify[counter].active == 1) {
					qdist = FastDistance(
					    player_list[trueplayernum].x - player_list2[j].x,
					    player_list[trueplayernum].y - player_list2[j].y,
					    player_list[trueplayernum].z - player_list2[j].z);

					if (qdist < 300.0f) {
						if (gotone == 0) {
							// DisplayDialogText(levelmodify[counter].Text1, -20.0f);
							int len = (int)strlen(levelmodify[counter].Text1);
							len = len / 2;
							RenderText(arialFont, charToWChar(levelmodify[counter].Text1), XMFLOAT2(0.5f - (len * 0.005f), 0.5f), XMFLOAT2(0.20f, 0.20f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
						}
						gotone = 1;

						ScanModJump(levelmodify[counter].jump);
						if (countmodtime == 0) {
							LevelModLastTime = (float)GameClockSeconds();
							countmodtime = 1;
						}

						if (LevelModTime - LevelModLastTime >= 5.0f) {
							levelmodify[counter].active = 0;
							countmodtime = 0;
						}
					}
				}
			}
		}

		for (j = 0; j < itemlistcount; j++) {
			if (item_list[j].monsterid == levelmodify[counter].objectid &&
			    levelmodify[counter].active == 1) {
				// Moveup
				if (strstr(levelmodify[counter].Function, "MoveUp") != NULL &&
				    levelmodify[counter].active == 1) {

					if (levelmodify[counter].currentheight == -9999)
						levelmodify[counter].currentheight = item_list[j].y;

					ScanModJump(levelmodify[counter].jump);
					if (item_list[j].y - levelmodify[counter].currentheight <= atoi(levelmodify[counter].Text1)) {
						item_list[j].y = item_list[j].y + (50.0f * fElapsedTime);

						qdist = FastDistance(
						    player_list[trueplayernum].x - item_list[j].x,
						    player_list[trueplayernum].y - item_list[j].y,
						    player_list[trueplayernum].z - item_list[j].z);
						// closesoundid[3] = qdist;
					} else {
						levelmodify[counter].active = 0;
					}
				}

				// TreasueAmount
				if (strstr(levelmodify[counter].Function, "TreasureAmount") != NULL &&
				    levelmodify[counter].active == 1) {
					levelmodify[counter].active = 0;

					item_list[j].gold = atoi(levelmodify[counter].Text1);
				}
			}
		}
		counter++;
	}
}

void DungeonStompApp::SetDungeonText() {

	for (int q = 0; q < oblist_length; q++) {
		int angle = (int)oblist[q].rot_angle;
		int ob_type = oblist[q].type;
		if (ob_type == 120) {
			float qdist = FastDistance(m_vEyePt.x - oblist[q].x, m_vEyePt.y - oblist[q].y, m_vEyePt.z - oblist[q].z);
			if (qdist < 500.0f) {
				if (strstr(oblist[q].name, "text") != NULL) {
					for (int il = 0; il < textcounter; il++) {
						if (gtext[il].textnum == q) {
							if (gtext[il].type == 0) {
								strcpy_s(gfinaltext, gtext[il].text);
							} else if (gtext[il].type == 1 || gtext[il].type == 2) {
								if (qdist < 200.0f) {

									// DisplayDialogText(gtext[il].text, 0.0f);
									// XMFLOAT2(0.5f, 0.0f), XMFLOAT4 color = XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f));

									int len = (int)strlen(gtext[il].text);
									len = len / 2;
									RenderText(arialFont, charToWChar(gtext[il].text), XMFLOAT2(0.5f - (len * 0.005f), 0.5f), XMFLOAT2(0.20f, 0.20f), XMFLOAT2(0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
								}
							}
						}
					}
				}
			}
		}
	}
}

void ConvertQuad(int fan_cnt);

void DungeonStompApp::DisplayPlayerCaption() {

	int i;
	float pangle = 0;
	int countit = 0;
	int cullloop = 0;
	int cullflag = 0;
	int num = 0;
	int len = 0;
	int count = 0;
	char junk2[2000];
	int flag = 1;
	float yadjust = 0;

	int j = 0;

	int totalcount = 0;
	displayCapture = 0;

	if (!enablePlayerCaptions)
		return;

	if (enableDXR && !enablePlayerHUD)
		return;

	ObjectsToDraw[number_of_polys_per_frame].srcstart = cnt;
	ObjectsToDraw[number_of_polys_per_frame].objectId = -99;
	ObjectsToDraw[number_of_polys_per_frame].srcfstart = 0;

	ObjectsToDraw[number_of_polys_per_frame].vert_index = number_of_polys_per_frame;
	ObjectsToDraw[number_of_polys_per_frame].texture = 378;
	ObjectsToDraw[number_of_polys_per_frame].vertsperpoly = 3;
	ObjectsToDraw[number_of_polys_per_frame].facesperpoly = 1;

	texture_list_buffer[number_of_polys_per_frame] = 378; // 263

	int fan_cnt = cnt;

	for (j = 0; j < num_monsters; j++) {
		cullflag = 0;
		for (cullloop = 0; cullloop < monstercount; cullloop++) {
			if (monstercull[cullloop] == monster_list[j].monsterid) {
				cullflag = 1;
				break;
			}
		}

		flag = 1;
		num = 0;
		count = 0;
		yadjust = 0.0f;

		if (monster_list[j].bIsPlayerValid && cullflag == 1 && monster_list[j].bStopAnimating == FALSE) {
			len = (int)strlen(monster_list[j].chatstr);

			while (flag) {
				count = 0;

				while (monster_list[j].chatstr[num] != '!') {

					junk2[count] = monster_list[j].chatstr[num];
					count++;
					num++;
					if (num >= len)
						break;
				}
				if (monster_list[j].chatstr[num] == '!')
					num++;

				junk2[count] = '\0';

				if (num >= len || len == 0)
					flag = 0;

				float x = monster_list[j].x;
				float y = (monster_list[j].y + monster_list[j].captionheight) - 70.0f - yadjust;
				float z = monster_list[j].z;

				yadjust += 6.0f;

				countdisplay = 0;

				display_font(0.0f, 0.0f, junk2, 255, 255, 0);

				XMFLOAT3 normroadold;
				XMFLOAT3 vw1, vw2;

				normroadold.x = 50;
				normroadold.y = 0;
				normroadold.z = 0;

				vw1.x = m_vEyePt.x;
				vw1.y = m_vEyePt.y;
				vw1.z = m_vEyePt.z;

				vw2.x = x;
				vw2.y = y;
				vw2.z = z;

				XMVECTOR vDiff = XMLoadFloat3(&vw1) - XMLoadFloat3(&vw2);
				XMVECTOR final = XMVector3Normalize(vDiff);
				XMVECTOR final2 = XMVector3Normalize(XMLoadFloat3(&normroadold));

				float fDot = XMVectorGetX(XMVector3Dot(final, final2));

				float convangle;
				convangle = (float)acos(fDot) / k;

				fDot = convangle;

				if (vw2.z < vw1.z) {
					fDot = -1.0f * (180.0f - fDot) + 90.0f;
				} else {
					fDot = 90.0f + (180.0f - fDot);
				}

				if ((vw2.x < vw1.x) && (vw2.z < vw1.z)) {
					fDot = fixangle(fDot, 360.0f);
				}

				// float cosine = cos_table[(int)fDot];
				// float sine = sin_table[(int)fDot];

				float cosine = (float)cos(fDot * k);
				float sine = (float)sin(fDot * k);

				if (enableDXR) {
					// DXR consumes triangle lists, so expand each 4 vertex glyph strip into two
					// triangles wound clockwise as seen from the camera (back faces are culled).
					for (int q = 0; q + 3 < countdisplay && cnt + 6 < MAX_NUM_QUADS; q += 4) {
						XMFLOAT3 p[4];
						for (int k = 0; k < 4; k++) {
							p[k].x = x + (bubble[q + k].x * cosine - bubble[q + k].z * sine);
							p[k].y = y + bubble[q + k].y;
							p[k].z = z + (bubble[q + k].x * sine + bubble[q + k].z * cosine);
						}

						XMVECTOR p0 = XMLoadFloat3(&p[0]);
						XMVECTOR faceNormal = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&p[1]) - p0, XMLoadFloat3(&p[2]) - p0));
						bool facesCamera = XMVectorGetX(XMVector3Dot(faceNormal, XMLoadFloat3(&vw1) - p0)) >= 0.0f;
						if (!facesCamera)
							faceNormal = -faceNormal;

						XMFLOAT3 n;
						XMStoreFloat3(&n, faceNormal);

						static const int frontOrder[6] = { 0, 1, 2, 2, 1, 3 };
						static const int backOrder[6] = { 0, 2, 1, 2, 3, 1 };
						const int *order = facesCamera ? frontOrder : backOrder;

						for (int k = 0; k < 6; k++) {
							int idx = order[k];
							memset(&src_v[cnt], 0, sizeof(D3DVERTEX2));
							src_v[cnt].x = p[idx].x;
							src_v[cnt].y = p[idx].y;
							src_v[cnt].z = p[idx].z;
							src_v[cnt].tu = bubble[q + idx].tu;
							src_v[cnt].tv = bubble[q + idx].tv;
							src_v[cnt].nx = n.x;
							src_v[cnt].ny = n.y;
							src_v[cnt].nz = n.z;
							src_v[cnt].CastShadow = 0;
							totalcount++;
							cnt++;
						}
					}
					continue;
				}

				displayCaptureIndex[displayCapture] = cnt;
				;
				displayCaptureCount[displayCapture] = countdisplay / 4;

				for (i = 0; i < ((countdisplay)); i += 1) {
					src_v[cnt].x = bubble[i].x;
					src_v[cnt].y = bubble[i].y;
					src_v[cnt].z = bubble[i].z;

					src_v[cnt].tu = bubble[i].tu;
					src_v[cnt].tv = bubble[i].tv;

					totalcount++;
					cnt++;
				}

				for (i = displayCaptureIndex[displayCapture]; i < cnt; i += 1) {
					float x2 = src_v[i].x;
					float y2 = src_v[i].y;
					float z2 = src_v[i].z;

					float wx = x, wy = y, wz = z;

					src_v[i].x = wx + (x2 * cosine - z2 * sine);
					src_v[i].y = wy + y2;
					src_v[i].z = wz + (x2 * sine + z2 * cosine);
				}

				displayCapture++;
			}
		}
	}

	int test = (totalcount / 4) * 6;
	if (enableDXR) {
		// Captions are already emitted as triangle lists; register them as a regular draw object
		// so the DXR path assigns the font texture to these triangles.
		displayCapture = 0;
		if (totalcount > 0) {
			int slot = number_of_polys_per_frame;
			ObjectsToDraw[slot].castshaddow = 0;
			ObjectsToDraw[slot].vertsperpoly = totalcount;
			ObjectsToDraw[slot].facesperpoly = totalcount / 3;
			verts_per_poly[slot] = totalcount;
			dp_command_index_mode[slot] = 1;
			dp_commands[slot] = D3DPT_TRIANGLELIST;
			number_of_polys_per_frame++;
		}
		return;
	}

	verts_per_poly[number_of_polys_per_frame] = test;
	dp_command_index_mode[number_of_polys_per_frame] = 1;
	dp_commands[number_of_polys_per_frame] = D3DPT_TRIANGLELIST;

	// number_of_polys_per_frame++;

	// ConvertQuad(fan_cnt);
}

// Ink bounds (left, right) in pixels inside the 16x16 cell of fontB for ASCII 32..126.
// Used to lay captions out with proportional spacing instead of a fixed 16px advance.
static const unsigned char kFontInk[95][2] = {
	{0,0}, {8,10}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {7,11},
	{6,11}, {7,12}, {5,13}, {6,12}, {7,11}, {6,12}, {8,10}, {5,12},
	{5,13}, {6,11}, {5,13}, {5,13}, {5,13}, {5,13}, {5,12}, {5,13},
	{5,13}, {5,12}, {8,10}, {7,10}, {6,12}, {5,13}, {6,12}, {5,13},
	{5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {5,12}, {5,12}, {5,13},
	{5,13}, {7,11}, {5,13}, {5,13}, {5,12}, {5,13}, {5,13}, {5,13},
	{5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13},
	{5,13}, {5,13}, {5,13}, {7,12}, {5,12}, {6,11}, {5,12}, {5,13},
	{7,11}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13}, {5,13},
	{5,13}, {8,10}, {5,11}, {5,13}, {7,10}, {5,13}, {5,13}, {5,13},
	{5,13}, {5,13}, {5,13}, {5,13}, {5,12}, {5,13}, {5,13}, {5,13},
	{5,13}, {5,13}, {5,13}, {6,12}, {8,10}, {6,12}, {5,12},
};

// Builds one camera-facing quad (4 vertices, triangle strip order) per visible glyph into bubble[],
// centered on x. Glyphs are cropped to their ink bounds and spaced proportionally so the caption
// reads as modern, compact text rather than a wide monospaced bitmap font.
void display_font(float x, float y, char text[1000], int r, int g, int b) {

	const float cellPx = 16.0f;
	const float texPx = 256.0f;
	const float fontsize = 3.4f; // world size of one 16px cell (height of every glyph quad)
	const float pxToWorld = fontsize / cellPx;
	const float padPx = 1.0f;      // bilinear/mip safety margin around the ink
	const float trackingPx = 1.0f; // extra gap between glyphs
	const float spacePx = 5.0f;
	const int maxGlyphs = 600 / 4;

	struct Glyph {
		float u0, u1;       // texture range, left to right in reading order
		float advance;      // in pixels
		float quadWidth;    // in pixels
		bool visible;
	} glyphs[maxGlyphs];

	y = 40.0f; // caption baseline offset above the monster anchor, matches the original placement
	int textlen = (int)strlen(text);
	if (textlen > maxGlyphs)
		textlen = maxGlyphs;

	float totalPx = 0.0f;

	for (int i = 0; i < textlen; i++) {
		unsigned char c = (unsigned char)text[i];
		Glyph &gl = glyphs[i];
		gl.visible = true;

		int col = 0;
		int row = 0;
		float inkL = 0.0f;
		float inkR = cellPx;
		float pad = 0.0f;
		float tracking = trackingPx;

		if (c == '|' || c == '`') {
			// Solid color swatch cells (health / max health bar segments) at the top-left of the font
			// sheet. Segments butt against each other with no gap so the run reads as one continuous
			// bar; the half pixel inset keeps bilinear sampling from bleeding in the neighbouring cell.
			col = (c == '|') ? 0 : 1;
			row = 0;
			inkL = 0.5f;
			inkR = cellPx - 0.5f;
			tracking = 0.0f;
		} else if (c >= 33 && c <= 126) {
			col = c % 16;
			row = c / 16;
			inkL = (float)kFontInk[c - 32][0];
			inkR = (float)kFontInk[c - 32][1];
			pad = padPx;
		} else {
			gl.visible = false;
		}

		if (!gl.visible) {
			gl.u0 = gl.u1 = 0.0f;
			gl.quadWidth = 0.0f;
			gl.advance = spacePx;
		} else {
			float left = (inkL - pad) > 0.0f ? (inkL - pad) : 0.0f;
			float right = (inkR + pad) < cellPx ? (inkR + pad) : cellPx;
			gl.u0 = (col * cellPx + left) / texPx;
			gl.u1 = (col * cellPx + right) / texPx;
			gl.quadWidth = right - left;
			gl.advance = gl.quadWidth + tracking;
		}

		totalPx += gl.advance;
	}

	float cursorPx = -totalPx * 0.5f;
	float vTop = 0.0f;
	float vBottom = 0.0f;

	countdisplay = 0;

	for (int i = 0; i < textlen; i++) {
		const Glyph &gl = glyphs[i];
		float startPx = cursorPx;
		cursorPx += gl.advance;

		if (!gl.visible)
			continue;

		unsigned char c = (unsigned char)text[i];
		int row = (c == '|' || c == '`') ? 0 : (c / 16);
		vTop = (row * cellPx) / texPx;
		vBottom = ((row + 1) * cellPx) / texPx;

		// The caption quad is rotated to face the camera, which mirrors its local x axis,
		// so the string is laid out in reverse along +x and the u range is flipped to compensate.
		float xMin = x - (startPx + gl.quadWidth) * pxToWorld;
		float xMax = x - startPx * pxToWorld;

		bubble[countdisplay].x = xMin;
		bubble[countdisplay].y = y;
		bubble[countdisplay].z = 0;
		bubble[countdisplay].tu = gl.u1;
		bubble[countdisplay].tv = vTop;
		countdisplay++;

		bubble[countdisplay].x = xMin;
		bubble[countdisplay].y = y - fontsize;
		bubble[countdisplay].z = 0;
		bubble[countdisplay].tu = gl.u1;
		bubble[countdisplay].tv = vBottom;
		countdisplay++;

		bubble[countdisplay].x = xMax;
		bubble[countdisplay].y = y;
		bubble[countdisplay].z = 0;
		bubble[countdisplay].tu = gl.u0;
		bubble[countdisplay].tv = vTop;
		countdisplay++;

		bubble[countdisplay].x = xMax;
		bubble[countdisplay].y = y - fontsize;
		bubble[countdisplay].z = 0;
		bubble[countdisplay].tu = gl.u0;
		bubble[countdisplay].tv = vBottom;
		countdisplay++;
	}
}
