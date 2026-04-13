
#include "stdafx.h"  
#include "PythonLeaderboard.h"
#include "PythonPlayer.h"
//#include "AbstractSingleton.h"

#include "../Render/StateManager.h"  


#include <wrl/client.h>  
#include <algorithm>
#include <fstream>
#include <string>
using Microsoft::WRL::ComPtr;


#ifdef LEADERBOARD_RAZOR93

CPythonLeaderboard::CPythonLeaderboard() = default;
CPythonLeaderboard::~CPythonLeaderboard() = default;


void CPythonLeaderboard::OnDeviceCreate(LPDIRECT3DDEVICE9 dev)
{
	if (!dev)
	{
		return;
	}

	if (m_tex || m_texskillmob || m_font)
	{
		return;
	}
	m_deviceLost = false;
	ResetBindings();
}

void CPythonLeaderboard::OnDeviceLost()
{
	if (m_font) { m_font->Release(); m_font = nullptr; }
	if (m_tex) { m_tex->Release();  m_tex = nullptr; }
	if (m_texskillmob) { m_texskillmob->Release(); m_texskillmob = nullptr; }
	if (m_texguild) { m_texguild->Release(); m_texguild = nullptr; }
	m_deviceLost = true;
}

void CPythonLeaderboard::OnDeviceReset(LPDIRECT3DDEVICE9 dev)
{
	if (!dev) { TraceError("[LB] Reset: dev NULL"); return; }

	if (m_deviceLost)
	{
		m_deviceLost = false;
		ResetBindings();
	}

	if (!m_tex) {
		dev->CreateTexture(
			W,
			H,
			1,
			D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			&m_tex,
			nullptr
		);

	}

	if (!m_texskillmob) {
		dev->CreateTexture(
			W,
			H,
			1,
			D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			&m_texskillmob,
			nullptr
		);
		if (!m_texguild) {
			dev->CreateTexture(
				W,
				H,
				1,
				D3DUSAGE_RENDERTARGET,
				D3DFMT_A8R8G8B8,
				D3DPOOL_DEFAULT,
				&m_texguild,
				nullptr
			);
		}
	}
	if (!m_font) {
		D3DXCreateFontA(
			dev,
			40,
			0,
			FW_BOLD,
			1,
			FALSE,
			DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,
			DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			"Arial",
			&m_font
		);

	}



}

void CPythonLeaderboard::OnDeviceDestroy()
{
	OnDeviceLost();
	ResetBindings();
}

void CPythonLeaderboard::ResetBindings()
{
	sBoundBillboard = false;
	sBoundMobSkill = false;
	sBoundGuild = false;
}


bool CPythonLeaderboard::Ensure() const
{
	LPDIRECT3DDEVICE9 dev = CGraphicBase::GetD3DDevice();
	if (!dev) { TraceError("[LB] Ensure: dev=NULL"); return false; }

	const bool ok = (m_font && m_tex && m_texskillmob && m_texguild);
	return ok;
}

void CPythonLeaderboard::SetTextGuild(const char* data)
{
	m_GuildLines.clear();
	std::istringstream iss(data);
	std::string line;
	while (std::getline(iss, line))
	{
		if (!line.empty())
			m_GuildLines.push_back(line);
	}
}

void CPythonLeaderboard::SetText(const char* data)
{
	m_leaderboardLines.clear();
	std::istringstream iss(data);
	std::string line;
	while (std::getline(iss, line))
	{
		if (!line.empty())
			m_leaderboardLines.push_back(line);
	}
}

