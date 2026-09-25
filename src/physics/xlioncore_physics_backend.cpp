#include "xlioncore_physics_backend.h"

namespace xlioncore::physics
{
    namespace
    {
        b3Quat ToB3(const xmath::fquat& Q) noexcept
        {
            return b3Quat{ { Q.m_X, Q.m_Y, Q.m_Z }, Q.m_W };
        }

        xmath::fquat FromB3(const b3Quat& Q) noexcept
        {
            return xmath::fquat{ Q.v.x, Q.v.y, Q.v.z, Q.s };
        }
    }

    backend::backend(void) noexcept
    {
        b3WorldDef WorldDef = b3DefaultWorldDef();
        m_World = b3CreateWorld(&WorldDef);
    }

    backend::~backend(void) noexcept
    {
        b3DestroyWorld(m_World);
    }

    b3BodyId backend::CreateBody( bool bDynamic
                                , const xmath::fvec3& Position
                                , const xmath::fquat& Rotation
                                , const xmath::fvec3& HalfExtents ) noexcept
    {
        b3BodyDef BodyDef = b3DefaultBodyDef();
        BodyDef.type     = bDynamic ? b3_dynamicBody : b3_staticBody;
        BodyDef.position = { Position.m_X, Position.m_Y, Position.m_Z };
        BodyDef.rotation = ToB3(Rotation);
        const b3BodyId BodyId = b3CreateBody(m_World, &BodyDef);

        b3ShapeDef ShapeDef = b3DefaultShapeDef();
        b3BoxHull  Hull     = b3MakeBoxHull(HalfExtents.m_X, HalfExtents.m_Y, HalfExtents.m_Z);
        b3CreateHullShape(BodyId, &ShapeDef, &Hull.base);

        return BodyId;
    }

    void backend::Step(void) noexcept
    {
        constexpr float FixedDt      = 1.0f / 60.0f;
        constexpr int   SubStepCount = 4;
        b3World_Step(m_World, FixedDt, SubStepCount);
    }

    xmath::fvec3 backend::GetPosition(b3BodyId BodyId) const noexcept
    {
        const b3Vec3 P = b3Body_GetPosition(BodyId);
        return { P.x, P.y, P.z };
    }

    xmath::fquat backend::GetRotation(b3BodyId BodyId) const noexcept
    {
        return FromB3(b3Body_GetRotation(BodyId));
    }

    void backend::SetTransform(b3BodyId BodyId, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept
    {
        b3Body_SetTransform(BodyId, { Position.m_X, Position.m_Y, Position.m_Z }, ToB3(Rotation));
    }

    void backend::DestroyBody(b3BodyId BodyId) noexcept
    {
        if (B3_IS_NON_NULL(BodyId))
            b3DestroyBody(BodyId);
    }
}
