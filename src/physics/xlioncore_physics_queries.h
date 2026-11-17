#ifndef XLIONCORE_PHYSICS_QUERIES_H
#define XLIONCORE_PHYSICS_QUERIES_H
#pragma once

// Shape casts and overlaps (plan 13.2) on a Box3D world: a sphere, a capsule or a box swept along a translation (the first thing it hits:
// a foot's ground probe with a size, a sword's swing, a thrown ball's path) or tested where it is (who is in this volume: a tackle's reach,
// an explosion). Box3D takes a shape as a point cloud with a radius (b3ShapeProxy): a sphere is one point, a capsule two, a box its eight
// corners. Header-only on Box3D (the exported, game-facing wrappers are in xlioncore_physics_api.h); tested by tests/test_physics_queries.cpp.
#include <box3d/box3d.h>
#include "xlioncore_physics_shape.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace xlioncore::physics
{
    // What a cast hit (the same as a ray's: xlioncore_physics_api.h ray_hit), with the Box3D shape
    struct cast_hit
    {
        bool            m_bHit     = false;
        xmath::fvec3    m_Point{}, m_Normal{ 0, 1, 0 };
        float           m_Fraction = 1;
        b3ShapeId       m_Shape    = b3_nullShapeId;
    };

    namespace queries
    {
        struct proxy { std::array<b3Vec3, 8> m_Points{}; b3ShapeProxy m_Proxy{}; };
        inline void MakeProxy(const query_shape& S, proxy& P) noexcept
        {
            const auto V = [](const xmath::fvec3& v) noexcept { return b3Vec3{ v.m_X, v.m_Y, v.m_Z }; };
            int n = 0;
            switch (S.m_Kind)
            {
            case query_shape::kind::SPHERE:  P.m_Points[n++] = V(S.m_A); break;
            case query_shape::kind::CAPSULE: P.m_Points[n++] = V(S.m_A); P.m_Points[n++] = V(S.m_B); break;
            case query_shape::kind::BOX:
                for (int i = 0; i < 8; ++i)
                {
                    const auto& H = S.m_HalfExtents;
                    P.m_Points[n++] = V(S.m_Rotation * xmath::fvec3((i & 1) ? H.m_X : -H.m_X, (i & 2) ? H.m_Y : -H.m_Y, (i & 4) ? H.m_Z : -H.m_Z));
                }
                break;
            }
            P.m_Proxy = { P.m_Points.data(), n, S.m_Kind == query_shape::kind::BOX ? 0.0f : std::max(S.m_Radius, 0.0f) };
        }
    }

    // The first shape the query shape hits moving from Origin along Translation. Sensors are skipped; Skip (optional) leaves shapes out (the
    // caster's own). A shape already overlapping at the start is a hit at fraction 0.
    template <class T_SKIP = bool(*)(b3ShapeId)>
    inline cast_hit CastShapeClosest(b3WorldId World, const xmath::fvec3& Origin, const query_shape& S, const xmath::fvec3& Translation, b3QueryFilter Filter = b3DefaultQueryFilter(), T_SKIP&& Skip = [](b3ShapeId) { return false; }) noexcept
    {
        queries::proxy P; queries::MakeProxy(S, P);
        struct ctx { cast_hit m_Hit; T_SKIP* m_pSkip; } C{ {}, &Skip };
        b3World_CastShape(World, b3Pos{ Origin.m_X, Origin.m_Y, Origin.m_Z }, &P.m_Proxy, b3Vec3{ Translation.m_X, Translation.m_Y, Translation.m_Z }, Filter
            , [](b3ShapeId Shape, b3Pos Point, b3Vec3 Normal, float Fraction, uint64_t, int, int, void* p) -> float
            {
                auto& C = *static_cast<ctx*>(p);
                if (b3Shape_IsSensor(Shape) || (*C.m_pSkip)(Shape)) return -1.0f;             // (-1: ignore this one, go on)
                if (!C.m_Hit.m_bHit || Fraction < C.m_Hit.m_Fraction)
                    C.m_Hit = { true, xmath::fvec3(float(Point.x), float(Point.y), float(Point.z)), xmath::fvec3(Normal.x, Normal.y, Normal.z), Fraction, Shape };
                return Fraction;                                                                // (clip the cast here: only closer hits after this)
            }, &C);
        return C.m_Hit;
    }

    // Every shape the query shape overlaps where it is (Origin), sensors skipped; each body once. Out: the bodies.
    inline void OverlapShape(b3WorldId World, const xmath::fvec3& Origin, const query_shape& S, std::vector<b3BodyId>& Out, b3QueryFilter Filter = b3DefaultQueryFilter()) noexcept
    {
        queries::proxy P; queries::MakeProxy(S, P);
        Out.clear();
        b3World_OverlapShape(World, b3Pos{ Origin.m_X, Origin.m_Y, Origin.m_Z }, &P.m_Proxy, Filter, [](b3ShapeId Shape, void* p) -> bool
        {
            auto& Out = *static_cast<std::vector<b3BodyId>*>(p);
            if (b3Shape_IsSensor(Shape)) return true;
            const auto B = b3Shape_GetBody(Shape);
            if (std::none_of(Out.begin(), Out.end(), [&](b3BodyId X) { return X.index1 == B.index1 && X.generation == B.generation; })) Out.push_back(B);
            return true;
        }, &Out);
    }
}

#endif