void CPythonLeaderboard::DrawGuild() const
{
	if (!Ensure())
		return;

	auto dev = CGraphicBase::GetD3DDevice();
	if (!dev || !m_font || !m_texguild)
		return;

	ComPtr<IDirect3DSurface9> rt, old;
	if (FAILED(m_texguild->GetSurfaceLevel(0, rt.GetAddressOf())) || !rt) return;
	if (FAILED(dev->GetRenderTarget(0, old.GetAddressOf()))) return;
	if (FAILED(dev->SetRenderTarget(0, rt.Get()))) return;

	dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(180, 20, 20, 20), 1.f, 0);

	// ----------------------------
	// OSZLOP BEALLITASOK
	//  - rövidebb headerek: W / D / L / S
	//  - a stat blokk keskenyebb, hogy a Leader kapjon több helyet
	//  - jobb szélhez anchorolva, nem vágja le
	// ----------------------------
	const int cellW = 56;   // kisebb cella -> több hely balra (Leadernek)
	const int colGap = 78;   // kisebb gap -> stat oszlopok közelebb, több hely a Leadernek
	const int rightPad = 45;   // kisebb = jobbra megy a blokk, nagyobb = balra

	// Jobbról balra: SCORE | LOSS | DRAW | WIN
	const int colScoreR = W - rightPad;
	const int colLossR = colScoreR - colGap;
	const int colDrawR = colLossR - colGap;
	const int colWinR = colDrawR - colGap;

	const int colScoreL = colScoreR - cellW;
	const int colLossL = colLossR - cellW;
	const int colDrawL = colDrawR - cellW;
	const int colWinL = colWinR - cellW;

	// Leader oszlop jobb széle a stat blokk elõtt
	int leaderRight = colWinL - 18;
	if (leaderRight < 560) // safety
		leaderRight = 560;

	// Színek
	const D3DCOLOR winColor = D3DCOLOR_XRGB(0, 255, 0);       // zöld
	const D3DCOLOR drawColor = D3DCOLOR_XRGB(255, 255, 0);     // sárga
	const D3DCOLOR lossColor = D3DCOLOR_XRGB(255, 0, 0);       // piros
	const D3DCOLOR scoreColor = D3DCOLOR_XRGB(200, 220, 255);   // világos (score)

	// Cím
	RECT titleRect = { 0, 60, W, 100 };
	D3DRECT titleBgRect = { 0, 60, W, 100 };
	dev->Clear(1, &titleBgRect, D3DCLEAR_TARGET, D3DCOLOR_XRGB(25, 60, 180), 1.f, 0);

	m_font->DrawTextA(nullptr,
		"AURIGA \x95 GUILD RANKING",
		-1, &titleRect, DT_CENTER | DT_VCENTER,
		D3DCOLOR_XRGB(255, 255, 255));

	// Fejléc bal oldali rész
	RECT headerLeftRect = { 60, 120, leaderRight, 150 };
	m_font->DrawTextA(nullptr,
		"#   Guild                         Leader",
		-1, &headerLeftRect, 0, D3DCOLOR_XRGB(255, 180, 20));

	// Fejléc stat oszlopok (pontosan középre, rövid feliratok)
	RECT hWin = { colWinL,    120, colWinR,    150 };
	RECT hDraw = { colDrawL,   120, colDrawR,   150 };
	RECT hLoss = { colLossL,   120, colLossR,   150 };
	RECT hScore = { colScoreL,  120, colScoreR,  150 };

	m_font->DrawTextA(nullptr, "W", -1, &hWin, DT_CENTER | DT_VCENTER, winColor);
	m_font->DrawTextA(nullptr, "D", -1, &hDraw, DT_CENTER | DT_VCENTER, drawColor);
	m_font->DrawTextA(nullptr, "L", -1, &hLoss, DT_CENTER | DT_VCENTER, lossColor);
	m_font->DrawTextA(nullptr, "S", -1, &hScore, DT_CENTER | DT_VCENTER, scoreColor);

	constexpr int startY = 160;
	constexpr int rowHeight = 36;

	for (size_t i = 0; i < m_GuildLines.size() && i < 10; ++i)
	{
		char guildBuf[64] = { 0 };
		char masterBuf[64] = { 0 };
		int win = 0, draw = 0, loss = 0;

		int scanned = sscanf(m_GuildLines[i].c_str(), "%63[^;];%63[^;];%d;%d;%d",
			guildBuf, masterBuf, &win, &draw, &loss);
		if (scanned < 5) continue;

		const int score = win - loss;

		int top = startY + static_cast<int>(i) * rowHeight;
		int bottom = top + rowHeight - 6;

		D3DCOLOR bgColor = D3DCOLOR_XRGB(30, 30, 30);
		D3DRECT bgRect = { 50, top, W - 50, bottom };
		dev->Clear(1, &bgRect, D3DCLEAR_TARGET, bgColor, 1.f, 0);

		// Bal oldali (rank/guild/leader) top3 kiemelés
		D3DCOLOR textColor = D3DCOLOR_XRGB(255, 255, 255);
		if (i == 0) textColor = D3DCOLOR_XRGB(255, 215, 0);
		else if (i == 1) textColor = D3DCOLOR_XRGB(192, 192, 192);
		else if (i == 2) textColor = D3DCOLOR_XRGB(205, 127, 50);

		// rank
		RECT r = { 60, top, 100, bottom };
		char bufRank[8];
		_snprintf_s(bufRank, sizeof(bufRank), "%zu.", i + 1);
		m_font->DrawTextA(nullptr, bufRank, -1, &r, DT_LEFT | DT_VCENTER, textColor);

		// guild
		r = { 100, top, 410, bottom };
		m_font->DrawTextA(nullptr, guildBuf, -1, &r, DT_LEFT | DT_VCENTER, textColor);

		// leader (kapott több helyet)
		r = { 420, top, leaderRight, bottom };
		m_font->DrawTextA(nullptr, masterBuf, -1, &r, DT_LEFT | DT_VCENTER, textColor);

		// stat cellák (ugyanaz a rect, mint a fejléc -> pontosan alatta középen)
		RECT rWin = { colWinL,   top, colWinR,   bottom };
		RECT rDraw = { colDrawL,  top, colDrawR,  bottom };
		RECT rLoss = { colLossL,  top, colLossR,  bottom };
		RECT rScore = { colScoreL, top, colScoreR, bottom };

		char b[16];

		_snprintf_s(b, sizeof(b), "%d", win);
		m_font->DrawTextA(nullptr, b, -1, &rWin, DT_CENTER | DT_VCENTER, winColor);

		_snprintf_s(b, sizeof(b), "%d", draw);
		m_font->DrawTextA(nullptr, b, -1, &rDraw, DT_CENTER | DT_VCENTER, drawColor);

		_snprintf_s(b, sizeof(b), "%d", loss);
		m_font->DrawTextA(nullptr, b, -1, &rLoss, DT_CENTER | DT_VCENTER, lossColor);

		_snprintf_s(b, sizeof(b), "%d", score);
		m_font->DrawTextA(nullptr, b, -1, &rScore, DT_CENTER | DT_VCENTER, scoreColor);
	}

	dev->SetRenderTarget(0, old.Get());
}

