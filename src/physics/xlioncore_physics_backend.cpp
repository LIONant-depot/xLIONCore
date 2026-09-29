#include "xlioncore_physics_backend.h"
#include <cstring>
#include <array>

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

        bool IsIdentityLocal(const xmath::fvec3& P, const xmath::fquat& R) noexcept
        {
            return P.m_X == 0.0f && P.m_Y == 0.0f && P.m_Z == 0.0f
                && R.m_X == 0.0f && R.m_Y == 0.0f && R.m_Z == 0.0f && R.m_W == 1.0f;
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

    b3BodyId backend::CreateBody(const body_create_params& Params) noexcept
    {
        b3BodyDef BodyDef = b3DefaultBodyDef();
        BodyDef.type           = Params.m_Type;
        BodyDef.position       = { Params.m_Position.m_X, Params.m_Position.m_Y, Params.m_Position.m_Z };
        BodyDef.rotation       = ToB3(Params.m_Rotation);
        BodyDef.linearDamping  = Params.m_LinearDamping;
        BodyDef.angularDamping = Params.m_AngularDamping;
        BodyDef.enableSleep    = Params.m_EnableSleep;
        BodyDef.isBullet       = Params.m_IsBullet;
        BodyDef.userData       = reinterpret_cast<void*>(static_cast<std::uintptr_t>(Params.m_UserData));
        return b3CreateBody(m_World, &BodyDef);
    }

    void backend::AddBoxShape(b3BodyId BodyId, const box_shape_params& Params) noexcept
    {
        b3ShapeDef ShapeDef = b3DefaultShapeDef();
        ShapeDef.density                    = Params.m_Density;
        ShapeDef.baseMaterial.friction      = Params.m_Friction;
        ShapeDef.baseMaterial.restitution   = Params.m_Restitution;
        ShapeDef.filter.categoryBits        = Params.m_CategoryBits;
        ShapeDef.filter.maskBits            = Params.m_MaskBits;
        ShapeDef.filter.groupIndex          = Params.m_GroupIndex;
        ShapeDef.isSensor                   = Params.m_IsSensor;
        ShapeDef.updateBodyMass             = true;

        if (IsIdentityLocal(Params.m_LocalPosition, Params.m_LocalRotation))
        {
            b3BoxHull Hull = b3MakeBoxHull(Params.m_HalfExtents.m_X, Params.m_HalfExtents.m_Y, Params.m_HalfExtents.m_Z);
            b3CreateHullShape(BodyId, &ShapeDef, &Hull.base);
        }
        else
        {
            b3Transform Local{};
            Local.p = { Params.m_LocalPosition.m_X, Params.m_LocalPosition.m_Y, Params.m_LocalPosition.m_Z };
            Local.q = ToB3(Params.m_LocalRotation);
            b3BoxHull Hull = b3MakeTransformedBoxHull(
                Params.m_HalfExtents.m_X, Params.m_HalfExtents.m_Y, Params.m_HalfExtents.m_Z, Local);
            b3CreateHullShape(BodyId, &ShapeDef, &Hull.base);
        }
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

    xmath::fvec3 backend::GetLinearVelocity(b3BodyId BodyId) const noexcept
    {
        const b3Vec3 V = b3Body_GetLinearVelocity(BodyId);
        return { V.x, V.y, V.z };
    }

    xmath::fvec3 backend::GetAngularVelocity(b3BodyId BodyId) const noexcept
    {
        const b3Vec3 V = b3Body_GetAngularVelocity(BodyId);
        return { V.x, V.y, V.z };
    }

    void backend::SetTransform(b3BodyId BodyId, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept
    {
        b3Body_SetTransform(BodyId, { Position.m_X, Position.m_Y, Position.m_Z }, ToB3(Rotation));
    }

    void backend::SetLinearVelocity(b3BodyId BodyId, const xmath::fvec3& Velocity) noexcept
    {
        b3Body_SetLinearVelocity(BodyId, { Velocity.m_X, Velocity.m_Y, Velocity.m_Z });
    }

    void backend::SetAngularVelocity(b3BodyId BodyId, const xmath::fvec3& Velocity) noexcept
    {
        b3Body_SetAngularVelocity(BodyId, { Velocity.m_X, Velocity.m_Y, Velocity.m_Z });
    }

    void backend::SetAwake(b3BodyId BodyId, bool Awake) noexcept
    {
        b3Body_SetAwake(BodyId, Awake);
    }

    void backend::SetBodyType(b3BodyId BodyId, b3BodyType Type) noexcept
    {
        b3Body_SetType(BodyId, Type);
    }

    void backend::SetMass(b3BodyId BodyId, float Mass) noexcept
    {
        if (Mass <= 0.0f) return;
        b3MassData Data = b3Body_GetMassData(BodyId);
        const float OldMass = Data.mass;
        if (OldMass > 1.0e-6f)
            Data.inertia = b3MulSM(Mass / OldMass, Data.inertia);
        Data.mass = Mass;
        b3Body_SetMassData(BodyId, Data);
    }

    void backend::ApplyForceToCenter(b3BodyId BodyId, const xmath::fvec3& Force) noexcept
    {
        b3Body_ApplyForceToCenter(BodyId, { Force.m_X, Force.m_Y, Force.m_Z }, true);
    }

    void backend::ApplyTorque(b3BodyId BodyId, const xmath::fvec3& Torque) noexcept
    {
        b3Body_ApplyTorque(BodyId, { Torque.m_X, Torque.m_Y, Torque.m_Z }, true);
    }

    void backend::MakeBodyUnqueryable(b3BodyId BodyId) noexcept
    {
        if (B3_IS_NULL(BodyId)) return;

        std::array<b3ShapeId, 8> Shapes;
        const int Count = b3Body_GetShapes(BodyId, Shapes.data(), static_cast<int>(Shapes.size()));
        const b3Filter Filter{ .categoryBits = 0, .maskBits = 0, .groupIndex = 0 };
        for (int i = 0; i < Count; ++i)
            b3Shape_SetFilter(Shapes[i], Filter, false);
    }

    void backend::DestroyBody(b3BodyId BodyId) noexcept
    {
        if (B3_IS_NON_NULL(BodyId))
            b3DestroyBody(BodyId);
    }
}
