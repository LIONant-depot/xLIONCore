#ifndef XLIONCORE_PHYSICS_SYSTEM_H
#define XLIONCORE_PHYSICS_SYSTEM_H
#pragma once

// Physics system V1 follow-up + freeze fix + static/kinetic split:
//   Body create uses dynamics presence + static_tag presence + Mass.
//   Dynamic bodies: physics is authoritative while simulating - ignore DirtyToPhysics
//   pose pushes (inspector/gizmo property writers MarkDirty; pushing that back into Box3D
//   every frame was resetting the pose => frozen crates).
//   Kinematic still honors Dirty teleports.
//   Each tick: (conditional) Dirty push; CoolDown--; apply Force/Torque then clear;
//   Step; velocity readback; pose writeback WITHOUT Dirty.
// SHARE args must be non-const refs (xECS iterator assigns into the share tuple slot).
//
// Static/kinetic split (direct user design, 2026-09-29):
//   Statics are excluded from the per-frame OnUpdate scan entirely (xecs::query::none_of<static_tag>) -
//   a level's static geometry, once created, never needs revisiting, and visiting it anyway "all the
//   time" was called out explicitly as a no-go. Their bodies are created exactly once, in
//   OnSceneReady (registered against xecs::scene::mgr::m_OnSceneReady - fired per scene at Play-press
//   in the editor, or at load in a future Game target - never at scene-LOAD time itself: NOTIFY_CREATE
//   fires with default-constructed component data during load, before LoadEntity's own deserialize
//   loop runs - confirmed by reading _CreateEntity's own call order - so it's unusable for this).
//   Destruction is the mirror case: a live Box3D body can only exist while Playing (Stop always does a
//   full GameMgr rebuild + reload from disk, wiping every body for free - see StopPlay/CreateWorld in
//   xlevel_session.h), so destroying one is only ever needed mid-session - destroy_notify (its own
//   tiny NOTIFY_DESTROY system, below) queues the BodyId; OnPostStructuralChanges (fires right after
//   this system's own per-frame structural-changes flush, always mid-Play when it matters) drains the
//   queue. NOTIFY_MOVE_IN/MOVE_OUT is deliberately NOT used anywhere here - too broad a signal (any
//   archetype migration, not specifically "this entity's physics data is now complete") - direct user
//   correction. A static entity assembled incrementally in the editor (AddComponent one piece at a
//   time, ending with static_tag) misses OnSceneReady the same way it would've missed a Move_In hook -
//   known, narrow, editor-authoring-only gap; a reload (Save, or a Play/Stop cycle) fixes it, since
//   that always re-creates the entity fresh via LoadEntity -> the next OnSceneReady.
//   Moving a static entity's Transform while Playing demotes it to Kinematic instead of silently
//   desyncing visuals from physics - see xscene_commands_property_edit.h / xscene_commands_transform_gizmo.h.
#include "xlioncore_physics.h"
#include "xlioncore_physics_backend.h"
#include "../transform/xlioncore_transform.h"
#include "../tags/xlioncore_tags.h"
#include <cstdio>

namespace xlioncore::physics
{
    // Resolves a component's live pointer for an arbitrary entity, DATA or SHARE, outside of any
    // Foreach - system::instance's own m_GameMgr is private, so a caller that needs raw pool access
    // for a handle NOT currently being iterated (OnSceneReady walks Scene.m_LocalToRuntime directly,
    // not a Search() result; destroy_notify::OnNotify gets a handle from an archetype event, not a
    // query) needs its own stored reference instead. Mirrors xscene::ResolveComponentPointer's exact
    // DATA/SHARE resolution (xscene_shared_component_template.h) - not reused directly since xLIONCore
    // (an engine DLL) shouldn't depend on xscene.plugin (an editor plugin).
    template< typename T >
    inline T* GetComponentPtr(xecs::game_mgr::instance& GameMgr, xecs::component::entity Entity) noexcept
    {
        auto& Details = GameMgr.m_ComponentMgr.getEntityDetails(Entity);
        if (!Details.m_pPool) return nullptr;
        auto& Info = xecs::component::type::info_v<T>;
        if (const auto iType = Details.m_pPool->findIndexComponentFromInfo(Info); iType >= 0)
            return reinterpret_cast<T*>(&Details.m_pPool->m_pComponent[iType][Details.m_PoolIndex.m_Value * Info.m_Size]);

        if (Info.m_TypeID != xecs::component::type::id::SHARE) return nullptr;
        auto* pFamily = Details.m_pPool->m_pMyFamily;
        if (!pFamily) return nullptr;
        for (int i = 0, end = static_cast<int>(pFamily->m_ShareInfos.size()); i < end; ++i)
        {
            if (pFamily->m_ShareInfos[i]->m_Guid.m_Value != Info.m_Guid.m_Value) continue;
            auto& ShareDetails = GameMgr.m_ComponentMgr.getEntityDetails(pFamily->m_ShareDetails[i].m_Entity);
            if (!ShareDetails.m_pPool) return nullptr;
            const auto iShare = ShareDetails.m_pPool->findIndexComponentFromInfo(Info);
            if (iShare < 0) return nullptr;
            return reinterpret_cast<T*>(&ShareDetails.m_pPool->m_pComponent[iShare][ShareDetails.m_PoolIndex.m_Value * Info.m_Size]);
        }
        return nullptr;
    }

