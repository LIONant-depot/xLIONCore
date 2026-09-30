#ifndef XLIONCORE_PHYSICS_COLLIDER_H
#define XLIONCORE_PHYSICS_COLLIDER_H
#pragma once

// Collider shape components - authored data only, no Box3D dependency, so editor-side code (the Level
// Editor's "Edit Collider" viewport tools) can include it without the physics backend. body_builder
// (xlioncore_physics_system.h) turns these into Box3D shapes when the entity is created.
//
//   PhysicsColliderBox       Boxes[]      Center, Orientation, Size
//   PhysicsColliderSphere    Spheres[]    Center, Radius
//   PhysicsColliderCapsule   Capsules[]   Center, Orientation, Radius, Height       (axis = the shape's local Y)
//   PhysicsColliderCylinder  Cylinders[]  Center, Orientation, Radius, Height       (axis = the shape's local Y)
//
// Every shape also has Material (a PhysicsMaterial asset, empty = friction 0.6 / restitution 0) and
// IsSensor. Several shapes in one array, or several collider components on one entity, make a compound
// body. All positions are in the body's frame (Transform Position/Rotation) and every size is multiplied
// by Transform.Scale, so a collider follows the entity when it is scaled:
//     Box       Size * Scale, per axis
//     Sphere    Radius * largest |Scale| axis
//     Capsule / Cylinder   Radius * larger of |Scale.x|, |Scale.z|;  Height * |Scale.y|
// Defaults match the Primitive of the same name at Scale 1, so adding a collider to a primitive fits it.
#include "dependencies/xECSV2/src/xecs.h"
#include <algorithm>
#include <cmath>
#include "dependencies/xmath/source/xmath.h"
#include "../xlioncore_small_vector_xproperty.h"
#include "xlioncore_physics_material.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

// Shared by every oriented shape: the orientation quaternion is the source of truth; the Inspector edits
// it as ZXY degrees through m_EditorRotation (same scheme as Transform). Macros because xproperty
// members need the concrete class in their pointer/lambda types.
#define XLION_COLLIDER_ORIENTATION_FIELDS                                                                       \
    xmath::fquat    m_Orientation     = xmath::fquat::fromIdentity();                                           \
    xmath::radian3  m_EditorRotation  = {};     /* Inspector-only Euler cache, not serialized */                \
    xmath::radian3 getEditorRotation(void) noexcept                                                             \
    {                                                                                                           \
        if (xmath::Abs(m_Orientation.Dot(xmath::fquat{ m_EditorRotation })) < 0.9999f) m_EditorRotation = m_Orientation.ToEuler(); \
        return m_EditorRotation;                                                                                \
    }                                                                                                           \
    void setEditorRotation(const xmath::radian3& R) noexcept { m_EditorRotation = R; m_Orientation = xmath::fquat{ R }; }

#define XLION_COLLIDER_ORIENTATION_MEMBERS(CLASS)                                                               \
    obj_member<"Orientation", &CLASS::m_Orientation, member_flags<flags::DONT_SHOW>>                            \
  , obj_scope<"Rotation", xproperty::settings::vector3_group                                                    \
        , obj_member<"X", +[](CLASS& O, bool bRead, float& V)                                                   \
            {                                                                                                   \
                auto R = O.getEditorRotation();                                                                 \
                if (bRead) V = xmath::RadToDeg(R.m_Pitch.m_Value);                                              \
                else     { R.m_Pitch = xmath::radian{ xmath::DegToRad(V) }; O.setEditorRotation(R); }           \
            }>                                                                                                  \
        , obj_member<"Y", +[](CLASS& O, bool bRead, float& V)                                                   \
            {                                                                                                   \
                auto R = O.getEditorRotation();                                                                 \
                if (bRead) V = xmath::RadToDeg(R.m_Yaw.m_Value);                                                \
                else     { R.m_Yaw = xmath::radian{ xmath::DegToRad(V) }; O.setEditorRotation(R); }             \
            }>                                                                                                  \
        , obj_member<"Z", +[](CLASS& O, bool bRead, float& V)                                                   \
            {                                                                                                   \
                auto R = O.getEditorRotation();                                                                 \
                if (bRead) V = xmath::RadToDeg(R.m_Roll.m_Value);                                               \
                else     { R.m_Roll = xmath::radian{ xmath::DegToRad(V) }; O.setEditorRotation(R); }            \
            }>                                                                                                  \
    >

