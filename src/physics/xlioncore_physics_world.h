#ifndef XLIONCORE_PHYSICS_WORLD_H
#define XLIONCORE_PHYSICS_WORLD_H
#pragma once

// The box3d world, wrapped behind a plain, xecs-agnostic class - THIS is what actually stays inside
// LIONCore.dll (see this file's own .cpp). It's a separate translation unit from the ECS system/
// component because xecs::component::type::info_v<T> is a PER-BINARY inline static: any code that
// touches a user component type by name (Search/Foreach/getOrCreateArchetype, all of xecs_system.h's
// query machinery) must be compiled into whichever binary actually calls RegisterComponents<T>() -
// xLION.exe, via xlevel_session.h - not hidden inside this DLL, or it reads the DLL's own separate,
// never-registered copy of that type's metadata (confirmed: this is exactly what crashed here first).
// world has no xecs types in its interface at all, so it has no such constraint and can safely be
// dllexport/dllimport'd across the boundary like any plain C++ class.
#include "dependencies/xmath/source/xmath.h"
#include <box3d/box3d.h>
#include "dependencies/xECSV2/src/xecs_api.h"

namespace xlioncore::physics
{
    class XECS_API world
    {
    public:
        world  (void) noexcept;
        ~world (void) noexcept;

        b3BodyId     CreateBody   (bool bDynamic, const xmath::fvec3& Position, const xmath::fvec3& HalfExtents) noexcept;
        void         Step         (void) noexcept;
        xmath::fvec3 GetPosition  (b3BodyId BodyId) const noexcept;

    private:
        b3WorldId m_World;
    };
}

#endif // XLIONCORE_PHYSICS_WORLD_H
