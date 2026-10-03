#ifndef XLIONCORE_PHYSICS_EVENTS_H
#define XLIONCORE_PHYSICS_EVENTS_H
#pragma once

// What Box3D tells about a step, for the events of the physics (xlioncore_physics.h: sensor_begin_event, contact_hit_event, ...). Each struct is the Box3D event of the same name with the bodies
// named by their entities (the user data of a Box3D body is its entity handle): everything else is Box3D's own, in Box3D's own types, so a handler can hand the ids and the vectors back to
// Box3D (b3Shape_GetBody( Touch.m_ShapeA ), b3Contact_IsValid( ... ), ...).
#include "dependencies/xECSV2/src/xecs.h"
#include <box3d/box3d.h>
#include <cstdint>

namespace xlioncore::physics
{
    // b3SensorBeginTouchEvent / b3SensorEndTouchEvent: a shape (the visitor) started or stopped overlapping a sensor shape. The ids of an end event may name shapes that were destroyed since
    // (b3Shape_IsValid).
    struct sensor_touch
    {
        xecs::component::entity m_Sensor        = {};
        xecs::component::entity m_Visitor       = {};
        b3ShapeId               m_SensorShape   = {};
        b3ShapeId               m_VisitorShape  = {};
    };

    // b3ContactBeginTouchEvent / b3ContactEndTouchEvent: two solid shapes started or stopped touching. The pair is in Box3D's order. The contact
    // is Box3D's transient contact id: check b3Contact_IsValid before using it (it is gone when the world is modified or simulated, and after an end event).
    struct contact_touch
    {
        xecs::component::entity m_A             = {};
        xecs::component::entity m_B             = {};
        b3ShapeId               m_ShapeA        = {};
        b3ShapeId               m_ShapeB        = {};
        b3ContactId             m_Contact       = {};
    };

    // b3ContactHitEvent: two shapes hit each other faster than the world's hit threshold. Same pair as contact_touch; the normal is a unit vector from A to B. The point is in the
    // world, halfway between the two surfaces (it may be a speculative one, where they were not touching yet at the start of the step). The approach speed is always positive, in meters per second.
    struct contact_hit
    {
        xecs::component::entity m_A             = {};
        xecs::component::entity m_B             = {};
        b3ShapeId               m_ShapeA        = {};
        b3ShapeId               m_ShapeB        = {};
        b3ContactId             m_Contact       = {};
        b3Pos                   m_Point         = {};
        b3Vec3                  m_Normal        = {};
        float                   m_ApproachSpeed = 0.0f;
        std::uint64_t           m_UserMaterialA = 0;
        std::uint64_t           m_UserMaterialB = 0;
    };
}

#endif // XLIONCORE_PHYSICS_EVENTS_H