namespace xlioncore::physics
{
    //----------------------------------------------------------------------------------------------
    // Box
    //----------------------------------------------------------------------------------------------
    struct collider_box_shape
    {
        xmath::fvec3    m_Center          = {};
        XLION_COLLIDER_ORIENTATION_FIELDS
        xmath::fvec3    m_Size            = xmath::fvec3::fromOne();
        material::ref   m_Material        = {};
        bool            m_IsSensor        = false;

        XPROPERTY_DEF
        ( "ColliderBox", collider_box_shape
        , obj_member<"Center", &collider_box_shape::m_Center>
        , XLION_COLLIDER_ORIENTATION_MEMBERS(collider_box_shape)
        , obj_member<"Size",     &collider_box_shape::m_Size>
        , obj_member<"Material", &collider_box_shape::m_Material>
        , obj_member<"IsSensor", &collider_box_shape::m_IsSensor>
        )
    };
    XPROPERTY_REG(collider_box_shape)

    // Builder component: box shapes handed to Box3D when the entity is created (see body_builder).
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

    //----------------------------------------------------------------------------------------------
    // Sphere - a sphere has no orientation, so there is none to edit
    //----------------------------------------------------------------------------------------------
    struct collider_sphere_shape
    {
        xmath::fvec3    m_Center          = {};
        float           m_Radius          = 0.5f;     // = the Sphere primitive at Scale 1
        material::ref   m_Material        = {};
        bool            m_IsSensor        = false;

        XPROPERTY_DEF
        ( "ColliderSphere", collider_sphere_shape
        , obj_member<"Center",   &collider_sphere_shape::m_Center>
        , obj_member<"Radius",   &collider_sphere_shape::m_Radius>
        , obj_member<"Material", &collider_sphere_shape::m_Material>
        , obj_member<"IsSensor", &collider_sphere_shape::m_IsSensor>
        )
    };
    XPROPERTY_REG(collider_sphere_shape)

