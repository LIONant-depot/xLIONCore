// The resource system of the core (see xlioncore_resources_api.h): the resource manager of this copy of LIONCore.dll, with the loaders of the resource types the engine uses itself. Today:
// Font (and the Texture its atlas is), GeomStatic, Material, MaterialInstance. The loaders are the plugins' own - compiled in here, the same way the editor folds them into its own translation unit - so a compiled resource is
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
#include "plugins/xmaterial.plugin/source/xmaterial_xgpu_rsc_loader.h"
#include "plugins/xmaterial.plugin/source/xmaterial_xgpu_rsc_loader.cpp"
#include "plugins/xmaterial_instance.plugin/source/xmaterial_instance_xgpu_rsc_loader.h"
#include "plugins/xmaterial_instance.plugin/source/xmaterial_instance_xgpu_rsc_loader.cpp"
#include "plugins/xgeom_static.plugin/source/xgeom_static_xgpu_rsc_loader.h"
#include "plugins/xgeom_static.plugin/source/xgeom_static_xgpu_rsc_loader.cpp"

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
        std::uint32_t                                   s_UpdateCount = 0;     // MarkUpdated calls (UpdateCount)
        std::unordered_map<std::uint64_t, loaded_font>  s_Fonts;

        struct loaded_resource
        {
            xresource::full_guid                    m_Ref{};
            void*                                   m_pData     = nullptr;
            std::chrono::steady_clock::time_point   m_NextTry{};
        };
        struct pair_hash { std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t>& P) const noexcept { return std::hash<std::uint64_t>{}(P.first * 0x9E3779B97F4A7C15ull ^ P.second); } };
        std::unordered_map<std::pair<std::uint64_t, std::uint64_t>, loaded_resource, pair_hash> s_Resources;
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

    void* GetResource(std::uint64_t TypeGuid, std::uint64_t Instance) noexcept
    {
        std::lock_guard Lock(s_Mutex);
        if (!s_bReady || Instance == 0) return nullptr;
        if ((Instance & 1) == 0)                                                    // already a pointer to the loaded resource (a loaded resource's own references)...
        {
            xresource::full_guid Ref;
            Ref.m_Instance.m_Pointer = reinterpret_cast<void*>(Instance);
            Ref.m_Type               = xresource::type_guid{ TypeGuid };
            if (!xresource::g_Mgr.isStale(Ref)) return reinterpret_cast<void*>(Instance);
            Instance = xresource::g_Mgr.getFullGuid(Ref).m_Instance.m_Value;        // ... unless compiled again since (MarkUpdated): the new one, by its guid
        }

        auto& Entry = s_Resources[{ TypeGuid, Instance }];
        if (Entry.m_pData)
        {
            if (!xresource::g_Mgr.isStale(Entry.m_Ref)) return Entry.m_pData;
            if (void* pNew = xresource::g_Mgr.getResource(Entry.m_Ref)) Entry.m_pData = pNew;      // compiled again (MarkUpdated): the reference moves to the new one
            return Entry.m_pData;
        }
        const auto Now = std::chrono::steady_clock::now();
        if (Now < Entry.m_NextTry) return nullptr;

        Entry.m_Ref.m_Instance = xresource::instance_guid{ Instance };
        Entry.m_Ref.m_Type     = xresource::type_guid{ TypeGuid };
        Entry.m_pData          = xresource::g_Mgr.getResource(Entry.m_Ref);
        if (!Entry.m_pData)
        {
            Entry.m_Ref.m_Instance = xresource::instance_guid{ Instance };        // not loaded: the reference stays a guid, asked again in a second
            Entry.m_NextTry        = Now + std::chrono::seconds(1);
        }
        return Entry.m_pData;
    }

    void MarkUpdated(std::uint64_t TypeGuid, std::uint64_t Instance) noexcept
    {
        std::lock_guard Lock(s_Mutex);
        if (!s_bReady || (Instance & 1) == 0) return;
        xresource::full_guid Guid;
        Guid.m_Instance = xresource::instance_guid{ Instance };
        Guid.m_Type     = xresource::type_guid{ TypeGuid };
        xresource::g_Mgr.MarkUpdated(Guid);
        ++s_UpdateCount;
    }

    std::uint32_t UpdateCount() noexcept
    {
        std::lock_guard Lock(s_Mutex);
        return s_UpdateCount;
    }

    void Shutdown() noexcept
    {
        std::lock_guard Lock(s_Mutex);
        for (auto& [Key, Entry] : s_Resources)
            if (Entry.m_pData) xresource::g_Mgr.ReleaseRef(Entry.m_Ref);
        s_Resources.clear();
        for (auto& [Instance, Entry] : s_Fonts)
            if (Entry.m_bRequested) xresource::g_Mgr.ReleaseRef(Entry.m_Ref);
        s_Fonts.clear();
        s_bReady = false;
    }
}
