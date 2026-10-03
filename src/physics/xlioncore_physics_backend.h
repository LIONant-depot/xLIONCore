#ifndef XLIONCORE_PHYSICS_BACKEND_H
#define XLIONCORE_PHYSICS_BACKEND_H
#pragma once

// Persistent b3WorldId behind a plain class. Creates multi-shape-ready bodies
// (body first, then hull shape); mass from physics_dynamics (SetMassData) when m_Mass > 0, else shapes.
#include "dependencies/xmath/source/xmath.h"
#include <box3d/box3d.h>
#include "xlioncore_physics_events.h"
#include <cstdint>
#include <vector>

namespace xlioncore::physics
{
    struct body_create_params
    {
        b3BodyType      m_Type              = b3_dynamicBody;
        float           m_LinearDamping     = 0.0f;
        float           m_AngularDamping    = 0.0f;
        bool            m_EnableSleep       = true;
        bool            m_IsBullet          = false;
        b3MotionLocks   m_Locks             = {};     // axes the body may not move along / turn around
        xmath::fvec3    m_Position          = {};
        xmath::fquat    m_Rotation          = xmath::fquat::fromIdentity();
        std::uint64_t   m_UserData          = 0;      // The owning entity's handle - maps a Box3D body back to its entity
    };

    // One collider shape, already resolved to world scale (the builder applies Transform.Scale). Box uses
    // m_HalfExtents; Sphere m_Radius; Capsule m_Radius + m_HalfHeight (half the distance between the two
    // cap centers); Cylinder m_Radius + m_HalfHeight (half the flat-to-flat height). Capsule and Cylinder
    // stand along the shape's local Y, which m_LocalRotation then orients.
    struct shape_params
    {
        enum class kind : std::uint8_t { BOX, SPHERE, CAPSULE, CYLINDER };

        kind            m_Kind              = kind::BOX;
        xmath::fvec3    m_HalfExtents       = xmath::fvec3::fromOne() * 0.5f;
        float           m_Radius            = 0.5f;
        float           m_HalfHeight        = 0.5f;
        xmath::fvec3    m_LocalPosition     = {};
        xmath::fquat    m_LocalRotation     = xmath::fquat::fromIdentity();
        float           m_Density           = 0.0f;   // 0 for static/kinematic bodies
        float           m_Friction          = 0.6f;
        float           m_Restitution       = 0.0f;
        std::uint64_t   m_CategoryBits      = B3_DEFAULT_CATEGORY_BITS;
        std::uint64_t   m_MaskBits          = B3_DEFAULT_MASK_BITS;
        std::int32_t    m_GroupIndex        = 0;
        bool            m_IsSensor          = false;
        bool            m_ContactEvents     = false;    // report the touches and the hits of this shape (a touch is reported when either of the two shapes asks)
    };

    class backend
    {
    public:
        backend  (void) noexcept;
        ~backend (void) noexcept;

        b3BodyId     CreateBody         ( const body_create_params& Params ) noexcept;     // No shapes yet - add them, then SetMass
        void         AddShape           ( b3BodyId BodyId, const shape_params& Params ) noexcept;
        void         Step               (float FixedDeltaTime) noexcept;
        xmath::fvec3 GetPosition        (b3BodyId BodyId) const noexcept;
        xmath::fquat GetRotation        (b3BodyId BodyId) const noexcept;
        xmath::fvec3 GetLinearVelocity  (b3BodyId BodyId) const noexcept;
        xmath::fvec3 GetAngularVelocity (b3BodyId BodyId) const noexcept;
        void         SetTransform       (b3BodyId BodyId, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept;
        void         SetLinearVelocity  (b3BodyId BodyId, const xmath::fvec3& Velocity) noexcept;
        void         SetAngularVelocity (b3BodyId BodyId, const xmath::fvec3& Velocity) noexcept;
        void         SetAwake           (b3BodyId BodyId, bool Awake) noexcept;
        void         SetMotionLocks     (b3BodyId BodyId, std::uint8_t ConstraintBits) noexcept;   // bits as physics_dynamics::constraintBits()
        void         SetMass            (b3BodyId BodyId, float Mass) noexcept;
        void         SetBodyType        (b3BodyId BodyId, b3BodyType Type) noexcept;
        void         ApplyForceToCenter (b3BodyId BodyId, const xmath::fvec3& Force) noexcept;
        void         ApplyTorque        (b3BodyId BodyId, const xmath::fvec3& Torque) noexcept;
        // Zeros every shape's collision filter on this body (category+mask), so a raycast/overlap
        // query issued in the window between a kill and its deferred DestroyBody won't match it -
        // best-effort (same locked-world no-op as everything else here), not a substitute for a
        // query result checking Entity.isZombie() once queries actually exist in this engine.
        void         MakeBodyUnqueryable(b3BodyId BodyId) noexcept;
        void         DestroyBody        (b3BodyId BodyId) noexcept;
        // What the last Step changed between sensors and the shapes in them (Box3D's sensor events with the entity of each body, its user data), sorted so that two runs of the same game report the
        // same order. An end event whose shape (or body) was destroyed since is dropped: it has no entity to name any more.
        void         DrainSensorEvents  ( std::vector<sensor_touch>& Begin, std::vector<sensor_touch>& End ) const noexcept;
        // The same for the shapes that asked for contact events: who started touching, who stopped, who hit (faster than the world's hit threshold), sorted by (A, B). Pairs with a body without an entity
        // are dropped.
        void         DrainContactEvents ( std::vector<contact_touch>& Begin, std::vector<contact_touch>& End, std::vector<contact_hit>& Hit ) const noexcept;

    private:
        b3WorldId m_World;
    };
}

#endif // XLIONCORE_PHYSICS_BACKEND_H
