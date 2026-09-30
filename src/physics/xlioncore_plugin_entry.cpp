// Self-registration entry points - same ABI Game.dll implements (xecs_plugin_api.h) plus
// XScript_GetComponentDisplayInfo so the editor can sort/filter inspector categories/priorities.
// Components/systems announce via XSCRIPT_REGISTER_* in their headers; this TU walks the lists.
#include "xlioncore_physics_system.h"
#include "xlioncore_physics_api.h"
#include "../transform/xlioncore_transform.h"
#include "../demo/xlioncore_demo_share.h"
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
        , xlioncore::physics::physics_body_properties
        , xlioncore::physics::physics_shape_properties
        , xlioncore::physics::physics_collider_box
        , xlioncore::physics::physics_collider_sphere
        , xlioncore::physics::physics_collider_capsule
        , xlioncore::physics::physics_collider_cylinder
        , xlioncore::physics::physics_dynamics
        , xlioncore::physics::physics_body
        , xlioncore::demo_share
        >();

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
        auto* pSystem = GameMgr.getUserData<system>();
        if (!pSystem) return false;

        auto* pBody = GetComponentPtr<physics_body>(GameMgr, Entity);
        if (!pBody || pBody->m_CachedBodyType != b3_dynamicBody) return false;

        pSystem->TeleportBody(*pBody, Position, Rotation);
        return true;
    }
}
