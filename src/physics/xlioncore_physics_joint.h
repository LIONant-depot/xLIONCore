#ifndef XLIONCORE_PHYSICS_JOINT_H
#define XLIONCORE_PHYSICS_JOINT_H
#pragma once

// A joint between this entity's body and another's (plan 13.1): Box3D's spherical, revolute, prismatic, weld and motor joints as one
// component, the way Unity's joints sit on a body and name the connected one. The entity must be a body (a root with a collider); the
// connected body is another root body, or none for a fixed point in the world.
// The joint frame (Anchor + Rotation) is given in this body's space at authoring time; the connected body's frame is taken from where the two
// bodies are when the joint is made, so what you see in the editor is the joint's rest. The frame's Z is the hinge axis (revolute), the slide
// axis (prismatic) and the cone's axis (spherical), as in Box3D.
// The joint is made by the Physics system before a step once both bodies exist, remade when an authored value changes (live tuning in Play),
// and goes with either body (Box3D destroys a body's joints with it).
#include "xlioncore_physics_collider.h"     // XLION_COLLIDER_ORIENTATION_*: the same Rotation editing as a collider
#include <box3d/box3d.h>

namespace xlioncore::physics
{
    struct physics_joint
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::physics_joint" }
        , .m_pName = "PhysicsJoint"
        };

        enum class kind : std::uint8_t { SPHERICAL, REVOLUTE, PRISMATIC, WELD, MOTOR };

        kind                    m_Kind              = kind::REVOLUTE;
        xecs::component::entity m_ConnectedBody     = {};               // none: the world
        xmath::fvec3            m_Anchor            = {};               // in this body's space (scaled with it, as a collider's Center)
        XLION_COLLIDER_ORIENTATION_FIELDS                               // m_Orientation: the joint frame in this body's space
        bool                    m_CollideConnected  = false;
        bool                    m_EnableLimit       = false;
        float                   m_LowerLimit        = -45.0f;           // deg (revolute, spherical twist) or m (prismatic)
        float                   m_UpperLimit        =  45.0f;
        float                   m_ConeAngle         =  45.0f;           // deg, spherical (when EnableLimit)
        bool                    m_EnableSpring      = false;            // revolute, prismatic, spherical: pull toward Target; motor and weld: always a spring
        float                   m_SpringHertz       = 2.0f;
        float                   m_SpringDamping     = 0.7f;             // damping ratio (1: critical)
        float                   m_Target            = 0.0f;             // deg or m (revolute, prismatic)
        bool                    m_EnableMotor       = false;
        float                   m_MotorSpeed        = 0.0f;             // deg/s or m/s
        float                   m_MaxMotorTorque    = 100.0f;           // N m (N for prismatic; motor joint: also its max spring force/torque)

        b3JointId               m_JointId           = b3_nullJointId;  // runtime
        std::uint64_t           m_BuiltHash         = 0;                // the authored values the joint was made with

        // What the authored values are (a change remakes the joint).
        std::uint64_t Hash(void) const noexcept
        {
            std::uint64_t H = 1469598103934665603ull;
            const auto Mix = [&](const void* p, std::size_t n) noexcept { for (std::size_t i = 0; i < n; ++i) H = (H ^ static_cast<const std::uint8_t*>(p)[i]) * 1099511628211ull; };
            const auto F = [&](float V) noexcept { Mix(&V, sizeof V); };
            Mix(&m_Kind, 1); Mix(&m_ConnectedBody.m_Value, 8);
            F(m_Anchor.m_X); F(m_Anchor.m_Y); F(m_Anchor.m_Z);
            F(m_Orientation.m_X); F(m_Orientation.m_Y); F(m_Orientation.m_Z); F(m_Orientation.m_W);
            Mix(&m_CollideConnected, 1); Mix(&m_EnableLimit, 1); Mix(&m_EnableSpring, 1); Mix(&m_EnableMotor, 1);
            F(m_LowerLimit); F(m_UpperLimit); F(m_ConeAngle); F(m_SpringHertz); F(m_SpringDamping); F(m_Target); F(m_MotorSpeed); F(m_MaxMotorTorque);
            return H;
        }

        XPROPERTY_DEF
        ( "PhysicsJoint", physics_joint
        , obj_member<"Kind", &physics_joint::m_Kind
            , member_enum_value<"Spherical", kind::SPHERICAL>
            , member_enum_value<"Revolute",  kind::REVOLUTE>
            , member_enum_value<"Prismatic", kind::PRISMATIC>
            , member_enum_value<"Weld",      kind::WELD>
            , member_enum_value<"Motor",     kind::MOTOR>
            , member_help<"Spherical: a ball joint (cone and twist limits). Revolute: a hinge around the frame's Z (a door). Prismatic: a slide along Z. Weld: glued (with a spring when SpringHertz > 0). Motor: drives the connected body to the pose it had when the joint was made (a spring of SpringHertz, capped by MaxMotorTorque)">>
        , obj_member<"ConnectedBody", &physics_joint::m_ConnectedBody, member_help<"The other body (a root entity with a collider). None: a fixed point in the world">>
        , obj_member<"Anchor", &physics_joint::m_Anchor, member_help<"Where the joint is, in this body's space">>
        , XLION_COLLIDER_ORIENTATION_MEMBERS(physics_joint)
        , obj_member<"CollideConnected", &physics_joint::m_CollideConnected>
        , obj_member<"EnableLimit",      &physics_joint::m_EnableLimit>
        , obj_member<"LowerLimit",       &physics_joint::m_LowerLimit, member_help<"Degrees (revolute, spherical twist) or meters (prismatic)">>
        , obj_member<"UpperLimit",       &physics_joint::m_UpperLimit, member_help<"Degrees (revolute, spherical twist) or meters (prismatic)">>
        , obj_member<"ConeAngle",        &physics_joint::m_ConeAngle,  member_help<"Spherical: how far (degrees) the connected body may swing from the frame's Z">>
        , obj_member<"EnableSpring",     &physics_joint::m_EnableSpring>
        , obj_member<"SpringHertz",      &physics_joint::m_SpringHertz>
        , obj_member<"SpringDamping",    &physics_joint::m_SpringDamping, member_help<"Damping ratio: 1 is critical">>
        , obj_member<"Target",           &physics_joint::m_Target, member_help<"The spring's rest: degrees (revolute) or meters (prismatic)">>
        , obj_member<"EnableMotor",      &physics_joint::m_EnableMotor>
        , obj_member<"MotorSpeed",       &physics_joint::m_MotorSpeed, member_help<"Degrees or meters per second">>
        , obj_member<"MaxMotorTorque",   &physics_joint::m_MaxMotorTorque, member_help<"N m (N for prismatic). Motor joint: its spring's maximum force and torque">>
        , obj_member<"JointIndex", +[](physics_joint& O, bool bRead, float& V) { if (bRead) V = static_cast<float>(O.m_JointId.index1); }, member_flags<flags::SHOW_READONLY>>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_joint, "Physics", 50)

    // Makes the Box3D joint for J between body A (this entity's) and B (the connected one, or Ground for the world). Both bodies as they are now.
    inline b3JointId CreateJoint( b3WorldId World, const physics_joint& J, b3BodyId A, b3BodyId B, const xmath::fvec3& Scale ) noexcept
    {
        constexpr float kDeg = 3.14159265358979f / 180.0f;
        const b3Vec3 LocalP{ J.m_Anchor.m_X * Scale.m_X, J.m_Anchor.m_Y * Scale.m_Y, J.m_Anchor.m_Z * Scale.m_Z };
        const b3Quat LocalQ{ { J.m_Orientation.m_X, J.m_Orientation.m_Y, J.m_Orientation.m_Z }, J.m_Orientation.m_W };

        // The frame in the world now, then in B's space: B's frame is where the joint is at rest.
        const b3Pos  PA = b3Body_GetPosition(A), PB = b3Body_GetPosition(B);
        const b3Quat QA = b3Body_GetRotation(A), QB = b3Body_GetRotation(B);
        const b3Vec3 W  = b3RotateVector(QA, LocalP);
        const b3Vec3 D{ float(PA.x - PB.x) + W.x, float(PA.y - PB.y) + W.y, float(PA.z - PB.z) + W.z };
        const b3Quat QW = b3MulQuat(QA, LocalQ);

        const auto Fill = [&](b3JointDef& Base) noexcept
        {
            Base.bodyIdA          = A;
            Base.bodyIdB          = B;
            Base.localFrameA      = { LocalP, LocalQ };
            Base.localFrameB      = { b3InvRotateVector(QB, D), b3InvMulQuat(QB, QW) };
            Base.collideConnected = J.m_CollideConnected;
        };

        switch (J.m_Kind)
        {
        case physics_joint::kind::SPHERICAL:
        {
            b3SphericalJointDef Def = b3DefaultSphericalJointDef(); Fill(Def.base);
            Def.enableSpring     = J.m_EnableSpring;    Def.hertz = J.m_SpringHertz; Def.dampingRatio = J.m_SpringDamping;
            Def.enableConeLimit  = J.m_EnableLimit;     Def.coneAngle = J.m_ConeAngle * kDeg;
            Def.enableTwistLimit = J.m_EnableLimit;     Def.lowerTwistAngle = J.m_LowerLimit * kDeg; Def.upperTwistAngle = J.m_UpperLimit * kDeg;
            Def.enableMotor      = J.m_EnableMotor;     Def.maxMotorTorque = J.m_MaxMotorTorque;     // (motorVelocity stays 0: a joint friction)
            return b3CreateSphericalJoint(World, &Def);
        }
        case physics_joint::kind::REVOLUTE:
        {
            b3RevoluteJointDef Def = b3DefaultRevoluteJointDef(); Fill(Def.base);
            Def.enableSpring = J.m_EnableSpring; Def.hertz = J.m_SpringHertz; Def.dampingRatio = J.m_SpringDamping; Def.targetAngle = J.m_Target * kDeg;
            Def.enableLimit  = J.m_EnableLimit;  Def.lowerAngle = J.m_LowerLimit * kDeg; Def.upperAngle = J.m_UpperLimit * kDeg;
            Def.enableMotor  = J.m_EnableMotor;  Def.maxMotorTorque = J.m_MaxMotorTorque; Def.motorSpeed = J.m_MotorSpeed * kDeg;
            return b3CreateRevoluteJoint(World, &Def);
        }
        case physics_joint::kind::PRISMATIC:
        {
            b3PrismaticJointDef Def = b3DefaultPrismaticJointDef(); Fill(Def.base);
            Def.enableSpring = J.m_EnableSpring; Def.hertz = J.m_SpringHertz; Def.dampingRatio = J.m_SpringDamping; Def.targetTranslation = J.m_Target;
            Def.enableLimit  = J.m_EnableLimit;  Def.lowerTranslation = J.m_LowerLimit; Def.upperTranslation = J.m_UpperLimit;
            Def.enableMotor  = J.m_EnableMotor;  Def.maxMotorForce = J.m_MaxMotorTorque; Def.motorSpeed = J.m_MotorSpeed;
            return b3CreatePrismaticJoint(World, &Def);
        }
        case physics_joint::kind::WELD:
        {
            b3WeldJointDef Def = b3DefaultWeldJointDef(); Fill(Def.base);
            if (J.m_EnableSpring) { Def.linearHertz = Def.angularHertz = J.m_SpringHertz; Def.linearDampingRatio = Def.angularDampingRatio = J.m_SpringDamping; }
            return b3CreateWeldJoint(World, &Def);
        }
        case physics_joint::kind::MOTOR:
        {
            b3MotorJointDef Def = b3DefaultMotorJointDef(); Fill(Def.base);
            Def.linearHertz    = Def.angularHertz        = J.m_SpringHertz;
            Def.linearDampingRatio = Def.angularDampingRatio = J.m_SpringDamping;
            Def.maxSpringForce = Def.maxSpringTorque     = J.m_MaxMotorTorque;
            return b3CreateMotorJoint(World, &Def);
        }
        }
        return b3_nullJointId;
    }
}

#endif // XLIONCORE_PHYSICS_JOINT_H
