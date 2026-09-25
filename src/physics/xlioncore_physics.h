#ifndef XLIONCORE_PHYSICS_H
#define XLIONCORE_PHYSICS_H
#pragma once

// The physics component: registered and consumed entirely inside LIONCore.dll (see
// xlioncore_plugin_entry.cpp) - xLION.exe never includes this header. Carries its own position/
// rotation rather than depending on a shared Transform component: xlevel::transform lives up in
// plugins/xlevel.plugin, a layer above xLIONCore, so depending on it here would invert the intended
// dependency direction. Reconciling with a shared Transform component (for xLIONRender, say) is a
// deliberate later step, not this one.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include <box3d/box3d.h>

namespace xlioncore::physics
{
    struct rigid_body
    {
        constexpr static auto typedef_v = xecs::component::type::data{ .m_pName = "RigidBody" };

        // Authored, saved with the entity
        xmath::fvec3 m_Position    = xmath::fvec3::fromZero();
        xmath::fvec3 m_Rotation    = xmath::fvec3::fromZero();   // radians, same convention as xlevel::transform
        xmath::fvec3 m_HalfExtents = xmath::fvec3::fromOne() * 0.5f;
        bool         m_bDynamic    = true;

        // Runtime only - not listed in XPROPERTY_DEF below, so it's neither shown in the inspector
        // nor round-tripped through scene save/load. b3_nullBodyId means "not created in the box3d
        // world yet"; the physics system creates it lazily the first time it sees this entity.
        b3BodyId     m_BodyId = b3_nullBodyId;

        XPROPERTY_DEF
        ( "RigidBody", rigid_body
        , obj_member<"Position",    &rigid_body::m_Position>
        , obj_member<"Rotation",    &rigid_body::m_Rotation>
        , obj_member<"HalfExtents", &rigid_body::m_HalfExtents>
        , obj_member<"Dynamic",     &rigid_body::m_bDynamic>
        )
    };
    XPROPERTY_REG(rigid_body)
}

#endif // XLIONCORE_PHYSICS_H