    struct physics_collider_sphere
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid     = xecs::component::type::guid{ "xlioncore::physics::physics_collider_sphere" }
        , .m_pName    = "PhysicsColliderSphere"
        , .m_bBuilder = true
        };

        xcontainer::small_vector<collider_sphere_shape, 1> m_Spheres;

        physics_collider_sphere( void ) noexcept { m_Spheres.resize(1); }

        XPROPERTY_DEF
        ( "PhysicsColliderSphere", physics_collider_sphere
        , obj_member<"Spheres", &physics_collider_sphere::m_Spheres>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_collider_sphere, "Physics", 26)

    //----------------------------------------------------------------------------------------------
    // Capsule - two hemispheres joined by a cylinder, standing along the shape's local Y.
    // Height is tip to tip (caps included); it is never allowed below 2 * Radius.
    //----------------------------------------------------------------------------------------------
    struct collider_capsule_shape
    {
        xmath::fvec3    m_Center          = {};
        XLION_COLLIDER_ORIENTATION_FIELDS
        float           m_Radius          = 0.25f;    // = the Capsule primitive at Scale 1
        float           m_Height          = 1.0f;
        material::ref   m_Material        = {};
        bool            m_IsSensor        = false;

        XPROPERTY_DEF
        ( "ColliderCapsule", collider_capsule_shape
        , obj_member<"Center", &collider_capsule_shape::m_Center>
        , XLION_COLLIDER_ORIENTATION_MEMBERS(collider_capsule_shape)
        , obj_member<"Radius", &collider_capsule_shape::m_Radius>
        , obj_member<"Height", &collider_capsule_shape::m_Height>
        , obj_member<"Material", &collider_capsule_shape::m_Material>
        , obj_member<"IsSensor", &collider_capsule_shape::m_IsSensor>
        )
    };
    XPROPERTY_REG(collider_capsule_shape)

    struct physics_collider_capsule
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid     = xecs::component::type::guid{ "xlioncore::physics::physics_collider_capsule" }
        , .m_pName    = "PhysicsColliderCapsule"
        , .m_bBuilder = true
        };

        xcontainer::small_vector<collider_capsule_shape, 1> m_Capsules;

        physics_collider_capsule( void ) noexcept { m_Capsules.resize(1); }

        XPROPERTY_DEF
        ( "PhysicsColliderCapsule", physics_collider_capsule
        , obj_member<"Capsules", &physics_collider_capsule::m_Capsules>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_collider_capsule, "Physics", 27)

    //----------------------------------------------------------------------------------------------
    // Cylinder - a flat-ended cylinder standing along the shape's local Y (a convex hull of 24 sides).
    //----------------------------------------------------------------------------------------------
    struct collider_cylinder_shape
    {
        xmath::fvec3    m_Center          = {};
        XLION_COLLIDER_ORIENTATION_FIELDS
        float           m_Radius          = 0.5f;     // = the Cylinder primitive at Scale 1
        float           m_Height          = 1.0f;
        material::ref   m_Material        = {};
        bool            m_IsSensor        = false;

        XPROPERTY_DEF
        ( "ColliderCylinder", collider_cylinder_shape
        , obj_member<"Center", &collider_cylinder_shape::m_Center>
        , XLION_COLLIDER_ORIENTATION_MEMBERS(collider_cylinder_shape)
        , obj_member<"Radius", &collider_cylinder_shape::m_Radius>
        , obj_member<"Height", &collider_cylinder_shape::m_Height>
        , obj_member<"Material", &collider_cylinder_shape::m_Material>
        , obj_member<"IsSensor", &collider_cylinder_shape::m_IsSensor>
        )
    };
    XPROPERTY_REG(collider_cylinder_shape)

    struct physics_collider_cylinder
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid     = xecs::component::type::guid{ "xlioncore::physics::physics_collider_cylinder" }
        , .m_pName    = "PhysicsColliderCylinder"
        , .m_bBuilder = true
        };

        xcontainer::small_vector<collider_cylinder_shape, 1> m_Cylinders;

        physics_collider_cylinder( void ) noexcept { m_Cylinders.resize(1); }

        XPROPERTY_DEF
        ( "PhysicsColliderCylinder", physics_collider_cylinder
        , obj_member<"Cylinders", &physics_collider_cylinder::m_Cylinders>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(physics_collider_cylinder, "Physics", 28)

    //----------------------------------------------------------------------------------------------
    // How Transform.Scale sizes each shape - one definition shared by the physics builder and the
    // editor's collider tools, so what you see while editing is exactly what Box3D gets.
    //----------------------------------------------------------------------------------------------
    namespace collider_scale
    {
        inline float Largest(const xmath::fvec3& S) noexcept
        {
            return std::max({ std::fabs(S.m_X), std::fabs(S.m_Y), std::fabs(S.m_Z) });
        }
        // Capsule / Cylinder radius: the two axes perpendicular to the shape's local Y.
        inline float Radial (const xmath::fvec3& S) noexcept { return std::max(std::fabs(S.m_X), std::fabs(S.m_Z)); }
        inline float Axial  (const xmath::fvec3& S) noexcept { return std::fabs(S.m_Y); }

        struct capsule_size { float m_Radius; float m_HalfSpine; };     // segment half length between the two cap centers
        inline capsule_size Capsule(const collider_capsule_shape& C, const xmath::fvec3& S) noexcept
        {
            const float Radius = std::min(std::max(C.m_Radius, 0.001f), std::max(C.m_Height * 0.5f, 0.001f));
            return { Radius * Radial(S), std::max(C.m_Height * 0.5f - Radius, 0.0f) * Axial(S) };
        }
        struct cylinder_size { float m_Radius; float m_HalfHeight; };
        inline cylinder_size Cylinder(const collider_cylinder_shape& C, const xmath::fvec3& S) noexcept
        {
            return { std::max(C.m_Radius, 0.001f) * Radial(S), std::max(C.m_Height, 0.001f) * 0.5f * Axial(S) };
        }
        inline float Sphere(const collider_sphere_shape& C, const xmath::fvec3& S) noexcept
        {
            return std::max(C.m_Radius, 0.001f) * Largest(S);
        }
    }
}

#endif // XLIONCORE_PHYSICS_COLLIDER_H
