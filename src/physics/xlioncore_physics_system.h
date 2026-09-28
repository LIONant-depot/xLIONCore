#ifndef XLIONCORE_PHYSICS_SYSTEM_H
#define XLIONCORE_PHYSICS_SYSTEM_H
#pragma once

// Physics system V1 follow-up + freeze fix:
//   Body create uses dynamics presence + static_tag presence + Mass.
//   Dynamic bodies: physics is authoritative while simulating - ignore DirtyToPhysics
//   pose pushes (inspector/gizmo property writers MarkDirty; pushing that back into Box3D
//   every frame was resetting the pose => frozen crates).
//   Kinematic/Static still honor Dirty teleports.
//   Each tick: (conditional) Dirty push; CoolDown--; apply Force/Torque then clear;
//   Step; velocity readback; pose writeback WITHOUT Dirty.
// SHARE args must be non-const refs (xECS iterator assigns into the share tuple slot).
#include "xlioncore_physics.h"
#include "xlioncore_physics_backend.h"
#include "../transform/xlioncore_transform.h"
#include "../tags/xlioncore_tags.h"
#include <cstdio>

namespace xlioncore::physics
{
    struct system : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::update{ .m_pName = "Physics" };
        // What OnUpdate actually touches (const = read only). optional<> entries don't affect matching -
        // they're reached through an optional Foreach pointer / hasComponents inside OnUpdate.
        using query = std::tuple
            < xecs::query::must
                < xlioncore::transform
                , const physics_body_properties
                , const physics_shape_properties
                , box3d_body
                >
            , xecs::query::optional
                < dynamics
                , const xlioncore::static_tag
                >
            >;

        backend m_Backend;

