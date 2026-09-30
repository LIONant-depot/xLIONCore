#ifndef XLIONCORE_PHYSICS_COLLIDER_H
#define XLIONCORE_PHYSICS_COLLIDER_H
#pragma once

// Collider shape components - authored data only, no Box3D dependency, so editor-side code (the Level
// Editor's "Edit Collider" viewport tool) can include it without the physics backend. body_builder
// (xlioncore_physics_system.h) turns these into Box3D shapes when the entity is created.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "../xlioncore_small_vector_xproperty.h"
#include "xlioncore_physics_material.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

namespace xlioncore::physics
{
    // One box of a PhysicsColliderBox, in the body's local frame (Transform Position/Rotation). Center
    // and Size get multiplied by Transform.Scale (Size 1 = the entity's own scaled unit box); the scale
    // applies along the box's own axes, so a rotated box never shears. m_Orientation is the source of
    // truth; the Inspector edits it as ZXY degrees through m_EditorRotation (same scheme as Transform).
    struct collider_box_shape
    {
        xmath::fvec3    m_Center          = {};
        xmath::fquat    m_Orientation     = xmath::fquat::fromIdentity();
        xmath::fvec3    m_Size            = xmath::fvec3::fromOne();
        material::ref   m_Material        = {};
        bool            m_IsSensor        = false;
        xmath::radian3  m_EditorRotation  = {};     // Inspector-only Euler cache, not serialized

        xmath::radian3 getEditorRotation(void) noexcept
        {
            if (xmath::Abs(m_Orientation.Dot(xmath::fquat{ m_EditorRotation })) < 0.9999f) m_EditorRotation = m_Orientation.ToEuler();
            return m_EditorRotation;
        }
        void setEditorRotation(const xmath::radian3& R) noexcept { m_EditorRotation = R; m_Orientation = xmath::fquat{ R }; }

        XPROPERTY_DEF
        ( "ColliderBox", collider_box_shape
        , obj_member<"Center",      &collider_box_shape::m_Center>
        , obj_member<"Orientation", &collider_box_shape::m_Orientation, member_flags<flags::DONT_SHOW>>
        , obj_scope<"Rotation", xproperty::settings::vector3_group
            , obj_member<"X", +[](collider_box_shape& O, bool bRead, float& V)
                {
                    auto R = O.getEditorRotation();
                    if (bRead) V = xmath::RadToDeg(R.m_Pitch.m_Value);
                    else     { R.m_Pitch = xmath::radian{ xmath::DegToRad(V) }; O.setEditorRotation(R); }
                }>
            , obj_member<"Y", +[](collider_box_shape& O, bool bRead, float& V)
                {
                    auto R = O.getEditorRotation();
                    if (bRead) V = xmath::RadToDeg(R.m_Yaw.m_Value);
                    else     { R.m_Yaw = xmath::radian{ xmath::DegToRad(V) }; O.setEditorRotation(R); }
                }>
            , obj_member<"Z", +[](collider_box_shape& O, bool bRead, float& V)
                {
                    auto R = O.getEditorRotation();
                    if (bRead) V = xmath::RadToDeg(R.m_Roll.m_Value);
                    else     { R.m_Roll = xmath::radian{ xmath::DegToRad(V) }; O.setEditorRotation(R); }
                }>
            >
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
}

#endif // XLIONCORE_PHYSICS_COLLIDER_H
