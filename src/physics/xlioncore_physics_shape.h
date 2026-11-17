#ifndef XLIONCORE_PHYSICS_SHAPE_H
#define XLIONCORE_PHYSICS_SHAPE_H
#pragma once

// The shape of a physics query (plan 13.2: xlioncore_physics_queries.h, the exported CastShapeClosest / OverlapShape in
// xlioncore_physics_api.h). No Box3D here: game code sees only this.
#include "dependencies/xmath/source/xmath.h"
#include <cstdint>

namespace xlioncore::physics
{
    // The shape of a query, relative to its origin
    struct query_shape
    {
        enum class kind : std::uint8_t { SPHERE, CAPSULE, BOX };
        kind            m_Kind        = kind::SPHERE;
        float           m_Radius      = 0.5f;                       // SPHERE, CAPSULE
        xmath::fvec3    m_A{}, m_B{};                               // CAPSULE: its two cap centres (SPHERE: m_A is the centre)
        xmath::fvec3    m_HalfExtents = xmath::fvec3::fromOne() * 0.5f;     // BOX
        xmath::fquat    m_Rotation    = xmath::fquat::fromIdentity();       // BOX

        static query_shape Sphere (float Radius) noexcept { query_shape S; S.m_Radius = Radius; return S; }
        static query_shape Capsule(const xmath::fvec3& A, const xmath::fvec3& B, float Radius) noexcept { query_shape S; S.m_Kind = kind::CAPSULE; S.m_A = A; S.m_B = B; S.m_Radius = Radius; return S; }
        static query_shape Box    (const xmath::fvec3& Half, const xmath::fquat& R = xmath::fquat::fromIdentity()) noexcept { query_shape S; S.m_Kind = kind::BOX; S.m_HalfExtents = Half; S.m_Rotation = R; return S; }
    };

}

#endif
