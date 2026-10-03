#include "xlioncore_physics_backend.h"
#include <cstring>
#include <array>
#include <algorithm>

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

        // The entity a shape belongs to: the handle its body carries in its user data (a shape that is gone has none).
        std::uint64_t EntityOfShape(b3ShapeId ShapeId) noexcept
        {
            constexpr std::uint64_t kNone = 0xffffffffffffffffull;
            if (!b3Shape_IsValid(ShapeId)) return kNone;
            const b3BodyId BodyId = b3Shape_GetBody(ShapeId);
            if (B3_IS_NULL(BodyId)) return kNone;
            return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(b3Body_GetUserData(BodyId)));
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
        BodyDef.motionLocks    = Params.m_Locks;
        BodyDef.userData       = reinterpret_cast<void*>(static_cast<std::uintptr_t>(Params.m_UserData));
        return b3CreateBody(m_World, &BodyDef);
    }

    void backend::AddShape(b3BodyId BodyId, const shape_params& Params) noexcept
    {
        b3ShapeDef ShapeDef = b3DefaultShapeDef();
        ShapeDef.density                    = Params.m_Density;
        ShapeDef.baseMaterial.friction      = Params.m_Friction;
        ShapeDef.baseMaterial.restitution   = Params.m_Restitution;
        ShapeDef.filter.categoryBits        = Params.m_CategoryBits;
        ShapeDef.filter.maskBits            = Params.m_MaskBits;
        ShapeDef.filter.groupIndex          = Params.m_GroupIndex;
        ShapeDef.isSensor                   = Params.m_IsSensor;
        ShapeDef.enableSensorEvents         = true;     // every shape may enter a sensor (the filter decides which do); a sensor reports only the shapes that have this
        ShapeDef.enableContactEvents        = Params.m_ContactEvents;
        ShapeDef.enableHitEvents            = Params.m_ContactEvents;
        ShapeDef.updateBodyMass             = true;

        const xmath::fvec3& P = Params.m_LocalPosition;
        b3Transform Local{};
        Local.p = { P.m_X, P.m_Y, P.m_Z };
        Local.q = ToB3(Params.m_LocalRotation);

        switch (Params.m_Kind)
        {
        case shape_params::kind::BOX:
        {
            const xmath::fvec3& H = Params.m_HalfExtents;
            if (IsIdentityLocal(Params.m_LocalPosition, Params.m_LocalRotation))
            {
                b3BoxHull Hull = b3MakeBoxHull(H.m_X, H.m_Y, H.m_Z);
                b3CreateHullShape(BodyId, &ShapeDef, &Hull.base);
            }
            else
            {
                b3BoxHull Hull = b3MakeTransformedBoxHull(H.m_X, H.m_Y, H.m_Z, Local);
                b3CreateHullShape(BodyId, &ShapeDef, &Hull.base);
            }
            break;
        }
        case shape_params::kind::SPHERE:
        {
            const b3Sphere Sphere{ Local.p, Params.m_Radius };
            b3CreateSphereShape(BodyId, &ShapeDef, &Sphere);
            break;
        }
        case shape_params::kind::CAPSULE:
        {
            // The segment runs along the shape's local Y; the shape rotation turns it into the body frame.
            const b3Vec3 Half = b3RotateVector(Local.q, { 0.0f, Params.m_HalfHeight, 0.0f });
            const b3Capsule Capsule{ { Local.p.x - Half.x, Local.p.y - Half.y, Local.p.z - Half.z }
                                   , { Local.p.x + Half.x, Local.p.y + Half.y, Local.p.z + Half.z }
                                   , Params.m_Radius };
            b3CreateCapsuleShape(BodyId, &ShapeDef, &Capsule);
            break;
        }
        case shape_params::kind::CYLINDER:
        {
            // b3CreateCylinder builds the hull from yOffset up to yOffset + height, so centre it on Y first.
            constexpr int Sides = 24;
            b3HullData* pBase = b3CreateCylinder(Params.m_HalfHeight * 2.0f, Params.m_Radius, -Params.m_HalfHeight, Sides);
            if (pBase == nullptr) break;
            b3HullData* pHull = IsIdentityLocal(Params.m_LocalPosition, Params.m_LocalRotation)
                              ? nullptr
                              : b3CloneAndTransformHull(pBase, Local, { 1.0f, 1.0f, 1.0f });
            b3CreateHullShape(BodyId, &ShapeDef, pHull ? pHull : pBase);      // the world clones the hull
            if (pHull) b3DestroyHull(pHull);
            b3DestroyHull(pBase);
            break;
        }
        }
    }

    void backend::Step(float FixedDeltaTime) noexcept
    {
        constexpr int SubStepCount = 4;
        b3World_Step(m_World, FixedDeltaTime, SubStepCount);
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

    void backend::SetMotionLocks(b3BodyId BodyId, std::uint8_t Bits) noexcept
    {
        const b3MotionLocks Locks{ (Bits & 1) != 0, (Bits & 2) != 0, (Bits & 4) != 0, (Bits & 8) != 0, (Bits & 16) != 0, (Bits & 32) != 0 };
        b3Body_SetMotionLocks(BodyId, Locks);
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

    void backend::DrainSensorEvents(std::vector<sensor_touch>& Begin, std::vector<sensor_touch>& End) const noexcept
    {
        Begin.clear();
        End.clear();
        constexpr std::uint64_t kNone = 0xffffffffffffffffull;
        const b3SensorEvents Events = b3World_GetSensorEvents(m_World);

        const auto Collect = [&](auto* pEvents, int Count, std::vector<sensor_touch>& Out) noexcept
        {
            for (int i = 0; i < Count; ++i)
            {
                const sensor_touch Touch{ xecs::component::entity{ EntityOfShape(pEvents[i].sensorShapeId) }, xecs::component::entity{ EntityOfShape(pEvents[i].visitorShapeId) }
                                        , pEvents[i].sensorShapeId, pEvents[i].visitorShapeId };
                if (Touch.m_Sensor.m_Value != kNone && Touch.m_Visitor.m_Value != kNone) Out.push_back(Touch);
            }
            std::sort(Out.begin(), Out.end(), [](const sensor_touch& A, const sensor_touch& B) noexcept
            {
                return A.m_Sensor.m_Value != B.m_Sensor.m_Value ? A.m_Sensor.m_Value < B.m_Sensor.m_Value : A.m_Visitor.m_Value < B.m_Visitor.m_Value;
            });
        };
        Collect(Events.beginEvents, Events.beginCount, Begin);
        Collect(Events.endEvents,   Events.endCount,   End);
    }

    void backend::DrainContactEvents(std::vector<contact_touch>& Begin, std::vector<contact_touch>& End, std::vector<contact_hit>& Hit) const noexcept
    {
        Begin.clear();
        End.clear();
        Hit.clear();
        constexpr std::uint64_t kNone = 0xffffffffffffffffull;
        const b3ContactEvents Events = b3World_GetContactEvents(m_World);

        // Box3D's order of the pair is kept (A, B); the events of one step are only sorted by entity so that two runs of the same game report the same order.
        const auto Collect = [&](auto* pEvents, int Count, auto&& Make, auto& Out) noexcept
        {
            for (int i = 0; i < Count; ++i)
            {
                // An end event may name a shape that was destroyed since: it has no entity to name any more.
                if (!b3Shape_IsValid(pEvents[i].shapeIdA) || !b3Shape_IsValid(pEvents[i].shapeIdB)) continue;
                const std::uint64_t A = EntityOfShape(pEvents[i].shapeIdA), B = EntityOfShape(pEvents[i].shapeIdB);
                if (A == kNone || B == kNone) continue;
                Out.push_back(Make(pEvents[i], A, B));
            }
            std::sort(Out.begin(), Out.end(), [](const auto& X, const auto& Y) noexcept
            {
                return X.m_A.m_Value != Y.m_A.m_Value ? X.m_A.m_Value < Y.m_A.m_Value : X.m_B.m_Value < Y.m_B.m_Value;
            });
        };

        const auto MakeTouch = [](const auto& E, std::uint64_t A, std::uint64_t B) noexcept
        {
            return contact_touch{ xecs::component::entity{ A }, xecs::component::entity{ B }, E.shapeIdA, E.shapeIdB, E.contactId };
        };
        Collect(Events.beginEvents, Events.beginCount, MakeTouch, Begin);
        Collect(Events.endEvents,   Events.endCount,   MakeTouch, End);
        Collect(Events.hitEvents,   Events.hitCount,   [](const b3ContactHitEvent& E, std::uint64_t A, std::uint64_t B) noexcept
        {
            return contact_hit{ xecs::component::entity{ A }, xecs::component::entity{ B }, E.shapeIdA, E.shapeIdB, E.contactId, E.point, E.normal, E.approachSpeed, E.userMaterialIdA, E.userMaterialIdB };
        }, Hit);
    }

    void backend::DestroyBody(b3BodyId BodyId) noexcept
    {
        if (B3_IS_NON_NULL(BodyId))
            b3DestroyBody(BodyId);
    }
}
