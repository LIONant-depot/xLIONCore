#ifndef XLIONCORE_PHYSICS_SYSTEM_H
#define XLIONCORE_PHYSICS_SYSTEM_H
#pragma once

// Physics lifecycle:
//   Creation: body_builder (a builder system, doc/xecs_builder_components.md) hands the entity's builder
//   components (PhysicsBodyProperties and the collider components) to Box3D once, while the entity is being
//   created. Box3D owns that data from then on; the entity keeps only physics_body's handle.
//   Body type: has physics_dynamics => Dynamic; else static_tag => Static; else Kinematic.
//   Per frame (system::OnUpdate): statics are skipped entirely (none_of<static_tag>). Force/torque are
//   applied, kinematic Dirty poses pushed, the world stepped, poses/velocities read back. A dynamic body
//   ignores Dirty pushes - it's physics-authoritative; callers move it through TeleportBody instead.
//   A type change (static demotion while Playing, dynamics added/removed) is applied in place with
//   SetBodyType - the builder components needed to recreate a body no longer exist.
//   Destruction: destroy_notify queues the BodyId; it's destroyed before the next Step or after the
//   structural-changes flush, never mid-Foreach.
#include "xlioncore_physics.h"
#include "xlioncore_physics_backend.h"
#include "../transform/xlioncore_transform.h"
#include "../tags/xlioncore_tags.h"
#include "../game/xlioncore_game.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace xlioncore::physics
{
    // Resolves a component's live pointer for an arbitrary entity, DATA or SHARE, outside of any
    // Foreach - for a caller that needs raw pool access (a system passes its getGameMgr())
    // for a handle NOT currently being iterated (OnSceneReady walks Scene.m_LocalToRuntime directly,
    // not a Search() result; destroy_notify::OnNotify gets a handle from an archetype event, not a
    // query), so there is no Foreach to hand it the component. Mirrors xscene::ResolveComponentPointer's exact
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
        // What OnUpdate actually touches. none_of<static_tag>: statics never need revisiting once built. none_of<parent>: a child's Transform is relative to its parent, so it cannot be
        // a body (only a root is: see xlioncore_hierarchy.h).
        using query = std::tuple
            < xecs::query::must     < xlioncore::transform, physics_body >
            , xecs::query::none_of  < xlioncore::static_tag, xecs::component::parent >
            , xecs::query::optional < physics_dynamics >
            >;

        backend                    m_Backend;

        // The places where other systems connect to the physics: they run once for each fixed step.
        static constexpr std::array<xecs::system::connector, 2> connectors_v
        { { { "Before Step", "Runs once for every fixed step, right before the world takes it: where the systems that push on bodies (forces, kicks, a person running) belong" }
          , { "After Step",  "Runs once for every fixed step, right after the world took it: where the systems that read what the step did belong" }
        } };

        std::vector<b3BodyId>      m_PendingDestroy;   // queued by destroy_notify::OnNotify, drained in OnPostStructuralChanges
        std::vector<sensor_touch>  m_SensorBegin, m_SensorEnd;      // what the last step reported (kept to reuse the memory)
        std::vector<contact_touch> m_ContactBegin, m_ContactEnd;
        std::vector<contact_hit>   m_ContactHit;

        system(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnCreate(void) noexcept
        {
            // The game this world belongs to (the user data of the game manager) keeps the live physics: cross-module callers
            // (xlioncore_physics_api.h's exported functions, compiled into THIS DLL, so same-module access to m_Backend's
            // non-exported methods) reach it through Game.m_pPhysics.
            if (auto* pGame = game::From(getGameMgr())) pGame->m_pPhysics = this;
        }

        void OnDestroy(void) noexcept
        {
            if (auto* pGame = game::From(getGameMgr()); pGame && pGame->m_pPhysics == this)
                pGame->m_pPhysics = nullptr;
        }

        static b3BodyType ResolveBodyType(const physics_dynamics* pDyn, bool bIsStatic) noexcept
        {
            if (pDyn) return b3_dynamicBody;
            return bIsStatic ? b3_staticBody : b3_kinematicBody;
        }

        // PhysicsMaterial assets, read from the project once per world (descriptor-only resources).
        const material::descriptor& getMaterial( const material::ref& Ref ) noexcept
        {
            const std::uint64_t Key = Ref.m_Instance.m_Value;
            if( auto It = m_Materials.find(Key); It != m_Materials.end() ) return It->second;
            return m_Materials.emplace( Key, material::Load( getGameMgr().m_SceneMgr.m_ProjectPath, Key ) ).first->second;
        }

        // Called once per entity by body_builder, while the entity is being created: the body, then
        // every shape, then - for a dynamic body - the PhysicsDynamics values as the body's starting state:
        //   Mass             the mass authority (shape densities only shape the inertia),
        //   Linear/Angular   the initial velocities (the body leaves the spawn point already moving/spinning),
        //   Force/Torque     stay in the component: OnUpdate applies them before the first Step and then
        //                    clears them, so they push on the very first simulated tick without the
        //                    builder having to (and without applying them twice).
        void CreateBody
        ( xecs::component::entity Entity, xlioncore::transform& T, const physics_body_properties& BodyProps
        , std::span<const shape_params> Shapes, physics_body& Body, b3BodyType ResolvedType, const physics_dynamics* pDyn
        ) noexcept
        {
            body_create_params Params;
            Params.m_Type           = ResolvedType;
            Params.m_LinearDamping  = BodyProps.m_LinearDamping;
            Params.m_AngularDamping = BodyProps.m_AngularDamping;
            Params.m_EnableSleep    = BodyProps.m_EnableSleep;
            Params.m_IsBullet       = BodyProps.m_EnableContinuousCollision;
            if (pDyn)                                                           // only a dynamic body has anything to constrain
                Params.m_Locks = { pDyn->m_ConstraintsTranslationX, pDyn->m_ConstraintsTranslationY, pDyn->m_ConstraintsTranslationZ
                                 , pDyn->m_ConstraintsRotationX, pDyn->m_ConstraintsRotationY, pDyn->m_ConstraintsRotationZ };
            Params.m_Position       = T.m_Position;
            Params.m_Rotation       = T.m_Rotation;
            Params.m_UserData       = Entity.m_Value;

            Body.m_BodyId = m_Backend.CreateBody(Params);
            for( auto& Shape : Shapes )
                m_Backend.AddShape( Body.m_BodyId, Shape );

            const float Mass = pDyn ? pDyn->m_Mass : 0.0f;
            if( ResolvedType == b3_dynamicBody )
            {
                if( Mass > 0.0f ) m_Backend.SetMass( Body.m_BodyId, Mass );

                if( pDyn )
                {
                    m_Backend.SetLinearVelocity ( Body.m_BodyId, pDyn->m_LinearVelocity );
                    m_Backend.SetAngularVelocity( Body.m_BodyId, pDyn->m_AngularVelocity );
                }
            }

            Body.m_CachedBodyType    = ResolvedType;
            Body.m_CachedMass        = Mass;
            Body.m_CachedConstraints = pDyn ? pDyn->constraintBits() : 0;

            T.m_DirtyToPhysics = 0;
        }

        std::unordered_map<std::uint64_t, material::descriptor> m_Materials;

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
        void TeleportBody(physics_body& Body, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept
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

        // What the step just taken says about sensors, to the game as events (see sensor_begin_event). Sent here, outside any Foreach: a handler may create or destroy entities.
        void SendSensorEvents(void) noexcept
        {
            m_Backend.DrainSensorEvents(m_SensorBegin, m_SensorEnd);
            for (const auto& Touch : m_SensorBegin)
                getGameMgr().SendGlobalEvent<sensor_begin_event>(Touch);
            for (const auto& Touch : m_SensorEnd)
                getGameMgr().SendGlobalEvent<sensor_end_event>(Touch);

            // The same for the solid contacts of the shapes that asked (see contact_begin_event).
            m_Backend.DrainContactEvents(m_ContactBegin, m_ContactEnd, m_ContactHit);
            for (const auto& Touch : m_ContactBegin)
                getGameMgr().SendGlobalEvent<contact_begin_event>(Touch);
            for (const auto& Hit : m_ContactHit)
                getGameMgr().SendGlobalEvent<contact_hit_event>(Hit);
            for (const auto& Touch : m_ContactEnd)
                getGameMgr().SendGlobalEvent<contact_end_event>(Touch);
        }

        void OnUpdate(void) noexcept
        {
            static int s_Tick = 0;
            ++s_Tick;
            const bool bLog = (s_Tick <= 3) || ((s_Tick % 60) == 0);

            xecs::query::instance Query;
            Query.m_Must.AddFromComponents< xlioncore::transform, physics_body >();
            Query.m_NoneOf.AddFromComponents< xlioncore::static_tag, xecs::component::parent >();
            auto S = Search(Query);

            int nEnt = 0, nNull = 0, nDyn = 0, nKin = 0, nHasDynComp = 0;
            float yMin = 1e9f, yMax = -1e9f;
            float yFirstDyn = -999.f;

            // The fixed steps the game's time says are due this frame (see game_time): the world takes exactly that many, and the systems
            // connected to this one run around each of them (Before Step: they push on the bodies, After Step: they read what happened).
            const auto* pGame = game::From(getGameMgr());
            const int   nSteps = pGame ? pGame->m_Time.m_FixedSteps : 0;
            for (int Step = 0; Step < nSteps; ++Step)
            {
                RunConnector(0);
                nEnt = nNull = nDyn = nKin = nHasDynComp = 0;
                Foreach(S, [&]( const xecs::component::entity& Entity
                              , physics_body& Body
                              , physics_dynamics* pDyn ) noexcept
                {
                    // A sibling killed earlier in THIS SAME pass (a future physics-driven-kill feature,
                    // e.g. culling) is still zombie, not yet reclaimed - S was snapshotted before this
                    // Foreach started. Nothing to do for something already dead: don't recreate, don't
                    // apply forces.
                    if (Entity.isZombie()) return;

                    ++nEnt;
                    if (pDyn) ++nHasDynComp;

                    const b3BodyType ResolvedType = ResolveBodyType(pDyn, false);   // never static here - excluded from this query

                    if (ResolvedType == b3_dynamicBody) ++nDyn;
                    else ++nKin;

                    // No body = its builder never ran (or failed) - nothing here can create one, the
                    // builder components it would need are gone. See body_builder.
                    if (B3_IS_NULL(Body.m_BodyId))
                    {
                        ++nNull;
                        return;
                    }

                    // Static demotion while Playing, or physics_dynamics added/removed.
                    if (Body.m_CachedBodyType != ResolvedType)
                    {
                        m_Backend.SetBodyType(Body.m_BodyId, ResolvedType);
                        if (ResolvedType == b3_dynamicBody) m_Backend.SetMass(Body.m_BodyId, pDyn->m_Mass);
                        m_Backend.SetAwake(Body.m_BodyId, true);
                        Body.m_CachedBodyType = ResolvedType;
                        Body.m_CachedMass     = pDyn ? pDyn->m_Mass : 0.0f;
                    }

                    if (pDyn && B3_IS_NON_NULL(Body.m_BodyId) && ResolvedType == b3_dynamicBody)
                    {
                        // Constraints are live: a change made while playing reaches the body at once.
                        if (const std::uint8_t Bits = pDyn->constraintBits(); Bits != Body.m_CachedConstraints)
                        {
                            m_Backend.SetMotionLocks(Body.m_BodyId, Bits);
                            m_Backend.SetAwake(Body.m_BodyId, true);
                            Body.m_CachedConstraints = Bits;
                        }
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
                // scoped out at the QUERY level (none_of<physics_dynamics>), not a runtime check, so a scene with
                // many dynamic crates never pays even one branch for this per entity, per frame. Only
                // entities visited here are guaranteed kinematic (not static - excluded already; not
                // dynamic - excluded here) by construction.
                {
                    xecs::query::instance DirtyQuery;
                    DirtyQuery.m_Must.AddFromComponents<xlioncore::transform, physics_body>();
                    DirtyQuery.m_NoneOf.AddFromComponents<xlioncore::static_tag, physics_dynamics, xecs::component::parent>();
                    auto DirtySet = Search(DirtyQuery);

                    Foreach(DirtySet, [&](const xecs::component::entity& Entity, xlioncore::transform& T, physics_body& Body) noexcept
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

                m_Backend.Step(pGame->m_Time.m_FixedDeltaTime);
                SendSensorEvents();
                RunConnector(1);
            }

            // A frame without a fixed step changed nothing in the world: reading the poses back then would overwrite what was edited since
            // the last step (an edit reaches the body in the next step's Dirty push, above).
            if (nSteps > 0)
            Foreach(S, [&]( xlioncore::transform& T
                          , physics_body& Body
                          , physics_dynamics* pDyn ) noexcept
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

            if (bLog && nSteps > 0)
            {
                std::printf("[Physics] tick=%d ents=%d hasDyn=%d types(d/k)=%d/%d null=%d yRange=[%.3f,%.3f] yDyn0=%.3f pendingDestroy=%zu\n"
                    , s_Tick, nEnt, nHasDynComp, nDyn, nKin, nNull
                    , (yMin > 1e8f ? 0.f : yMin), (yMax < -1e8f ? 0.f : yMax), yFirstDyn, m_PendingDestroy.size()
                    );
                std::fflush(stdout);
            }
        }
    };

    // Hands an entity's physics builder components to Box3D while the entity is being created (see
    // doc/xecs_builder_components.md). Runs once per entity; the builder components are dropped
    // right after, so Box3D (through physics_body's handle) is the only owner of this data.
    struct body_builder : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::builder{ .m_pName = "Physics Body Builder" };

        using xecs::system::instance::instance;

        void operator()( const xecs::component::entity&   Entity
                       , xlioncore::transform&             T
                       , const physics_body_properties&    BodyProps
                       , physics_body&                     Body
                       , const physics_collider_box*       pBoxes
                       , const physics_collider_sphere*    pSpheres
                       , const physics_collider_capsule*   pCapsules
                       , const physics_collider_cylinder*  pCylinders
                       , const physics_dynamics*           pDyn ) noexcept
        {
            auto&            Physics  = getSystem<system>();
            const b3BodyType Type     = system::ResolveBodyType( pDyn, hasComponents<xlioncore::static_tag>(Entity) );
            const bool       bDynamic = (Type == b3_dynamicBody);

            std::vector<shape_params> Shapes;

            // What every collider shape shares: the material, the body's collision filter, the sensor and contact-event flags.
            const auto Add = [&]( shape_params::kind Kind, const material::ref& MaterialRef, bool bIsSensor, bool bContactEvents
                                , const xmath::fvec3& Center, const xmath::fquat& Orientation ) -> shape_params&
            {
                const auto& Material = Physics.getMaterial( MaterialRef );

                shape_params& S = Shapes.emplace_back();
                S.m_Kind          = Kind;
                S.m_LocalPosition = Center * T.m_Scale;
                S.m_LocalRotation = Orientation;
                S.m_Density       = bDynamic ? Material.m_Density : 0.0f;
                S.m_Friction      = Material.m_Friction;
                S.m_Restitution   = Material.m_Restitution;
                S.m_CategoryBits  = BodyProps.m_CategoryBits;
                S.m_MaskBits      = BodyProps.m_MaskBits;
                S.m_GroupIndex    = BodyProps.m_GroupIndex;
                S.m_IsSensor      = bIsSensor;
                S.m_ContactEvents = bContactEvents;
                return S;
            };

            // Scaling rules live in collider_scale (xlioncore_physics_collider.h) - shared with the editor tools.
            if( pBoxes )
                for( auto& Box : pBoxes->m_Boxes )
                    Add( shape_params::kind::BOX, Box.m_Material, Box.m_IsSensor, Box.m_ContactEvents, Box.m_Center, Box.m_Orientation ).m_HalfExtents = Box.m_Size * T.m_Scale * 0.5f;

            if( pSpheres )
                for( auto& Sphere : pSpheres->m_Spheres )
                    Add( shape_params::kind::SPHERE, Sphere.m_Material, Sphere.m_IsSensor, Sphere.m_ContactEvents, Sphere.m_Center, xmath::fquat::fromIdentity() ).m_Radius = collider_scale::Sphere( Sphere, T.m_Scale );

            if( pCapsules )
                for( auto& Capsule : pCapsules->m_Capsules )
                {
                    const auto Size = collider_scale::Capsule( Capsule, T.m_Scale );
                    auto& S = Add( shape_params::kind::CAPSULE, Capsule.m_Material, Capsule.m_IsSensor, Capsule.m_ContactEvents, Capsule.m_Center, Capsule.m_Orientation );
                    S.m_Radius     = Size.m_Radius;
                    S.m_HalfHeight = Size.m_HalfSpine;
                }

            if( pCylinders )
                for( auto& Cylinder : pCylinders->m_Cylinders )
                {
                    const auto Size = collider_scale::Cylinder( Cylinder, T.m_Scale );
                    auto& S = Add( shape_params::kind::CYLINDER, Cylinder.m_Material, Cylinder.m_IsSensor, Cylinder.m_ContactEvents, Cylinder.m_Center, Cylinder.m_Orientation );
                    S.m_Radius     = Size.m_Radius;
                    S.m_HalfHeight = Size.m_HalfHeight;
                }

            if( Shapes.empty() )
            {
                std::printf("[Physics] WARNING: entity 0x%llX has PhysicsBodyProperties but no collider - no body created\n", static_cast<unsigned long long>(Entity.m_Value));
                std::fflush(stdout);
                return;
            }

            Physics.CreateBody( Entity, T, BodyProps, Shapes, Body, Type, pDyn );
        }
    };

    // Tiny companion notify-system - physics::system itself can't ALSO be NOTIFY_DESTROY (a system's
    // typedef_v.id_v is one kind only). Fires synchronously, immediately, inside DestroyEntity itself
    // (xecs_archetype_inline.h) - BEFORE the entity is zombied or its pool slot freed, so the
    // component data read here is always still live and correct.
    struct destroy_notify : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::notify_destroy{ .m_pName = "Physics Destroy Notify" };
        using query = std::tuple< xecs::query::must< physics_body > >;

        destroy_notify(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnNotify(xecs::component::entity& Entity) noexcept
        {
            auto* pBody = GetComponentPtr<physics_body>(getGameMgr(), Entity);
            if (!pBody || B3_IS_NULL(pBody->m_BodyId)) return;
            if (auto* pOwner = findSystem<system>())
                pOwner->QueueDestroy(pBody->m_BodyId);
        }
    };
}

#endif // XLIONCORE_PHYSICS_SYSTEM_H