void CPythonLeaderboard::DrawRect(IDirect3DDevice9* dev, int x1, int y1, int x2, int y2,D3DCOLOR color)
{
	if (!dev) return;
	const D3DRECT r = { LONG(x1), LONG(y1), LONG(x2), LONG(y2) };
	dev->Clear(1, &r, D3DCLEAR_TARGET, color, 1.0f, 0);

}

void CPythonLeaderboard::DrawTest() const
{
	auto dev = CGraphicBase::GetD3DDevice();
	if (!dev || !m_font || !m_tex) return;

	ComPtr<IDirect3DSurface9> rt, old;
	if (FAILED(m_tex->GetSurfaceLevel(0, rt.GetAddressOf())) || !rt) return;
	if (FAILED(dev->GetRenderTarget(0, old.GetAddressOf()))) return;
	if (FAILED(dev->SetRenderTarget(0, rt.Get()))) return;

	// ---------- CÍM ----------
	RECT titleRect = { 0, 60, W, 100 };

	// háttér téglalap (kék)
	D3DRECT titleBgRect = { 0, 60, W, 100 };
	dev->Clear(1, &titleBgRect, D3DCLEAR_TARGET, D3DCOLOR_XRGB(25, 60, 180), 1.f, 0);

	// szöveg rajzolás a háttér fölé
	m_font->DrawTextA(nullptr,
		"AURIGA \x95 LEADERBOARD \x95 BY METINSTONE",
		-1, &titleRect, DT_CENTER | DT_VCENTER,
		D3DCOLOR_XRGB(255, 255, 255)); // fehér szöveg
	// ---------- FEJLÉC ----------
	RECT headerRect = { 60, 120, W - 60, 150 };
	m_font->DrawTextA(nullptr, "#   Name                      Lv       Killed Metins         Boss DMG",
		-1, &headerRect, 0, D3DCOLOR_XRGB(255, 180, 20));

	constexpr int startY = 160;
	constexpr int rowHeight = 36;

	if (m_leaderboardLines.empty()) {
		RECT errRect = { 60, startY, W - 60, startY + 30 };
		m_font->DrawTextA(nullptr, "No leaderboard data received", -1, &errRect, 0, D3DCOLOR_XRGB(255, 0, 0));
	}
	else {
		const std::string myName = CPythonPlayer::Instance().GetName();

		for (size_t i = 0; i < m_leaderboardLines.size() && i < 10; ++i)
		{
			std::string name;
			int lv = 0, metins = 0, dmg = 0;

			const std::string& line = m_leaderboardLines[i];
			char nameBuf[64] = { 0 };
			int scanned = sscanf(line.c_str(), "%63[^;];%d;%d;%d", nameBuf, &lv, &metins, &dmg);
			if (scanned < 4) continue;

			name = nameBuf;
			int top = startY + static_cast<int>(i) * rowHeight;
			int bottom = top + rowHeight - 6;

			// sor háttér
			D3DCOLOR bgColor = (name == myName) ? D3DCOLOR_XRGB(0, 100, 0) : D3DCOLOR_XRGB(30, 30, 30);
			D3DRECT bgRect = { 50, top, W - 50, bottom };
			dev->Clear(1, &bgRect, D3DCLEAR_TARGET, bgColor, 1.f, 0);

			// szín
			D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255);
			if (i == 0) color = D3DCOLOR_XRGB(255, 215, 0);
			else if (i == 1) color = D3DCOLOR_XRGB(192, 192, 192);
			else if (i == 2) color = D3DCOLOR_XRGB(205, 127, 50);

			// rank
			RECT colRect = { 60, top, 100, bottom };
			char bufRank[8];
			_snprintf_s(bufRank, sizeof(bufRank), "%zu.", i + 1);
			m_font->DrawTextA(nullptr, bufRank, -1, &colRect, DT_LEFT | DT_VCENTER, color);

			// name
			colRect = {.left = 100, .top = top, .right = 360, .bottom = bottom };
			m_font->DrawTextA(nullptr, name.c_str(), -1, &colRect, DT_LEFT | DT_VCENTER, color);

			// lv
			char bufLv[16]; _snprintf_s(bufLv, sizeof(bufLv), "%d", lv);
			colRect = {.left = 390, .top = top, .right = 440, .bottom = bottom };
			m_font->DrawTextA(nullptr, bufLv, -1, &colRect, DT_RIGHT | DT_VCENTER, color);

			// metins
			char bufMetins[32]; _snprintf_s(bufMetins, sizeof(bufMetins), "%d", metins);
			colRect = {.left = 480, .top = top, .right = 650, .bottom = bottom };
			m_font->DrawTextA(nullptr, bufMetins, -1, &colRect, DT_RIGHT | DT_VCENTER, color);

			// dmg
			char bufDmg[32]; _snprintf_s(bufDmg, sizeof(bufDmg), "%d", dmg);
			colRect = {.left = 770, .top = top, .right = 920, .bottom = bottom };
			m_font->DrawTextA(nullptr, bufDmg, -1, &colRect, DT_RIGHT | DT_VCENTER, color);
		}
	}

	dev->SetRenderTarget(0, old.Get());

}

