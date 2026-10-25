#ifndef XLIONCORE_RESOURCES_API_H
#define XLIONCORE_RESOURCES_API_H
#pragma once

// The resource system of the core: like the physics, it lives in LIONCore.dll, so every Level (each has its own copy of the core) owns its own resource manager, and everything that runs
// inside that copy - the renderer's text, a script of the game - asks the core for what it needs instead of keeping a manager of its own. The functions are exported (ordinary import-lib
// linking, the same as xlioncore_physics_api.h): a caller never sees the manager, only what it loaded.
//
// What it loads is the compiled resources of the project (<project>/Cache/Resources/Platforms/Windows), the ones the resource pipeline writes. A resource that is not compiled yet (or not
// loadable) is simply "not there": the call says so and the caller asks again later (the core does not retry more than once a second for the same resource).
#include <cstdint>

#if defined(XLIONCORE_BUILD_SHARED)
    #if defined(XLIONCORE_EXPORTS)
        #define XLIONCORE_API __declspec(dllexport)
    #else
        #define XLIONCORE_API __declspec(dllimport)
    #endif
#else
    #define XLIONCORE_API
#endif

namespace xgpu        { struct device; struct texture; }
namespace xfont_rsc   { struct font; }

namespace xlioncore::resources
{
    // A loaded font, as the text renderer needs it: the compiled glyph data (xfont_rsc::font: metrics, kerning, the lookup of a codepoint) and the atlas texture it samples (what the texture
    // holds depends on the font's output type: MTSDF, SDF or BITMAP). Both stay valid until Shutdown.
    struct font_view
    {
        const xfont_rsc::font*  m_pFont     = nullptr;
        xgpu::texture*          m_pTexture  = nullptr;
    };

    // Starts the resource manager of this copy of the core: the device the textures are made on, and the project whose compiled resources are loaded. Called again it does nothing (a
    // different project asks for a Shutdown first).
    XLIONCORE_API void Initialize(xgpu::device& Device, const wchar_t* pProjectPath) noexcept;

    // The font with this instance guid (the instance part of a font reference), loaded the first time it is asked for. False while it cannot be had.
    XLIONCORE_API bool GetFont(std::uint64_t FontInstance, font_view& Out) noexcept;

    // A loaded resource of the types the engine draws with, as the pointer of the plugin's runtime struct: GeomStatic (xgeom_static::xgpu::geom), Material (xmaterial::rt),
    // MaterialInstance (xmaterial_instance::rt), Texture (xgpu::texture). Loaded the first time it is asked for; null while it cannot be had (not compiled yet: asked again after a
    // second). Instance: the instance part of a reference (odd), or an already loaded pointer (even: returned as it is - what a loaded resource's own references become).
    XLIONCORE_API void* GetResource(std::uint64_t TypeGuid, std::uint64_t Instance) noexcept;

    // The resource was compiled again (the editor, after a compile): what is loaded of it is old. GetResource gives the new one from now on (the old one
    // goes when nothing holds it); what was made from the old one has to be made again - UpdateCount tells a cache when to look.
    XLIONCORE_API void MarkUpdated(std::uint64_t TypeGuid, std::uint64_t Instance) noexcept;

    // The instance guid of a reference: Instance itself when it is one (odd), the guid of the loaded resource when it is a pointer (even: what a loaded resource's own
    // references become). 0 when the pointer is not a resource of TypeGuid that is known.
    XLIONCORE_API std::uint64_t GuidOf(std::uint64_t TypeGuid, std::uint64_t Instance) noexcept;

    // How many times MarkUpdated ran: a cache of things made from resources (a renderer's materials) checks them again when it changes.
    XLIONCORE_API std::uint32_t UpdateCount() noexcept;

    // Lets go of everything that was loaded.
    XLIONCORE_API void Shutdown() noexcept;
}

#endif // XLIONCORE_RESOURCES_API_H
