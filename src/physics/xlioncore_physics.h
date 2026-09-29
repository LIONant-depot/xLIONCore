#ifndef XLIONCORE_PHYSICS_H
#define XLIONCORE_PHYSICS_H
#pragma once

// Physics component organization V1 follow-up (Box3D):
//   transform (existing, DirtyToPhysics/CoolDown) + physics_body_properties (SHARE)
//   + physics_shape_properties (SHARE) + physics_body (DATA UNIQUE runtime handle)
//   + physics_dynamics (DATA UNIQUE; Dynamic bodies only - mass/force/torque/vel readback).
// Body typing: has physics_dynamics => Dynamic; else xlioncore::static_tag => Static; else Kinematic
// (the default - see xlioncore_tags.h and ResolveBodyType in xlioncore_physics_system.h). Not a
// bool flag here anymore: static is a cross-cutting concept (rendering/navmesh/occlusion culling
// care about it too, not just physics), so it lives as its own tag component, not a
// physics_body_properties field.
// Mass lives only on physics_dynamics (authoritative); density is off-core (future material).
//
// SHARE keys: zero-init explicit pads (DemoShare lesson) so default HashBytes(sizeof(T)) is stable.
// Shape local pose stored as plain floats (not fvec3/fquat) to avoid alignment holes in the SHARE blob.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "../xlioncore_small_vector_xproperty.h"
#include "xlioncore_physics_material.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"
#include <box3d/box3d.h>
#include <cstdint>

namespace xlioncore::physics
{
    // Authored body flags + the collision filter every shape of the body uses. Explicit pads keep
    // the SHARE key (a hash of the raw bytes) stable.
    struct physics_body_properties
    {
        constexpr static auto typedef_v = xecs::component::type::share
        { .m_Guid     = xecs::component::type::guid{ "xlioncore::physics::physics_body_properties" }
        , .m_pName    = "PhysicsBodyProperties"
        , .m_bBuilder = true
        };

        bool            m_EnableSleep                = true;
        bool            m_EnableContinuousCollision  = false;
        std::uint8_t    m_Pad0[2]                    = {};
        float           m_LinearDamping              = 0.0f;
        float           m_AngularDamping             = 0.0f;
        std::int32_t    m_GroupIndex                 = 0;
        std::uint64_t   m_CategoryBits               = B3_DEFAULT_CATEGORY_BITS;
        std::uint64_t   m_MaskBits                   = B3_DEFAULT_MASK_BITS;

        XPROPERTY_DEF
        ( "PhysicsBodyProperties", physics_body_properties
        , obj_member<"LinearDamping",             &physics_body_properties::m_LinearDamping>
        , obj_member<"AngularDamping",            &physics_body_properties::m_AngularDamping>
        , obj_member<"EnableSleep",               &physics_body_properties::m_EnableSleep>
        , obj_member<"EnableContinuousCollision", &physics_body_properties::m_EnableContinuousCollision>
        , obj_member<"CategoryBits",              &physics_body_properties::m_CategoryBits>
        , obj_member<"MaskBits",                  &physics_body_properties::m_MaskBits>
        , obj_member<"GroupIndex",                &physics_body_properties::m_GroupIndex>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_body_properties, "Physics", 10)
    static_assert(sizeof(physics_body_properties) == 32);

    // One box of a PhysicsColliderBox. Offset and Size are in the entity's local space and get
    // multiplied by Transform.Scale (so Size 1 = the entity's own scaled unit box).
    struct collider_box_shape
    {
        xmath::fvec3    m_Offset          = {};
        xmath::fvec3    m_RotationDegrees = {};
        xmath::fvec3    m_Size            = xmath::fvec3::fromOne();
        material::ref   m_Material        = {};
        bool            m_IsSensor        = false;

        XPROPERTY_DEF
        ( "ColliderBox", collider_box_shape
        , obj_member<"Offset",   &collider_box_shape::m_Offset>
        , obj_member<"Rotation", &collider_box_shape::m_RotationDegrees>
        , obj_member<"Size",     &collider_box_shape::m_Size>
        , obj_member<"Material", &collider_box_shape::m_Material>
        , obj_member<"IsSensor", &collider_box_shape::m_IsSensor>
        )
    };
    XPROPERTY_REG(collider_box_shape)

    // Builder component: box shapes handed to Box3D when the entity is created (see body_builder).
    // Several collider components (and several boxes) on one entity make a compound body.
    struct physics_collider_box
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid     = xecs::component::type::guid{ "xlioncore::physics::physics_collider_box" }
        , .m_pName    = "PhysicsColliderBox"
        , .m_bBuilder = true
        };