    struct system : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::update{ .m_pName = "Physics" };
        // What OnUpdate actually touches (const = read only). none_of<static_tag>: statics are
        // created once (OnSceneReady) and never revisited here - see this file's own top comment.
        using query = std::tuple
            < xecs::query::must
                < xlioncore::transform
                , const physics_body_properties
                , const physics_shape_properties
                , box3d_body
                >
            , xecs::query::none_of< xlioncore::static_tag >
            , xecs::query::optional< dynamics >
            >;

        backend                    m_Backend;
        xecs::game_mgr::instance&  m_MyGameMgr;   // system::instance's own m_GameMgr is private - see GetComponentPtr's comment
        std::vector<b3BodyId>      m_PendingDestroy;   // queued by destroy_notify::OnNotify, drained in OnPostStructuralChanges

        system(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr), m_MyGameMgr(GameMgr) {}

        void OnCreate(void) noexcept
        {
            m_MyGameMgr.m_SceneMgr.m_OnSceneReady.Register<&system::OnSceneReady>(*this);

            // Lets cross-module callers (xlioncore_physics_api.h's exported functions, compiled into
            // THIS DLL, so same-module access to m_Backend's non-exported methods) reach the live
            // instance via GameMgr.getUserData<system>() instead of findSystem<system>() - either
            // would work here, but userdata is the general mechanism now, not physics-specific.
            m_MyGameMgr.setUserData(this);
        }

        void OnDestroy(void) noexcept
        {
            if (m_MyGameMgr.getUserData<system>() == this)
                m_MyGameMgr.setUserData(nullptr);
        }

        static b3BodyType ResolveBodyType(const dynamics* pDyn, bool bIsStatic) noexcept
        {
            if (pDyn) return b3_dynamicBody;
            return bIsStatic ? b3_staticBody : b3_kinematicBody;
        }

        // Shared by OnUpdate (kinetic entities, every frame something changes) and OnSceneReady
        // (statics, once). Builds Box3D creation params from the live component data and (re)creates
        // the body - destroying the old one first if this is a recreate, not a first creation.
        void CreateOrRecreateBody
        ( xlioncore::transform& T, physics_body_properties& BodyProps, physics_shape_properties& ShapeProps
        , box3d_body& Body, b3BodyType ResolvedType, float Mass
        ) noexcept
        {
            if (B3_IS_NON_NULL(Body.m_BodyId))
            {
                m_Backend.DestroyBody(Body.m_BodyId);
                Body.m_BodyId = b3_nullBodyId;
            }

            const xmath::fvec3 ScaledHalfExtents = T.m_Scale * 0.5f;

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

            T.m_DirtyToPhysics = 0;
        }

        // Fired once per scene, when it's about to actually run (Play in the editor; load in a future
        // Game target) - never at scene-load time itself, see this file's own top comment. Creates
        // Box3D bodies for every static entity that doesn't have one yet. Scene.m_LocalToRuntime is
        // already exactly this scene's own entities - no query needed, just a presence check per one.
        void OnSceneReady(xecs::scene::instance& Scene) noexcept
        {
            int nCreated = 0;
            for (auto& Pair : Scene.m_LocalToRuntime)
            {
                const auto Entity = Pair.second;
                if (!hasComponents<xlioncore::static_tag>(Entity)) continue;
                if (!hasComponents<xlioncore::transform, physics_body_properties, physics_shape_properties, box3d_body>(Entity)) continue;

                auto* pBody = GetComponentPtr<box3d_body>(m_MyGameMgr, Entity);
                if (!pBody || B3_IS_NON_NULL(pBody->m_BodyId)) continue;

                auto* pT          = GetComponentPtr<xlioncore::transform>(m_MyGameMgr, Entity);
                auto* pBodyProps  = GetComponentPtr<physics_body_properties>(m_MyGameMgr, Entity);
                auto* pShapeProps = GetComponentPtr<physics_shape_properties>(m_MyGameMgr, Entity);
                if (!pT || !pBodyProps || !pShapeProps) continue;

                CreateOrRecreateBody(*pT, *pBodyProps, *pShapeProps, *pBody, b3_staticBody, 0.0f);
                ++nCreated;
            }
            if (nCreated)
            {
                std::printf("[Physics] OnSceneReady '%s': created %d static bodies\n", Scene.m_Name.c_str(), nCreated);
                std::fflush(stdout);
            }
        }

