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
#include <span>
#include "xlioncore_physics_shape.h"

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

    // What a ray hit (plan 13.2): the world point and the surface normal there, the fraction of the ray, the entity whose body owns the
    // shape (0: a body without one)
    struct ray_hit
    {
        bool            m_bHit      = false;
        xmath::fvec3    m_Point     {};
        xmath::fvec3    m_Normal    {0, 1, 0};
        float           m_Fraction  = 1;
        std::uint64_t   m_Entity    = 0;
    };

    // The closest shape a ray from Origin along Translation (its length is the reach) hits in the game's physics world (Box3D
    // b3World_CastRayClosest, the default filter; sensors and back faces are skipped). No game physics or nothing hit: m_bHit false.
    // The feet's ground casts (K3) and any game query.
    XLIONCORE_API ray_hit CastRayClosest( xecs::game_mgr::instance& GameMgr, const xmath::fvec3& Origin, const xmath::fvec3& Translation ) noexcept;

    // Shape casts and overlaps (plan 13.2; xlioncore_physics_queries.h): the first thing Shape (relative to Origin) hits moving along
    // Translation - sensors and SkipEntity's bodies (0: none, e.g. the caster's own) left out; a shape that starts inside something hits at
    // fraction 0 - and the entities whose bodies overlap Shape at Origin (each once; returns how many, writes up to MaxOut). No game physics:
    // no hit, 0.
    XLIONCORE_API ray_hit CastShapeClosest( xecs::game_mgr::instance& GameMgr, const xmath::fvec3& Origin, const query_shape& Shape, const xmath::fvec3& Translation, std::uint64_t SkipEntity = 0 ) noexcept;
    XLIONCORE_API int     OverlapShape    ( xecs::game_mgr::instance& GameMgr, const xmath::fvec3& Origin, const query_shape& Shape, std::uint64_t* pOut = nullptr, int MaxOut = 0 ) noexcept;

    // The solid hits (contact hit events: shapes with ContactEvents on, faster than 1 m/s) of the steps since the last call, in step order: the
    // two entities, where, the normal (from A to B), the approach speed, and an id unique in this game world (an interaction event id, R3b').
    // Returns how many there were (writes up to MaxOut); the rest are dropped with this call.
    // m_MassA, m_MassB: the two bodies' masses (0: not dynamic - a wall, a kinematic mover)
    struct impact { std::uint64_t m_Id = 0, m_A = 0, m_B = 0; xmath::fvec3 m_Point{}, m_Normal{}; float m_Speed = 0, m_MassA = 0, m_MassB = 0; };
    XLIONCORE_API int TakeImpacts( xecs::game_mgr::instance& GameMgr, impact* pOut, int MaxOut ) noexcept;

    // A character's capsule moved through the world (plan 13.4; Box3D's mover: b3World_CollideMover + b3SolvePlanes + b3World_CastMover,
    // as its character sample): Base is the feet (the capsule stands on it, its bottom Step above so the ground and kerbs under that do not
    // block it - the feet's ground fit does the height), Delta the move asked. Slides along walls; dynamic bodies (a ball) do not block it.
    // Extra: more planes (normal, offset: normal . move >= offset), e.g. other characters. Returns the move actually made.
    struct mover_plane { xmath::fvec3 m_Normal{}; float m_Offset = 0; };
    XLIONCORE_API xmath::fvec3 MoveCapsule( xecs::game_mgr::instance& GameMgr, const xmath::fvec3& Base, float Radius, float Height, float Step
                                          , const xmath::fvec3& Delta, std::span<const mover_plane> Extra = {} ) noexcept;

    // Pushes the dynamic bodies within Radius of Center (their shapes' boxes overlap the sphere's box) to at least Velocity along it: a
    // kick, a shove (an animation's Contact event). A body already faster that way keeps its speed. Returns how many it pushed.
    struct pushed_body { std::uint64_t m_Entity = 0; xmath::fvec3 m_Position{}; xmath::fquat m_Rotation{}; };      // (where it was when pushed)
    XLIONCORE_API int PushBodies( xecs::game_mgr::instance& GameMgr, const xmath::fvec3& Center, float Radius, const xmath::fvec3& Velocity
                                , pushed_body* pOut = nullptr, int MaxOut = 0 ) noexcept;
}

#endif // XLIONCORE_PHYSICS_API_H
