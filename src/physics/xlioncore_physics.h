#ifndef XLIONCORE_PHYSICS_H
#define XLIONCORE_PHYSICS_H
#pragma once

// Physics component organization (Box3D):
//   transform (existing, DirtyToPhysics/CoolDown) + physics_body_properties (SHARE: damping, sleep,
//   collision filter) + one or more collider components (xlioncore_physics_collider.h: Box, Sphere,
//   Capsule, Cylinder - shape, size, PhysicsMaterial, sensor) + physics_body (DATA UNIQUE runtime handle)
//   + physics_dynamics (DATA UNIQUE; Dynamic bodies only - mass/force/torque/vel readback).
// Body typing: has physics_dynamics => Dynamic; else xlioncore::static_tag => Static; else Kinematic
// (the default - see xlioncore_tags.h and ResolveBodyType in xlioncore_physics_system.h). Not a
// bool flag here anymore: static is a cross-cutting concept (rendering/navmesh/occlusion culling
// care about it too, not just physics), so it lives as its own tag component, not a
// physics_body_properties field.
// Mass lives only on physics_dynamics (authoritative); friction, restitution and density come from the
// PhysicsMaterial each collider shape references.
//
// SHARE keys: zero-init explicit pads (DemoShare lesson) so default HashBytes(sizeof(T)) is stable.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "../xlioncore_small_vector_xproperty.h"
#include "xlioncore_physics_material.h"
#include "xlioncore_physics_collider.h"
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
        b3BodyType      m_CachedBodyType    = b3_dynamicBody;
        float           m_CachedMass        = 0.0f;

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
        , obj_member<"CachedMass",        &physics_body::m_CachedMass,        member_flags<flags::SHOW_READONLY>>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_body, "Physics", 40)
}

#endif // XLIONCORE_PHYSICS_H