void CPythonLeaderboard::SetTextSkillMob(const char* data)
{
	m_MobSkillLines.clear();
	std::istringstream iss(data);
	std::string line;
	while (std::getline(iss, line))
	{
		if (!line.empty())
			m_MobSkillLines.push_back(line);
	}
}

void CPythonLeaderboard::DrawRectSkillMob(IDirect3DDevice9* dev, int x1, int y1, int x2, int y2, D3DCOLOR color)
{
	if (!dev) return;
	const D3DRECT r = { LONG(x1), LONG(y1), LONG(x2), LONG(y2) };
	dev->Clear(1, &r, D3DCLEAR_TARGET, color, 1.0f, 0);

}

void CPythonLeaderboard::DrawSkillMob() const
{
	if (!Ensure())
		return;

	auto dev = CGraphicBase::GetD3DDevice();
	if (!dev) return;

	ComPtr<IDirect3DSurface9> rt, old;
	if (!m_texskillmob) return;
	if (FAILED(m_texskillmob->GetSurfaceLevel(0, rt.GetAddressOf())) || !rt) return;
	if (FAILED(dev->GetRenderTarget(0, old.GetAddressOf()))) return;
	if (FAILED(dev->SetRenderTarget(0, rt.Get()))) return;


	dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(180, 30, 20, 20), 1.f, 0);

	if (!m_font) {
		TraceError("[LB] DrawSkillMob(): m_font NULL");
		dev->SetRenderTarget(0, old.Get());
		return;
	}

	// ---------- háttér tábla keret ----------
	int rows = (int)m_MobSkillLines.size();
	rows = min(rows, 10);
	constexpr int rowHeight1 = 28;
	int totalHeight1 = 150 + rows * rowHeight1;
	DrawRectSkillMob(dev, 50, 80, 950, 80 + totalHeight1, D3DCOLOR_ARGB(180, 20, 20, 20));

	// ---------- cím ----------
	RECT titleRect = { 0, 60, W, 100 };

	// címkék háttér (kék csík)
	D3DRECT titleBgRect = { 0, 60, W, 100 };
	dev->Clear(1, &titleBgRect, D3DCLEAR_TARGET, D3DCOLOR_XRGB(25, 60, 180), 1.f, 0);

	m_font->DrawTextA(nullptr,
		"AURIGA \x95 SKILL DAMAGE RANKING",
		-1, &titleRect, DT_CENTER | DT_VCENTER,
		D3DCOLOR_XRGB(255, 215, 205));


 // ---------- fejléc ----------
	RECT headerRect = { 60, 120, W - 60, 150 };
	m_font->DrawTextA(nullptr,
		"#        Name                  Lv       Victim                 Skill DMG",
		-1, &headerRect, 0, D3DCOLOR_XRGB(250, 180, 120));

	// ---------- sorok ----------
	constexpr int startY = 160;
	constexpr int rowHeight = 36;

	if (m_MobSkillLines.empty()) {
		RECT errRect = { 60, startY, W - 60, startY + 30 };
		m_font->DrawTextA(nullptr, "No skill mob data received", -1, &errRect, 0, D3DCOLOR_XRGB(255, 0, 0));
	}
	else {
		const std::string myName = CPythonPlayer::Instance().GetName();

		for (size_t i = 0; i < m_MobSkillLines.size() && i < 10; ++i)
		{
			std::string name, victim;
			int lv = 0, dmg = 0;

			const std::string& line = m_MobSkillLines[i];
			char nameBuf[64] = { 0 };
			char victimBuf[64] = { 0 };

			// ÚJ FORMÁTUM: name;level;victim;dmg
			int scanned = sscanf(line.c_str(), "%63[^;];%d;%63[^;];%d", nameBuf, &lv, victimBuf, &dmg);
			if (scanned < 4) continue;

			name = nameBuf;
			victim = victimBuf;

			int top = startY + static_cast<int>(i) * rowHeight;
			int bottom = top + rowHeight - 6;

			// saját sor kiemelés
			D3DCOLOR bgColor = (name == myName) ? D3DCOLOR_XRGB(0, 100, 0) : D3DCOLOR_XRGB(30, 30, 30);
			D3DRECT bgRect = { 50, top, W - 50, bottom };
			dev->Clear(1, &bgRect, D3DCLEAR_TARGET, bgColor, 1.f, 0);

			// színek
			D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255);
			if (i == 0) color = D3DCOLOR_XRGB(255, 215, 0);
			else if (i == 1) color = D3DCOLOR_XRGB(192, 192, 192);
			else if (i == 2) color = D3DCOLOR_XRGB(205, 127, 50);
			if (name == myName) color = D3DCOLOR_XRGB(255, 255, 255);

			// --- oszlop kiosztás ---
			// Rank
			RECT colRect = { 60, top, 100, bottom };
			char bufRank[8];
			_snprintf_s(bufRank, sizeof(bufRank), "%zu.", i + 1);
			m_font->DrawTextA(nullptr, bufRank, -1, &colRect, DT_LEFT | DT_VCENTER, color);

			// Name
			colRect = {.left = 100, .top = top, .right = 370, .bottom = bottom };
			m_font->DrawTextA(nullptr, name.c_str(), -1, &colRect, DT_LEFT | DT_VCENTER, color);

			// Lv
			char bufLv[16]; _snprintf_s(bufLv, sizeof(bufLv), "%d", lv);
			colRect = {.left = 400, .top = top, .right = 440, .bottom = bottom };
			m_font->DrawTextA(nullptr, bufLv, -1, &colRect, DT_RIGHT | DT_VCENTER, color);

			// Victim (új oszlop)
			colRect = {.left = 490, .top = top, .right = 700, .bottom = bottom };
			m_font->DrawTextA(nullptr, victim.c_str(), -1, &colRect, DT_LEFT | DT_VCENTER, color);

			// Skill DMG
			char bufDmg[32]; _snprintf_s(bufDmg, sizeof(bufDmg), "%d", dmg);
			colRect = {.left = 770, .top = top, .right = 920, .bottom = bottom };
			m_font->DrawTextA(nullptr, bufDmg, -1, &colRect, DT_RIGHT | DT_VCENTER, color);
		}
	}

	dev->SetRenderTarget(0, old.Get());
}