        xcontainer::small_vector<collider_box_shape, 1> m_Boxes;

        physics_collider_box( void ) noexcept { m_Boxes.resize(1); }    // Added from the editor = one unit box

        XPROPERTY_DEF
        ( "PhysicsColliderBox", physics_collider_box
        , obj_member<"Boxes", &physics_collider_box::m_Boxes>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_collider_box, "Physics", 25)

    // V1 inlined shape. Density kept for future physics material but is NOT mass authority.
    // Plain floats for local pose avoid fvec3/fquat alignment padding.
    struct physics_shape_properties
    {
        constexpr static auto typedef_v = xecs::component::type::share
        { .m_Guid     = xecs::component::type::guid{ "xlioncore::physics::physics_shape_properties" }
        , .m_pName    = "PhysicsShapeProperties"
        , .m_bBuilder = true
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
    struct physics_dynamics
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::dynamics" }  // GUID string keeps the legacy name on purpose so saved scenes still load
        , .m_pName = "PhysicsDynamics"
        };

        xmath::fvec3    m_LinearVelocity  = {};
        xmath::fvec3    m_AngularVelocity = {};
        float           m_Mass            = 1.0f;
        xmath::fvec3    m_Force           = {};
        xmath::fvec3    m_Torque          = {};

        XPROPERTY_DEF
        ( "PhysicsDynamics", physics_dynamics
        , obj_member<"LinearVelocity",  &physics_dynamics::m_LinearVelocity>
        , obj_member<"AngularVelocity", &physics_dynamics::m_AngularVelocity>
        , obj_member<"Mass",            &physics_dynamics::m_Mass>
        , obj_member<"Force",           &physics_dynamics::m_Force>
        , obj_member<"Torque",          &physics_dynamics::m_Torque>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_dynamics, "Physics", 30)

    // UNIQUE runtime handle (DATA). Empty property table so archetype presence round-trips;
    // BodyId / caches are runtime-only (not listed as members).
    struct physics_body
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::physics::box3d_body" }  // GUID string keeps the legacy name on purpose so saved scenes still load
        , .m_pName = "Physics"
        };

        b3BodyId        m_BodyId            = b3_nullBodyId;
        xmath::fvec3    m_BodyHalfExtents   = {};
        b3BodyType      m_CachedBodyType    = b3_dynamicBody;
        float           m_CachedMass        = 0.0f;
        float           m_CachedFriction    = 0.0f;
        float           m_CachedRestitution = 0.0f;

        // Read-only, not hidden: not being editable doesn't mean not worth seeing - this component
        // exists partly for debugging, so the live handle/cache is exposed via SHOW_READONLY instead
        // of an empty property table (direct user note: "just because something is not meant to be
        // edited does not mean that we should hide it from the user"). m_BodyId's fields are surfaced
        // individually (BodyIndex/BodyGeneration) since b3BodyId itself isn't a registered xproperty
        // type; B3_IS_NULL/B3_IS_NON_NULL both key off index1 == 0, so that alone already tells you
        // whether a real Box3D body exists for this entity.
        XPROPERTY_DEF
        ( "Physics", physics_body
        , obj_member<"BodyIndex",         +[](physics_body& O, bool bRead, float& V) { if (bRead) V = static_cast<float>(O.m_BodyId.index1); },      member_flags<flags::SHOW_READONLY>>
        , obj_member<"BodyGeneration",    +[](physics_body& O, bool bRead, float& V) { if (bRead) V = static_cast<float>(O.m_BodyId.generation); },   member_flags<flags::SHOW_READONLY>>
        , obj_member<"CachedBodyType",    +[](physics_body& O, bool bRead, float& V) { if (bRead) V = static_cast<float>(O.m_CachedBodyType); }, member_flags<flags::SHOW_READONLY>> // 0=static 1=kinematic 2=dynamic (b3BodyType)
        , obj_member<"BodyHalfExtents",   &physics_body::m_BodyHalfExtents,   member_flags<flags::SHOW_READONLY>>
        , obj_member<"CachedMass",        &physics_body::m_CachedMass,        member_flags<flags::SHOW_READONLY>>
        , obj_member<"CachedFriction",    &physics_body::m_CachedFriction,    member_flags<flags::SHOW_READONLY>>
        , obj_member<"CachedRestitution", &physics_body::m_CachedRestitution, member_flags<flags::SHOW_READONLY>>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_body, "Physics", 40)
}

#endif // XLIONCORE_PHYSICS_H
