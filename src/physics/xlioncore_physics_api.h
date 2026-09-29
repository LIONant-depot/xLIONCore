#ifndef XLIONCORE_PHYSICS_API_H
#define XLIONCORE_PHYSICS_API_H
#pragma once

// The fixed, always-present cross-DLL surface for physics - ordinary compile-time import-lib
// linking (xLION.exe links xLIONCore directly, same as xLIONRender's Init/Draw), not GetProcAddress -
// that's for the ECS component/system self-registration set instead (xlioncore_plugin_entry.cpp),
// which needs runtime discovery for a hot-reloadable Game.dll. This is the "editor is a privileged
// caller" escape hatch: a live dynamic body is physics-authoritative and Physics::OnUpdate never
// looks at its Dirty flag (see xlioncore_physics_system.h's own comment) - a caller that really
// wants to move one goes through here, not through the ECS. Callers never need xlioncore_physics.h's
// physics_body definition - everything box3d-shaped is resolved inside this DLL, via
// GameMgr.getUserData<physics::system>() (set in system::OnCreate).
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"

#if defined(XLIONCORE_BUILD_SHARED)
    #if defined(XLIONCORE_EXPORTS)
        #define XLIONCORE_API __declspec(dllexport)
    #else
        #define XLIONCORE_API __declspec(dllimport)
    #endif
#else
    #define XLIONCORE_API
#endif

namespace xlioncore::physics
{
    // Teleports a DYNAMIC body's pose directly (position + rotation), zeroing linear/angular
    // velocity - a teleport has no implied motion, same semantic as Unity's Rigidbody.position
    // setter or Unreal's SetActorLocation on a simulating body. Returns false (no-op) if Entity has
    // no live body, or its body isn't currently Dynamic (static/kinematic go through the ordinary
    // Dirty-flag path instead - see xscene's DemoteStaticIfPlaying for the static case).
    XLIONCORE_API bool TeleportDynamicBody
    ( xecs::game_mgr::instance&  GameMgr
    , xecs::component::entity    Entity
    , const xmath::fvec3&        Position
    , const xmath::fquat&        Rotation
    ) noexcept;
}

#endif // XLIONCORE_PHYSICS_API_H
