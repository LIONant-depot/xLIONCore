#ifndef XLIONCORE_PHYSICS_BACKEND_H
#define XLIONCORE_PHYSICS_BACKEND_H
#pragma once

// The box3d world, wrapped behind a plain class with no xecs types in its interface at all. Purely
// internal to this DLL now - both this and the ECS-facing system/component (xlioncore_physics_system.h)
// are compiled into LIONCore.dll (see xlioncore_plugin_entry.cpp), so nothing here needs dllexport/
// dllimport: the only symbols this DLL exports are the two self-registration entry points
// (XecsPlugin_RegisterComponents/RegisterSystems, resolved by xLION.exe via GetProcAddress, same ABI
// Game.dll already uses) - see xlioncore_plugin_entry.cpp's own comment for why registration and
// entity iteration have to live in the same binary.
#include "dependencies/xmath/source/xmath.h"
#include <box3d/box3d.h>

namespace xlioncore::physics
{
    class backend
    {
    public:
        backend  (void) noexcept;
        ~backend (void) noexcept;

        b3BodyId     CreateBody   (bool bDynamic, const xmath::fvec3& Position, const xmath::fvec3& HalfExtents) noexcept;
        void         Step         (void) noexcept;
        xmath::fvec3 GetPosition  (b3BodyId BodyId) const noexcept;

    private:
        b3WorldId m_World;
    };
}

#endif // XLIONCORE_PHYSICS_BACKEND_H
