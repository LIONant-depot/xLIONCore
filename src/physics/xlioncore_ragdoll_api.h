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
    // Supported tracking (plan R1, ladder step 2): every joint pulls toward the animated pose (xanimation::SetTrackingTargets: scale-free gain,
    // damping ratio, the feed-forward motor of the animation's own joint velocity); Supported: the root body turns kinematic and follows the
    // animated root (the character is held up, not balanced: balance is R2). BonePrev, BoneCur: the animated world rigid bone frames one fixed
    // step back and now (as ActivateRagdoll's). Call every frame while it tracks; once called, the joints track until the ragdoll is deactivated.
    struct ragdoll_tracking
    {
        float   m_Gain        = 30.0f;  // spring stiffness in units of the joint's gravity load (k = Gain M g l per radian)
        float   m_Damping     = 1.0f;   // damping ratio on the chain's own inertia
        float   m_FeedForward = 0.3f;   // the velocity motor's torque cap, in spring torques per radian (0: spring only)
        bool    m_bSupported  = true;
        std::span<const float> m_JointTone;     // KD4b: a stiffness factor per joint of the graph (empty: all 1)
        float   m_GravityFF   = 0.0f;   // R2 stage 2a: each joint also holds the side away from the feet against gravity (equal and opposite
                                        // torque pairs before every step; the feet: the two lowest bodies; shares by how near the COM is over each), times this
    };
    // Returns how far the joints were from their targets after each physics step since the last call (-1: no step ran): the rms over the joints of each joint's
    // rotation from its target, degrees (pWorldRms: the same over each body's world rotation, which adds up down a limb).
    XLIONCORE_API float TrackRagdoll( ragdoll& Ragdoll, std::span<const xanimation::rigid> BonePrev, std::span<const xanimation::rigid> BoneCur, float Dt, const ragdoll_tracking& Params, float* pWorldRms = nullptr ) noexcept;

    // R3b' (contract 2.2): an interaction event on a ragdoll - Impulse (N s) at the world Point, on the body nearest it, applied right
    // before the next physics step and only once per EventId in this game world (the ledger: the same event sent again is dropped and logged)
    XLIONCORE_API void HitRagdoll( xecs::game_mgr::instance& GameMgr, ragdoll& Ragdoll, std::uint64_t EventId, const xmath::fvec3& Point, const xmath::fvec3& Impulse ) noexcept;

    // The ragdoll's bodies never touch Entity's body (contract 2.6: no double collision coverage - a player's capsule and the Lilly's ragdoll
    // inside it are the same character). Box3D filter joints, gone with the ragdoll. Entity without a body: nothing.
    XLIONCORE_API void RagdollIgnoreEntity( xecs::game_mgr::instance& GameMgr, ragdoll& Ragdoll, std::uint64_t Entity ) noexcept;

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

    // The Ragdoll editor's hand (test the ragdoll by pulling it): a body's world frame now (false: no such body); a pull of the point Local (in
    // the body's frame) toward Target, a mouse joint as Box2D v3 makes one (a motor joint's spring to a static anchor, Hertz, force capped at
    // four times the ragdoll's weight); call it before each step while the hand holds, and ReleaseRagdollPull when it lets go.
    XLIONCORE_API bool           RagdollBodyFrame( const ragdoll& Ragdoll, int iBody, xanimation::rigid& Out ) noexcept;
    XLIONCORE_API void           PullRagdollBody( ragdoll& Ragdoll, int iBody, const xmath::fvec3& Local, const xmath::fvec3& Target, float Hertz = 12.0f ) noexcept;
    XLIONCORE_API void           ReleaseRagdollPull( ragdoll& Ragdoll ) noexcept;
}

#endif
