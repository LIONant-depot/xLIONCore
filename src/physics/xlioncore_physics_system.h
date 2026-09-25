#ifndef XLIONCORE_PHYSICS_SYSTEM_H
#define XLIONCORE_PHYSICS_SYSTEM_H
#pragma once

// The physics system itself - fully inline, like every other user-authored xecs system in this
// codebase (e.g. xlevel::tick_logger_a/b). This is NOT optional here: any code that touches a user
// component type by name (Search/Foreach/getOrCreateArchetype - all of xecs_system.h's query
// machinery) reads xecs::component::type::info_v<T>, a per-binary inline static, so it must be
// compiled into whichever binary actually calls RegisterComponents<T>() (xLION.exe, via
// xlevel_session.h) - hiding it inside LIONCore.dll instead reads the DLL's own separate, never-
// registered copy of rigid_body's metadata (confirmed live: garbage pointers, a crash on the very
// first CreateEntity). box3d itself still stays entirely inside the DLL - see xlioncore_physics_world.h,
// which this system only ever calls through, never touching a b3* function directly.
#include "xlioncore_physics.h"
#include "xlioncore_physics_world.h"
#include <cstdio>

namespace xlioncore::physics
{
    struct system : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::update{ .m_pName = "Physics" };
        using query = std::tuple<xecs::query::must<rigid_body>>;

        world m_World;

        system(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnCreate(void) noexcept
        {
            // Temporary proof-of-life: a static ground plane and one dynamic box dropped above it, so
            // there is something real to observe falling under gravity without needing the Level
            // Editor's own entity-creation UI wired up yet. Remove once physics is exposed there instead.
            auto& Archetype = getOrCreateArchetype<rigid_body>();

            Archetype.CreateEntity([](rigid_body& RB) noexcept
            {
                RB.m_bDynamic    = false;
                RB.m_Position    = { 0.0f, 0.0f, 0.0f };
                RB.m_HalfExtents = { 50.0f, 1.0f, 50.0f };
            });

            Archetype.CreateEntity([](rigid_body& RB) noexcept
            {
                RB.m_bDynamic    = true;
                RB.m_Position    = { 0.0f, 10.0f, 0.0f };
                RB.m_HalfExtents = { 0.5f, 0.5f, 0.5f };
            });
        }

        void OnUpdate(void) noexcept
        {
            xecs::query::instance Query;
            Query.m_Must.AddFromComponents<rigid_body>();
            auto S = Search(Query);

            // Lazily create the box3d body/shape the first time this entity is seen
            Foreach(S, [&](rigid_body& RB) noexcept
            {
                if (B3_IS_NON_NULL(RB.m_BodyId)) return;
                RB.m_BodyId = m_World.CreateBody(RB.m_bDynamic, RB.m_Position, RB.m_HalfExtents);
            });

            m_World.Step();

            // Dynamic bodies moved - pull the new position back into the component
            Foreach(S, [&](rigid_body& RB) noexcept
            {
                if (!RB.m_bDynamic) return;
                RB.m_Position = m_World.GetPosition(RB.m_BodyId);

                // Temporary proof-of-life printf - remove once this is visible in the Inspector/viewport instead.
                std::printf("[Physics] dynamic body Y = %f\n", RB.m_Position.m_Y);
                std::fflush(stdout);
            });
        }
    };
}

#endif // XLIONCORE_PHYSICS_SYSTEM_H