        // Called by destroy_notify::OnNotify. The actual DestroyBody can't happen synchronously here -
        // something else may still be mid-Foreach over this same archetype at that exact moment (direct
        // user caution: "someone may have a pointer or something to the Box3D object") - so it's queued
        // and drained at two safe points instead: right before this system's own Step() (OnUpdate,
        // below - guarantees a same-frame-or-earlier kill never gets one more simulated tick, without
        // needing an immediate Disable() first - see this file's own history for why that was dropped),
        // and again in OnPostStructuralChanges as a same-frame catch for anything queued after Step()
        // already ran (the readback loop below, or a later system). MakeBodyUnqueryable happens
        // synchronously right here regardless - best-effort, so a raycast/overlap query issued in the
        // gap before the real destroy doesn't match a logically-dead entity (see its own comment).
        void QueueDestroy(b3BodyId Id) noexcept
        {
            m_Backend.MakeBodyUnqueryable(Id);
            m_PendingDestroy.push_back(Id);
        }

        void DrainPendingDestroy(void) noexcept
        {
            for (auto Id : m_PendingDestroy)
                m_Backend.DestroyBody(Id);
            m_PendingDestroy.clear();
        }

        // The direct-call escape hatch for a dynamic body: OnUpdate's per-frame loop deliberately never
        // looks at a dynamic body's Dirty flag (see its own comment) - a live dynamic body is physics-
        // authoritative, and this project's own contract is "call the physics system directly if you
        // really want to move one," not "poke the ECS field and hope something notices." This is that
        // direct call - used today by xscene's TeleportDynamicIfPlaying (an editor-only feature: a
        // gizmo/Inspector edit on a dynamic entity while Playing is a deliberate human action, not
        // gameplay code, so the editor is the caller here, not OnUpdate). Zeros velocity because a
        // teleport has no implied motion - the same semantic every other engine's equivalent API uses
        // (Unity's Rigidbody.position setter, Unreal's SetActorLocation on a simulating body).
        void TeleportBody(box3d_body& Body, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept
        {
            if (B3_IS_NULL(Body.m_BodyId)) return;
            m_Backend.SetTransform(Body.m_BodyId, Position, Rotation);
            m_Backend.SetLinearVelocity(Body.m_BodyId, xmath::fvec3::fromZero());
            m_Backend.SetAngularVelocity(Body.m_BodyId, xmath::fvec3::fromZero());
            // Confirmed live: a settled, sleeping crate teleported to mid-air just sat there forever -
            // SetTransform repositions a sleeping body without waking it, so nothing then integrates
            // it, teleported or not.
            m_Backend.SetAwake(Body.m_BodyId, true);
        }

        void OnPostStructuralChanges(void) noexcept
        {
            DrainPendingDestroy();
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
            Query.m_NoneOf.AddFromComponents< xlioncore::static_tag >();
            auto S = Search(Query);

            int nEnt = 0, nNull = 0, nDyn = 0, nKin = 0, nHasDynComp = 0;
            float yMin = 1e9f, yMax = -1e9f;
            float yFirstDyn = -999.f;

            Foreach(S, [&]( const xecs::component::entity& Entity
                          , xlioncore::transform& T
                          , physics_body_properties& BodyProps
                          , physics_shape_properties& ShapeProps
                          , box3d_body& Body
                          , dynamics* pDyn ) noexcept
            {
                // A sibling killed earlier in THIS SAME pass (a future physics-driven-kill feature,
                // e.g. culling) is still zombie, not yet reclaimed - S was snapshotted before this
                // Foreach started. Nothing to do for something already dead: don't recreate, don't
                // apply forces.
                if (Entity.isZombie()) return;

                ++nEnt;
                if (pDyn) ++nHasDynComp;

                const b3BodyType ResolvedType = ResolveBodyType(pDyn, false);   // never static here - excluded from this query
                const float      Mass         = pDyn ? pDyn->m_Mass : 0.0f;

                if (ResolvedType == b3_dynamicBody) ++nDyn;
                else ++nKin;

                // Only "does a body exist at all" and "did its type change" (static demotion, a
                // dynamics add/remove) still warrant a recreate. Mass/Friction/Restitution/Scale are
                // construction-only data (BodyProps/ShapeProps are SHARE components, seed values read
                // once here) - a live change to any of those after creation is the caller's job, via
                // the physics system's own direct Box3D-backed setters, not something this loop polls
                // for anymore. See xlion_construction_component_idea memory for the fuller design.
                const bool NeedsRecreate = B3_IS_NULL(Body.m_BodyId) || Body.m_CachedBodyType != ResolvedType;

                if (NeedsRecreate)
                {
                    CreateOrRecreateBody(T, BodyProps, ShapeProps, Body, ResolvedType, Mass);
                    if (B3_IS_NULL(Body.m_BodyId)) ++nNull;
                }
                else if (B3_IS_NULL(Body.m_BodyId))
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

            // Editor/gameplay Dirty pose pushes: a dynamic body ignores these entirely (direct user
            // decision - a live body is physics-authoritative; a caller that really wants to move one
            // calls the physics system directly, see TeleportBody/xscene's TeleportDynamicIfPlaying) -
            // scoped out at the QUERY level (none_of<dynamics>), not a runtime check, so a scene with
            // many dynamic crates never pays even one branch for this per entity, per frame. Only
            // entities visited here are guaranteed kinematic (not static - excluded already; not
            // dynamic - excluded here) by construction.
            {
                xecs::query::instance DirtyQuery;
                DirtyQuery.m_Must.AddFromComponents<xlioncore::transform, box3d_body>();
                DirtyQuery.m_NoneOf.AddFromComponents<xlioncore::static_tag, dynamics>();
                auto DirtySet = Search(DirtyQuery);

                Foreach(DirtySet, [&](const xecs::component::entity& Entity, xlioncore::transform& T, box3d_body& Body) noexcept
                {
                    if (Entity.isZombie()) return;
                    if (B3_IS_NULL(Body.m_BodyId)) return;   // not created yet this frame - the main pass above will
                    if (!T.m_DirtyToPhysics) return;

                    m_Backend.SetTransform(Body.m_BodyId, T.m_Position, T.m_Rotation);
                    T.m_DirtyToPhysics = 0;
                });
            }

            // Drain anything queued for destroy since the last drain - including by this very Foreach,
            // if a future feature has physics kill an entity reactively - strictly before Step(), so a
            // same-frame-or-earlier kill never gets one more simulated tick. See QueueDestroy's comment.
            DrainPendingDestroy();

            m_Backend.Step();

            Foreach(S, [&]( xlioncore::transform& T
                          , physics_body_properties&
                          , physics_shape_properties&
                          , box3d_body& Body
                          , dynamics* pDyn ) noexcept
            {
                if (B3_IS_NULL(Body.m_BodyId)) return;

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
                std::printf("[Physics] tick=%d ents=%d hasDyn=%d types(d/k)=%d/%d null=%d yRange=[%.3f,%.3f] yDyn0=%.3f pendingDestroy=%zu\n"
                    , s_Tick, nEnt, nHasDynComp, nDyn, nKin, nNull
                    , (yMin > 1e8f ? 0.f : yMin), (yMax < -1e8f ? 0.f : yMax), yFirstDyn, m_PendingDestroy.size()
                    );
                std::fflush(stdout);
            }
        }
    };

    // Tiny companion notify-system - physics::system itself can't ALSO be NOTIFY_DESTROY (a system's
    // typedef_v.id_v is one kind only). Fires synchronously, immediately, inside DestroyEntity itself
    // (xecs_archetype_inline.h) - BEFORE the entity is zombied or its pool slot freed, so the
    // component data read here is always still live and correct.
    struct destroy_notify : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::notify_destroy{ .m_pName = "Physics Destroy Notify" };
        using query = std::tuple< xecs::query::must< box3d_body > >;

        xecs::game_mgr::instance& m_MyGameMgr;

        destroy_notify(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr), m_MyGameMgr(GameMgr) {}

        void OnNotify(xecs::component::entity& Entity) noexcept
        {
            auto* pBody = GetComponentPtr<box3d_body>(m_MyGameMgr, Entity);
            if (!pBody || B3_IS_NULL(pBody->m_BodyId)) return;
            if (auto* pOwner = findSystem<system>())
                pOwner->QueueDestroy(pBody->m_BodyId);
        }
    };
}

#endif // XLIONCORE_PHYSICS_SYSTEM_H
