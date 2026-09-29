#ifndef XLIONCORE_PHYSICS_BACKEND_H
#define XLIONCORE_PHYSICS_BACKEND_H
#pragma once

// Persistent b3WorldId behind a plain class - no xecs types. Creates multi-shape-ready bodies
// (body first, then hull shape); mass from physics_dynamics (SetMassData) when m_Mass > 0, else shapes.
#include "dependencies/xmath/source/xmath.h"
#include <box3d/box3d.h>
#include <cstdint>

namespace xlioncore::physics
{
    struct body_create_params
    {
        b3BodyType      m_Type              = b3_dynamicBody;
        float           m_LinearDamping     = 0.0f;
        float           m_AngularDamping    = 0.0f;
        bool            m_EnableSleep       = true;
        bool            m_IsBullet          = false;
        xmath::fvec3    m_Position          = {};
        xmath::fquat    m_Rotation          = xmath::fquat::fromIdentity();
        xmath::fvec3    m_HalfExtents       = xmath::fvec3::fromOne() * 0.5f;
        float           m_Density           = 0.0f;   // unused for mass when m_Mass > 0
        float           m_Mass              = 0.0f;   // >0 => authoritative mass via SetMassData
        float           m_Friction          = 0.6f;
        float           m_Restitution       = 0.0f;
        std::uint64_t   m_CategoryBits      = B3_DEFAULT_CATEGORY_BITS;
        std::uint64_t   m_MaskBits          = B3_DEFAULT_MASK_BITS;
        std::int32_t    m_GroupIndex        = 0;
        bool            m_IsSensor          = false;
        xmath::fvec3    m_LocalPosition     = {};
        xmath::fquat    m_LocalRotation     = xmath::fquat::fromIdentity();
        std::uint64_t   m_UserData          = 0;      // The owning entity's handle - maps a Box3D body back to its entity
    };

    class backend
    {
    public:
        backend  (void) noexcept;
        ~backend (void) noexcept;

        b3BodyId     CreateBody         ( const body_create_params& Params ) noexcept;
        void         Step               (void) noexcept;
        xmath::fvec3 GetPosition        (b3BodyId BodyId) const noexcept;
        xmath::fquat GetRotation        (b3BodyId BodyId) const noexcept;
        xmath::fvec3 GetLinearVelocity  (b3BodyId BodyId) const noexcept;
        xmath::fvec3 GetAngularVelocity (b3BodyId BodyId) const noexcept;
        void         SetTransform       (b3BodyId BodyId, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept;
        void         SetLinearVelocity  (b3BodyId BodyId, const xmath::fvec3& Velocity) noexcept;
        void         SetAngularVelocity (b3BodyId BodyId, const xmath::fvec3& Velocity) noexcept;
        void         SetAwake           (b3BodyId BodyId, bool Awake) noexcept;
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

    private:
        b3WorldId m_World;
    };
}

#endif // XLIONCORE_PHYSICS_BACKEND_H
