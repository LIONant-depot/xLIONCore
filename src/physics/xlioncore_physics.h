#ifndef XLIONCORE_PHYSICS_H
#define XLIONCORE_PHYSICS_H
#pragma once

// Physics component: registered and consumed entirely inside LIONCore.dll (see
// xlioncore_plugin_entry.cpp) - xLION.exe never includes this header. Pose lives on
// xlioncore::transform (world-space, shared with render); this component only carries
// collider/body authoring + the runtime body id.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include <box3d/box3d.h>

namespace xlioncore::physics
{
    struct rigid_body
    {
        constexpr static auto typedef_v = xecs::component::type::data{ .m_pName = "RigidBody" };

        // Authored, saved with the entity
        xmath::fvec3 m_HalfExtents = xmath::fvec3::fromOne() * 0.5f;
        bool         m_bDynamic    = true;

        // Runtime only - not listed in XPROPERTY_DEF below, so it's neither shown in the inspector
        // nor round-tripped through scene save/load. b3_nullBodyId means "not created in the box3d
        // world yet"; the physics system creates it lazily the first time it sees this entity.
        // m_BodyHalfExtents is the last HalfExtents*Scale pushed into box3d (recreate when it drifts).
        b3BodyId     m_BodyId          = b3_nullBodyId;
        xmath::fvec3 m_BodyHalfExtents = {};

        XPROPERTY_DEF
        ( "RigidBody", rigid_body
        , obj_member<"HalfExtents", &rigid_body::m_HalfExtents>
        , obj_member<"Dynamic",     &rigid_body::m_bDynamic>
        )
    };
    XPROPERTY_REG(rigid_body)
}

#endif // XLIONCORE_PHYSICS_H