        system(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        static b3BodyType ResolveBodyType(const dynamics* pDyn, bool bIsStatic) noexcept
        {
            if (pDyn) return b3_dynamicBody;
            return bIsStatic ? b3_staticBody : b3_kinematicBody;
        }

        void OnUpdate(void) noexcept
        {
            static int s_Tick = 0;
            ++s_Tick;
            const bool bLog = (s_Tick <= 3) || ((s_Tick % 60) == 0);

            xecs::query::instance Query;
            Query.m_Must.AddFromComponents
                < xlioncore::transform
                , physics_body_properties
                , physics_shape_properties
                , box3d_body
                >();
            auto S = Search(Query);

            int nEnt = 0, nNull = 0, nDyn = 0, nKin = 0, nStat = 0, nHasDynComp = 0, nDirty = 0, nDirtyIgnored = 0;
            float yMin = 1e9f, yMax = -1e9f;
            float yFirstDyn = -999.f;

            Foreach(S, [&]( const xecs::component::entity& Ent
                          , xlioncore::transform& T
                          , physics_body_properties& BodyProps
                          , physics_shape_properties& ShapeProps
                          , box3d_body& Body
                          , dynamics* pDyn ) noexcept
            {
                ++nEnt;
                if (pDyn) ++nHasDynComp;
                if (T.m_DirtyToPhysics) ++nDirty;

                // xecs::component::type::TAG components (static_tag) can't appear as a Foreach
                // parameter at all (assert_standard_function_v's own static_assert - they carry no
                // per-entity storage to hand back a pointer to), unlike DATA/SHARE components like
                // dynamics above. hasComponents<T> is the public presence check that works for any
                // component kind, TAG included.
                const bool bIsStatic = hasComponents<xlioncore::static_tag>(Ent);

                const xmath::fvec3 ScaledHalfExtents = T.m_Scale * 0.5f;
                const b3BodyType   ResolvedType      = ResolveBodyType(pDyn, bIsStatic);
                const float        Mass              = pDyn ? pDyn->m_Mass : 0.0f;

                if (ResolvedType == b3_dynamicBody) ++nDyn;
                else if (ResolvedType == b3_kinematicBody) ++nKin;
                else ++nStat;

                const bool NeedsRecreate =
                       B3_IS_NULL(Body.m_BodyId)
                    || Body.m_BodyHalfExtents != ScaledHalfExtents
                    || Body.m_CachedBodyType  != ResolvedType
                    || Body.m_CachedMass      != Mass
                    || Body.m_CachedFriction  != ShapeProps.m_Friction
                    || Body.m_CachedRestitution != ShapeProps.m_Restitution;

                if (NeedsRecreate)
                {
                    if (B3_IS_NON_NULL(Body.m_BodyId))
                    {
                        m_Backend.DestroyBody(Body.m_BodyId);
                        Body.m_BodyId = b3_nullBodyId;
                    }

                    body_create_params Params;
                    Params.m_Type           = ResolvedType;
                    Params.m_LinearDamping  = BodyProps.m_LinearDamping;
                    Params.m_AngularDamping = BodyProps.m_AngularDamping;
                    Params.m_EnableSleep    = BodyProps.m_EnableSleep;
                    Params.m_IsBullet       = BodyProps.m_EnableContinuousCollision;
                    Params.m_Position       = T.m_Position;
                    Params.m_Rotation       = T.m_Rotation;
                    Params.m_HalfExtents    = ScaledHalfExtents;
                    Params.m_Mass           = Mass;
                    Params.m_Friction       = ShapeProps.m_Friction;
                    Params.m_Restitution    = ShapeProps.m_Restitution;
                    Params.m_CategoryBits   = ShapeProps.m_CategoryBits;
                    Params.m_MaskBits       = ShapeProps.m_MaskBits;
                    Params.m_GroupIndex     = ShapeProps.m_GroupIndex;
                    Params.m_IsSensor       = ShapeProps.m_IsSensor;
                    Params.m_LocalPosition  = ShapeProps.LocalPosition();
                    Params.m_LocalRotation  = ShapeProps.LocalRotation();

                    Body.m_BodyId            = m_Backend.CreateBody(Params);
                    Body.m_BodyHalfExtents   = ScaledHalfExtents;
                    Body.m_CachedBodyType    = ResolvedType;
                    Body.m_CachedMass        = Mass;
                    Body.m_CachedFriction    = ShapeProps.m_Friction;
                    Body.m_CachedRestitution = ShapeProps.m_Restitution;

                    if (B3_IS_NULL(Body.m_BodyId)) ++nNull;

                    T.m_DirtyToPhysics = 0;
                    T.m_PhysicsSyncCoolDown = xlioncore::transform::kPhysicsSyncCoolDownN;
                }
                else if (B3_IS_NON_NULL(Body.m_BodyId))
                {
                    if (T.m_DirtyToPhysics)
                    {
                        // Freeze fix: dynamic bodies ignore Dirty pose pushes while simulating.
                        if (ResolvedType != b3_dynamicBody)
                            m_Backend.SetTransform(Body.m_BodyId, T.m_Position, T.m_Rotation);
                        else
                            ++nDirtyIgnored;
                        T.m_DirtyToPhysics = 0;
                        T.m_PhysicsSyncCoolDown = xlioncore::transform::kPhysicsSyncCoolDownN;
                    }
                    else if (T.m_PhysicsSyncCoolDown > 0)
                    {
                        --T.m_PhysicsSyncCoolDown;
                    }
                }
                else
                {
                    ++nNull;
                }

                if (pDyn && B3_IS_NON_NULL(Body.m_BodyId) && ResolvedType == b3_dynamicBody)
                {
                    if (pDyn->m_Force.m_X != 0.0f || pDyn->m_Force.m_Y != 0.0f || pDyn->m_Force.m_Z != 0.0f)
                        m_Backend.ApplyForceToCenter(Body.m_BodyId, pDyn->m_Force);
                    if (pDyn->m_Torque.m_X != 0.0f || pDyn->m_Torque.m_Y != 0.0f || pDyn->m_Torque.m_Z != 0.0f)
                        m_Backend.ApplyTorque(Body.m_BodyId, pDyn->m_Torque);
                    pDyn->m_Force  = {};
                    pDyn->m_Torque = {};
                }
            });

            m_Backend.Step();

            Foreach(S, [&]( xlioncore::transform& T
                          , physics_body_properties&
                          , physics_shape_properties&
                          , box3d_body& Body
                          , dynamics* pDyn ) noexcept
            {
                if (B3_IS_NULL(Body.m_BodyId)) return;
                if (Body.m_CachedBodyType == b3_staticBody) return;

                T.m_Position = m_Backend.GetPosition(Body.m_BodyId);
                T.m_Rotation = m_Backend.GetRotation(Body.m_BodyId);

                if (T.m_Position.m_Y < yMin) yMin = T.m_Position.m_Y;
                if (T.m_Position.m_Y > yMax) yMax = T.m_Position.m_Y;
                if (yFirstDyn < -900.f && Body.m_CachedBodyType == b3_dynamicBody)
                    yFirstDyn = T.m_Position.m_Y;

                if (pDyn && Body.m_CachedBodyType == b3_dynamicBody)
                {
                    pDyn->m_LinearVelocity  = m_Backend.GetLinearVelocity(Body.m_BodyId);
                    pDyn->m_AngularVelocity = m_Backend.GetAngularVelocity(Body.m_BodyId);
                }
            });

            if (bLog)
            {
                std::printf("[Physics] tick=%d ents=%d hasDyn=%d dirty=%d dirtyIgn=%d types(d/k/s)=%d/%d/%d null=%d yRange=[%.3f,%.3f] yDyn0=%.3f\n"
                    , s_Tick, nEnt, nHasDynComp, nDirty, nDirtyIgnored, nDyn, nKin, nStat, nNull
                    , (yMin > 1e8f ? 0.f : yMin), (yMax < -1e8f ? 0.f : yMax), yFirstDyn);
                std::fflush(stdout);
            }
        }
    };
}

#endif // XLIONCORE_PHYSICS_SYSTEM_H
