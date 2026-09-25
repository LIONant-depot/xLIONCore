#ifndef XLIONCORE_PHYSICS_SYSTEM_H
#define XLIONCORE_PHYSICS_SYSTEM_H
#pragma once

// Physics system - compiled entirely inside LIONCore.dll (see xlioncore_plugin_entry.cpp, the ONLY
// place RegisterComponents / RegisterSystems for these types are called). Requires xlioncore::transform
// + rigid_body; pose is read/written on Transform, collider/body flags stay on rigid_body.
// Collider half-extents = 0.5 * Transform.Scale (unit mesh / unit cube convention).
#include "xlioncore_physics.h"
#include "xlioncore_physics_backend.h"
#include "../transform/xlioncore_transform.h"
#include <cstdio>

namespace xlioncore::physics
{
    struct system : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::update{ .m_pName = "Physics" };
        using query = std::tuple<xecs::query::must<xlioncore::transform, rigid_body>>;

        backend m_Backend;

        system(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnUpdate(void) noexcept
        {
            xecs::query::instance Query;
            Query.m_Must.AddFromComponents<xlioncore::transform, rigid_body>();
            auto S = Search(Query);

            // Lazily create (or recreate when 0.5*Scale drifts) so collider matches visuals
            Foreach(S, [&](xlioncore::transform& T, rigid_body& RB) noexcept
            {
                const xmath::fvec3 ScaledHalfExtents = T.m_Scale * 0.5f;
                if (B3_IS_NON_NULL(RB.m_BodyId))
                {
                    if (RB.m_BodyHalfExtents == ScaledHalfExtents) return;
                    m_Backend.DestroyBody(RB.m_BodyId);
                    RB.m_BodyId = b3_nullBodyId;
                }
                RB.m_BodyId = m_Backend.CreateBody(RB.m_bDynamic, T.m_Position, T.m_Rotation, ScaledHalfExtents);
                RB.m_BodyHalfExtents = ScaledHalfExtents;
            });

            m_Backend.Step();

            // Dynamic bodies moved - pull position + rotation back into Transform
            Foreach(S, [&](xlioncore::transform& T, rigid_body& RB) noexcept
            {
                if (!RB.m_bDynamic) return;
                T.m_Position = m_Backend.GetPosition(RB.m_BodyId);
                T.m_Rotation = m_Backend.GetRotation(RB.m_BodyId);
            });
        }
    };
}

#endif // XLIONCORE_PHYSICS_SYSTEM_H
