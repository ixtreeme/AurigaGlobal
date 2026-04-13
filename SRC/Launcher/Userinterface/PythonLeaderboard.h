#pragma once
#include "AbstractSingleton.h"
#include <d3d9.h>

#ifdef LEADERBOARD_RAZOR93


class CPythonLeaderboard : public TAbstractSingleton<CPythonLeaderboard>
{
public:
    void SetTextGuild(const char* data);
    void DrawGuild() const;
    LPDIRECT3DTEXTURE9 GetTexGuild() const { return m_texguild; }
    static CPythonLeaderboard& Instance() { return GetSingleton(); }
    static bool HasInstance() { return HasSingleton(); }
    static void DestroyInstance() { DestroySingleton(); }

    CPythonLeaderboard();
    ~CPythonLeaderboard();
    void SetText(const char* data);

    const std::string& GetText() const { return m_text; }
    void OnDeviceCreate(LPDIRECT3DDEVICE9 dev);
    void OnDeviceLost();
    void OnDeviceReset(LPDIRECT3DDEVICE9 dev);
    void OnDeviceDestroy();
    void ResetBindings();
 

    bool Ensure() const;
  
    void DrawTest() const;

    static void DrawRect(IDirect3DDevice9* dev, int x1, int y1, int x2, int y2, D3DCOLOR color);
    void SetTextSkillMob(const char* data);
    static void DrawRectSkillMob(IDirect3DDevice9* dev, int x1, int y1, int x2, int y2, D3DCOLOR color);
    void DrawSkillMob() const;

    virtual void	OnLeaderBoardRender();

    LPDIRECT3DTEXTURE9 m_newsTex = nullptr; 

    LPDIRECT3DTEXTURE9 GetTex() const { return m_tex; }
    LPDIRECT3DTEXTURE9 GetTexSkillMob() const { return m_texskillmob; }

private:
    static constexpr int W = 1024;
    static constexpr int H = 512;
    IDirect3DDevice9* m_pd3dDevice = nullptr;
    std::string m_text;
    std::vector<std::string> m_leaderboardLines;
    std::vector<std::string> m_MobSkillLines;

    bool sBoundBillboard = false;
    bool sBoundMobSkill = false;
    bool m_deviceLost = false;

    LPDIRECT3DTEXTURE9 m_tex = nullptr;
    LPD3DXFONT         m_font = nullptr;
    LPDIRECT3DTEXTURE9 m_texskillmob = nullptr;
    std::vector<std::string> m_GuildLines;

    bool sBoundGuild = false;

    LPDIRECT3DTEXTURE9 m_texguild = nullptr;


};

#endif