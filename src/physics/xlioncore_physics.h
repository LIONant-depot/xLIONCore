#ifndef XLIONCORE_PHYSICS_H
#define XLIONCORE_PHYSICS_H
#pragma once

// Physics component organization V1 follow-up (Box3D):
//   transform (existing, DirtyToPhysics/CoolDown) + physics_body_properties (SHARE, IsKinematic)
//   + physics_shape_properties (SHARE) + box3d_body (DATA UNIQUE runtime handle)
//   + dynamics (DATA UNIQUE; Dynamic bodies only - mass/force/torque/vel readback).
// Body typing: has dynamics => Dynamic; else IsKinematic => Kinematic; else Static.
// Mass lives only on dynamics (authoritative); density is off-core (future material).
//
// SHARE keys: zero-init explicit pads (DemoShare lesson) so default HashBytes(sizeof(T)) is stable.
// Shape local pose stored as plain floats (not fvec3/fquat) to avoid alignment holes in the SHARE blob.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"
#include <box3d/box3d.h>
#include <cstdint>

namespace xlioncore::physics
{
    // Authored body flags. IsKinematic only applies when the entity has NO dynamics
    // (has dynamics => always Dynamic in Box3D). Layout packs to 16 bytes with pads.
    struct physics_body_properties
    {
        constexpr static auto typedef_v = xecs::component::type::share
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::physics_body_properties" }
        , .m_pName = "PhysicsBodyProperties"
        };

        bool            m_IsKinematic                = false;
        bool            m_EnableSleep                = true;
        bool            m_EnableContinuousCollision  = false;
        std::uint8_t    m_Pad0[5]                    = {};
        float           m_LinearDamping              = 0.0f;
        float           m_AngularDamping             = 0.0f;

        XPROPERTY_DEF
        ( "PhysicsBodyProperties", physics_body_properties
        , obj_member<"IsKinematic",               &physics_body_properties::m_IsKinematic>
        , obj_member<"LinearDamping",             &physics_body_properties::m_LinearDamping>
        , obj_member<"AngularDamping",            &physics_body_properties::m_AngularDamping>
        , obj_member<"EnableSleep",               &physics_body_properties::m_EnableSleep>
        , obj_member<"EnableContinuousCollision", &physics_body_properties::m_EnableContinuousCollision>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_body_properties, "Physics", 10)
    static_assert(sizeof(physics_body_properties) == 16);

    // V1 inlined shape. Density kept for future physics material but is NOT mass authority.
    // Plain floats for local pose avoid fvec3/fquat alignment padding.
    struct physics_shape_properties
    {
        constexpr static auto typedef_v = xecs::component::type::share
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::physics_shape_properties" }
        , .m_pName = "PhysicsShapeProperties"
        };

        float           m_Density        = 1000.0f;
        float           m_Friction       = 0.6f;
        float           m_Restitution    = 0.0f;
        std::uint8_t    m_Pad0[4]        = {};
        std::uint64_t   m_CategoryBits  = B3_DEFAULT_CATEGORY_BITS;
        std::uint64_t   m_MaskBits      = B3_DEFAULT_MASK_BITS;
        std::int32_t    m_GroupIndex    = 0;
        bool            m_IsSensor      = false;
        std::uint8_t    m_Pad1[3]        = {};
        float           m_LocalPosX      = 0.0f;
        float           m_LocalPosY      = 0.0f;
        float           m_LocalPosZ      = 0.0f;
        float           m_LocalRotX      = 0.0f;
        float           m_LocalRotY      = 0.0f;
        float           m_LocalRotZ      = 0.0f;
        float           m_LocalRotW      = 1.0f;
        std::uint8_t    m_Pad2[4]        = {}; // trailing align-to-8 for HashBytes

        xmath::fvec3 LocalPosition(void) const noexcept { return { m_LocalPosX, m_LocalPosY, m_LocalPosZ }; }
        xmath::fquat LocalRotation(void) const noexcept { return { m_LocalRotX, m_LocalRotY, m_LocalRotZ, m_LocalRotW }; }

        XPROPERTY_DEF
        ( "PhysicsShapeProperties", physics_shape_properties
        , obj_member<"Density",       &physics_shape_properties::m_Density, member_flags<flags::DONT_SHOW>>
        , obj_member<"Friction",      &physics_shape_properties::m_Friction>
        , obj_member<"Restitution",   &physics_shape_properties::m_Restitution>
        , obj_member<"CategoryBits",  &physics_shape_properties::m_CategoryBits>
        , obj_member<"MaskBits",      &physics_shape_properties::m_MaskBits>
        , obj_member<"GroupIndex",    &physics_shape_properties::m_GroupIndex>
        , obj_member<"IsSensor",      &physics_shape_properties::m_IsSensor>
        , obj_scope<"LocalPosition", xproperty::settings::vector3_group
            , obj_member<"X", &physics_shape_properties::m_LocalPosX>
            , obj_member<"Y", &physics_shape_properties::m_LocalPosY>
            , obj_member<"Z", &physics_shape_properties::m_LocalPosZ>
            >
        , obj_member<"LocalRotX", &physics_shape_properties::m_LocalRotX, member_flags<flags::DONT_SHOW>>
        , obj_member<"LocalRotY", &physics_shape_properties::m_LocalRotY, member_flags<flags::DONT_SHOW>>
        , obj_member<"LocalRotZ", &physics_shape_properties::m_LocalRotZ, member_flags<flags::DONT_SHOW>>
        , obj_member<"LocalRotW", &physics_shape_properties::m_LocalRotW, member_flags<flags::DONT_SHOW>>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_shape_properties, "Physics", 20)
    static_assert(sizeof(physics_shape_properties) == 72);

    // DATA UNIQUE - Dynamic bodies only. Box3D->ECS velocity readback; Force/Torque consumed each step;
    // Mass is the single authoritative mass (ECS->Box3D on create/recreate / when changed).
    struct dynamics
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::dynamics" }
        , .m_pName = "dynamics"
        };

        xmath::fvec3    m_LinearVelocity  = {};
        xmath::fvec3    m_AngularVelocity = {};
        float           m_Mass            = 1.0f;
        xmath::fvec3    m_Force           = {};
        xmath::fvec3    m_Torque          = {};

        XPROPERTY_DEF
        ( "dynamics", dynamics
        , obj_member<"LinearVelocity",  &dynamics::m_LinearVelocity>
        , obj_member<"AngularVelocity", &dynamics::m_AngularVelocity>
        , obj_member<"Mass",            &dynamics::m_Mass>
        , obj_member<"Force",           &dynamics::m_Force>
        , obj_member<"Torque",          &dynamics::m_Torque>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(dynamics, "Physics", 30)

    // UNIQUE runtime handle (DATA). Empty property table so archetype presence round-trips;
    // BodyId / caches are runtime-only (not listed as members).
    struct box3d_body
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::box3d_body" }
        , .m_pName = "Box3dBody"
        };

        b3BodyId        m_BodyId            = b3_nullBodyId;
        xmath::fvec3    m_BodyHalfExtents   = {};
        b3BodyType      m_CachedBodyType    = b3_dynamicBody;
        float           m_CachedMass        = 0.0f;
        float           m_CachedFriction    = 0.0f;
        float           m_CachedRestitution = 0.0f;

        XPROPERTY_DEF
        ( "Box3dBody", box3d_body
        )
    };
    XSCRIPT_REGISTER_COMPONENT(box3d_body, "Physics", 40)
}

#endif // XLIONCORE_PHYSICS_H
