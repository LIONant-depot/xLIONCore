#include "xlioncore_physics_world.h"

namespace xlioncore::physics
{
    world::world(void) noexcept
    {
        b3WorldDef WorldDef = b3DefaultWorldDef();
        m_World = b3CreateWorld(&WorldDef);
    }

    world::~world(void) noexcept
    {
        b3DestroyWorld(m_World);
    }

    b3BodyId world::CreateBody(bool bDynamic, const xmath::fvec3& Position, const xmath::fvec3& HalfExtents) noexcept
    {
        b3BodyDef BodyDef = b3DefaultBodyDef();
        BodyDef.type     = bDynamic ? b3_dynamicBody : b3_staticBody;
        BodyDef.position = { Position.m_X, Position.m_Y, Position.m_Z };
        const b3BodyId BodyId = b3CreateBody(m_World, &BodyDef);

        b3ShapeDef ShapeDef = b3DefaultShapeDef();
        b3BoxHull  Hull     = b3MakeBoxHull(HalfExtents.m_X, HalfExtents.m_Y, HalfExtents.m_Z);
        b3CreateHullShape(BodyId, &ShapeDef, &Hull.base);

        return BodyId;
    }

    void world::Step(void) noexcept
    {
        constexpr float FixedDt      = 1.0f / 60.0f;
        constexpr int   SubStepCount = 4;
        b3World_Step(m_World, FixedDt, SubStepCount);
    }

    xmath::fvec3 world::GetPosition(b3BodyId BodyId) const noexcept
    {
        const b3Vec3 P = b3Body_GetPosition(BodyId);
        return { P.x, P.y, P.z };
    }
}
