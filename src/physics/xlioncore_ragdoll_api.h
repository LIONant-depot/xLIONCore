#ifndef XLIONCORE_RAGDOLL_API_H
#define XLIONCORE_RAGDOLL_API_H
#pragma once

// Ragdolls in a game's physics world (ragdoll design v2 section 8, R4): the bodies and joints are Box3D's, and Box3D lives inside this DLL
// (a static library: its worlds are this DLL's globals), so every call that touches them is made here. A ragdoll belongs to the physics
// system of its game: it is captured after every fixed step (two body frames kept: the render blends between them) and freed with the
// world. The caller (LIONRender's Ragdoll system: the animated pose, the component) only holds the handle.
// The world takes max(4, SubSteps of the active ragdolls) sub-steps while any ragdoll is active (16 hold the joints at a fall's impact).
#include "xlioncore_physics_api.h"
#include "dependencies/xAnimation/source/physical/xanimation_body_model.h"
#include <span>
#include <string>
#include <utility>

namespace xlioncore::physics
{
    struct ragdoll;     // opaque

    struct ragdoll_params
    {
        float   m_PassiveStiffness  = 3.0f;     // the scale-free passive spring (k M g l per radian) and joint friction (f M g l): xanimation_box3d.h
        float   m_FrictionFactor    = 0.05f;
        float   m_JointHertz        = 240.0f;   // capped by Box3D at a quarter of the sub-step rate
        float   m_Size              = 1.7f;     // m: the character's height (the sleep threshold, the teleport distance)
        int     m_SubSteps          = 16;
    };

    struct ragdoll_stats
    {
        float   m_MaxAnchor = 0;                // m: the worst joint anchor separation now
        float   m_KE = 0, m_Mass = 0, m_ComY = 0;
        float   m_MaxSpeed = 0;                 // m/s, the fastest body's centre
        int     m_nAwake = 0, m_nBodies = 0;
        int     m_iFastest = -1;                // the body of m_MaxSpeed
    };

    // Bodies at BoneCur with the motion between BonePrev and BoneCur (Dt apart): world rigid bone frames (xanimation::RigidBoneWorlds);
    // RestModel: the rest's rigid model frames. Null: no game physics. pNote: clamps, teleports, overlaps at the switch
    XLIONCORE_API ragdoll* ActivateRagdoll( xecs::game_mgr::instance& GameMgr, const xanimation::body_graph& Graph
                                          , std::span<const xanimation::rigid> RestModel, std::span<const xanimation::rigid> BonePrev, std::span<const xanimation::rigid> BoneCur
                                          , float Dt, std::span<const std::pair<int, int>> Filter, const ragdoll_params& Params, std::string* pNote ) noexcept;
    XLIONCORE_API void DeactivateRagdoll( xecs::game_mgr::instance& GameMgr, ragdoll* pRagdoll ) noexcept;
    // Every bone's world rigid frame, Alpha between the last two captures (the game's m_FixedInterpolate); Animated: the animated pose's world
    // rigid frames (the bones the ragdoll does not own keep theirs relative to their parent)
    XLIONCORE_API void PoseRagdoll( const ragdoll& Ragdoll, const xanimation::skeleton_view& Skel, float Alpha, std::span<const xanimation::rigid> Animated, std::span<xanimation::rigid> Out ) noexcept;
    XLIONCORE_API void RagdollStats( const ragdoll& Ragdoll, ragdoll_stats& Out ) noexcept;
    XLIONCORE_API xanimation::rigid RagdollRootBody( const ragdoll& Ragdoll, float Alpha ) noexcept;      // the root body's (the hips') world frame

    // A world of its own, outside any game (the Ragdoll editor's Drop, R5): gravity, a floor (its top at FloorY) and the ragdolls activated
    // in it. The SAME activation, sub-steps and capture as the game's world: one path for the editor and Play. Destroy frees its ragdolls.
    struct ragdoll_world;     // opaque
    XLIONCORE_API ragdoll_world* CreateRagdollWorld( float FloorY, float Gravity = 9.81f ) noexcept;
    XLIONCORE_API void           DestroyRagdollWorld( ragdoll_world* pWorld ) noexcept;
    XLIONCORE_API ragdoll*       ActivateRagdoll( ragdoll_world& World, const xanimation::body_graph& Graph
                                                , std::span<const xanimation::rigid> RestModel, std::span<const xanimation::rigid> BonePrev, std::span<const xanimation::rigid> BoneCur
                                                , float Dt, std::span<const std::pair<int, int>> Filter, const ragdoll_params& Params, std::string* pNote ) noexcept;
    XLIONCORE_API void           StepRagdollWorld( ragdoll_world& World, float Dt ) noexcept;     // one fixed step (the ragdolls' sub-steps), then every ragdoll captured
}

#endif