void CPythonLeaderboard::OnLeaderBoardRender()
{
	auto& LB = Instance();
	auto dev = CGraphicBase::GetD3DDevice();
	if (LB.m_deviceLost) { return; }

	/*if (!HasInstance())
	{
		TraceError("[LB] OnRender: HasInstance()==false -> creating now");
		CreateSingleton();
	}*/


	if (!dev)
		LB.ResetBindings();


	if (!LB.Ensure())
	{
		LB.OnDeviceCreate(dev);
		LB.OnDeviceReset(dev);
		if (!LB.Ensure())
			return;
	}



	if (!sBoundBillboard) {

		LB.DrawTest();
		const bool ok = ReplaceTextureGlobalByFilenameLoose("d:/ymir work/razor93/body.png", LB.GetTex());
		if (ok) sBoundBillboard = true;

	}

	if (!sBoundMobSkill) {

		LB.DrawSkillMob();
		const bool ok1 = ReplaceTextureGlobalByFilenameLoose("d:/ymir work/razor93/image0.tga", LB.GetTexSkillMob());
		if (ok1) sBoundMobSkill = true;

	}

	if (!sBoundGuild) {
		LB.DrawGuild();
		const bool ok2 = ReplaceTextureGlobalByFilenameLoose("d:/ymir work/razor93/clan.tga", LB.GetTexGuild());
		if (ok2) sBoundGuild = true;
	}
}

#endif