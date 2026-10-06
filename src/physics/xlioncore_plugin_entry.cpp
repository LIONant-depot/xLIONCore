// Self-registration entry points - same ABI Game.dll implements (xecs_plugin_api.h) plus
// XScript_GetComponentDisplayInfo so the editor can sort/filter inspector categories/priorities.
// Components/systems announce via XSCRIPT_REGISTER_* in their headers; this TU walks the lists.
#include "xlioncore_physics_system.h"
#include "xlioncore_physics_api.h"
#include "../transform/xlioncore_transform.h"
#include "../tags/xlioncore_tags.h"
#include "../demo/xlioncore_demo_share.h"
#include "../demo/xlioncore_demo_systems.h"
#include "xecs_plugin_api.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

using physics_system = xlioncore::physics::system;
XSCRIPT_REGISTER_SYSTEM(physics_system)

using physics_destroy_notify = xlioncore::physics::destroy_notify;
XSCRIPT_REGISTER_SYSTEM(physics_destroy_notify)

using physics_body_builder = xlioncore::physics::body_builder;
XSCRIPT_REGISTER_SYSTEM(physics_body_builder)

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterComponents(xecs::game_mgr::instance& GameMgr, xecs::plugin::token Token) noexcept
{
    for (auto* p = xscript::self_registration<xscript::component_entry>::s_pHead; p; p = p->m_pNext)
        p->m_Value.m_pRegisterFn(GameMgr, Token);
}

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterSystems(xecs::game_mgr::instance& GameMgr) noexcept
{
    // Same cross-module BitID lesson as LIONRender: Lock may have assigned BitIDs onto a different
    // info_v* than this DLL's own copies (ptrEq==0). Physics Search uses raw info_v<T>.m_BitID -
    // sync from the registry by GUID before any system is created/queries.
    GameMgr.m_ComponentMgr.LockComponentTypes();
    xecs::component::mgr::SyncLocalBitIDs
        < xlioncore::transform
        , xlioncore::render_transform
        , xlioncore::physics::physics_body_properties
        , xlioncore::physics::physics_collider_box
        , xlioncore::physics::physics_collider_sphere
        , xlioncore::physics::physics_collider_capsule
        , xlioncore::physics::physics_collider_cylinder
        , xlioncore::physics::physics_dynamics
        , xlioncore::physics::physics_body
        , xlioncore::demo_share
        >();

    // The events of the physics are registered before any system (a system that listens to one finds it there).
    GameMgr.RegisterGlobalEvents<xlioncore::physics::sensor_begin_event, xlioncore::physics::sensor_end_event, xlioncore::physics::contact_begin_event, xlioncore::physics::contact_end_event, xlioncore::physics::contact_hit_event>();

    for (auto* p = xscript::self_registration<xscript::system_entry>::s_pHead; p; p = p->m_pNext)
        p->m_Value.m_pRegisterFn(GameMgr);
}

extern "C" __declspec(dllexport)
void XecsPlugin_Unregister(xecs::plugin::token /*Token*/) noexcept
{
}

extern "C" __declspec(dllexport)
void XScript_GetComponentDisplayInfo(xscript::pfn_component_display_visitor pVisitor, void* pUserData) noexcept
{
    for (auto* p = xscript::self_registration<xscript::component_entry>::s_pHead; p; p = p->m_pNext)
        pVisitor(pUserData, p->m_Value.m_Guid, p->m_Value.m_pName, p->m_Value.m_pCategory, p->m_Value.m_Priority);

    // The hierarchy is xECS's own (it registers parent and children itself), so it announces itself here: with the Transform it belongs to, in the Basics.
    const auto& Parent   = xecs::component::type::info_v<xecs::component::parent>;
    const auto& Children = xecs::component::type::info_v<xecs::component::children>;
    pVisitor(pUserData, Parent.m_Guid.m_Value,   Parent.m_pName,   "Basics", 2);
    pVisitor(pUserData, Children.m_Guid.m_Value, Children.m_pName, "Basics", 3);

    // The state of the entity in the editor (the Level Tree's power and eye): in the Basics too, after the runtime ones.
    const auto& EditorDisable  = xecs::component::type::info_v<xecs::editor::disable_tag>;
    const auto& EditorNoRender = xecs::component::type::info_v<xecs::editor::no_render_tag>;
    pVisitor(pUserData, EditorDisable.m_Guid.m_Value,  EditorDisable.m_pName,  "Basics", 6);
    pVisitor(pUserData, EditorNoRender.m_Guid.m_Value, EditorNoRender.m_pName, "Basics", 7);
}

namespace xlioncore::physics
{
    bool TeleportDynamicBody
    ( xecs::game_mgr::instance&  GameMgr
    , xecs::component::entity    Entity
    , const xmath::fvec3&        Position
    , const xmath::fquat&        Rotation
    ) noexcept
    {
        auto* pGame   = game::From(GameMgr);
        auto* pSystem = pGame ? pGame->m_pPhysics : nullptr;
        if (!pSystem) return false;

        auto* pBody = GetComponentPtr<physics_body>(GameMgr, Entity);
        if (!pBody || pBody->m_CachedBodyType != b3_dynamicBody) return false;

        pSystem->TeleportBody(*pBody, Position, Rotation);
        return true;
    }
}
