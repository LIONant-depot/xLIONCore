// The resource system of the core (see xlioncore_resources_api.h): the resource manager of this copy of LIONCore.dll, with the loaders of the resource types the engine uses itself. Today:
// Font (and the Texture its atlas is). The loaders are the plugins' own - compiled in here, the same way the editor folds them into its own translation unit - so a compiled resource is
// read by the one code that wrote it.


// What the plugins' loaders assume to be there when they are compiled (see LevelEditor_Main.cpp, which does the same for the editor): the manager, the GPU, the user data of the manager.
#include "dependencies/xGPU/source/xGPU.h"
#include "dependencies/xresource_mgr/source/xresource_mgr.h"

struct resource_mgr_user_data
{
    xgpu::device m_Device = {};
};

#include "plugins/xtexture.plugin/source/xtexture_xgpu_rsc_loader.h"
#include "plugins/xtexture.plugin/source/xtexture_xgpu_rsc_loader.cpp"
#include "plugins/xfont.plugin/source/xfont_xgpu_rsc_loader.h"
#include "plugins/xfont.plugin/source/xfont_xgpu_rsc_loader.cpp"

#include "xlioncore_resources_api.h"

#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <unordered_map>

namespace xlioncore::resources
{
    namespace
    {
        struct loaded_font
        {
            xrsc::font_ref                          m_Ref{};
            std::chrono::steady_clock::time_point   m_NextTry{};        // a font that could not be loaded is asked for again after a second, not every frame
            bool                                    m_bRequested = false;
        };

        std::mutex                                      s_Mutex;
        resource_mgr_user_data                          s_UserData;
        bool                                            s_bReady = false;
        std::unordered_map<std::uint64_t, loaded_font>  s_Fonts;
    }

    void Initialize(xgpu::device& Device, const wchar_t* pProjectPath) noexcept
    {
        std::lock_guard Lock(s_Mutex);
        if (s_bReady || !pProjectPath) return;

        s_UserData.m_Device = Device;
        xresource::g_Mgr.Initiallize(2000);
        xresource::g_Mgr.setUserData(&s_UserData, false);
        xresource::g_Mgr.setRootPath(std::format(L"{}//Cache//Resources//Platforms//Windows", pProjectPath));
        s_bReady = true;
    }

    bool GetFont(std::uint64_t FontInstance, font_view& Out) noexcept
    {
        std::lock_guard Lock(s_Mutex);
        if (!s_bReady) return false;
        if ((FontInstance & 1) == 0) return false;      // an instance guid is odd: the resource manager tells a guid from a pointer by that bit (an even value is no resource)

        auto& Entry = s_Fonts[FontInstance];
        const auto Now = std::chrono::steady_clock::now();
        if (!Entry.m_bRequested)
        {
            if (Now < Entry.m_NextTry) return false;
            Entry.m_Ref.m_Instance = xresource::instance_guid{ FontInstance };
        }

        // getResource loads on first use and turns the reference into the pointer of the loaded resource (see xresource_mgr.h): the reference is kept, so it is released once.
        xfont::rt* pFont = xresource::g_Mgr.getResource(Entry.m_Ref);
        if (!pFont || !pFont->m_pFont || !pFont->m_pTexture)
        {
            Entry.m_NextTry = Now + std::chrono::seconds(1);
            return false;
        }

        Entry.m_bRequested = true;
        Out.m_pFont    = pFont->m_pFont;
        Out.m_pTexture = pFont->m_pTexture;
        return true;
    }

    void Shutdown() noexcept
    {
        std::lock_guard Lock(s_Mutex);
        for (auto& [Instance, Entry] : s_Fonts)
            if (Entry.m_bRequested) xresource::g_Mgr.ReleaseRef(Entry.m_Ref);
        s_Fonts.clear();
        s_bReady = false;
    }
}
