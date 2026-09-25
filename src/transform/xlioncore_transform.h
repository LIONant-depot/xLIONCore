#ifndef XLIONCORE_TRANSFORM_H
#define XLIONCORE_TRANSFORM_H
#pragma once

// World-space Transform shared by physics + render. No hierarchy this pass - pose is always
// world. Registered by LIONCore.dll (xlioncore_plugin_entry.cpp); xLIONRender SyncLocalBitIDs's it.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"

namespace xlioncore
{
    struct transform
    {
        // Stable GUID so the type identity survives renames / TU moves (default would hash
        // __FUNCSIG__). m_Guid must precede m_pName (declaration order of type::data).
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::transform" }
        , .m_pName = "Transform"
        };

        xmath::fvec3    m_Position              = xmath::fvec3::fromZero();
        xmath::fquat    m_Rotation              = xmath::fquat::fromIdentity(); // source of truth
        xmath::fvec3    m_Scale                 = xmath::fvec3::fromOne();
        xmath::radian3  m_EditorRotation        = {};

        // EditorRotationDegrees: ZXY roll-pitch-yaw in degrees (X=pitch, Y=yaw, Z=roll).
        // xmath::fquat(radian3)/ToEuler() already implement that convention (radian3 docs: rotation
        // order is (Z,X,Y); fquat's "YXZ" comment names the left-to-right multiply order of the same
        // thing). Exposed as a scope of float virtuals - fvec3 is a reflected object, not a virtual
        // atomic, so the bool+fvec3 virtual contract does not apply.
        //
        // Angle API (xmath_trigonometry.h): DegToRad/RadToDeg are template<floating_point T> and
        // take/return bare floats. radian::getDegrees() currently fails MSVC C3445 (returns
        // degree via copy-list-init of an explicit ctor), so convert via DegToRad/RadToDeg +
        // radian{...} / .m_Value instead.
        XPROPERTY_DEF
        ( "Transform", transform
        , obj_member<"Position", &transform::m_Position>
        , obj_member<"Rotation", &transform::m_Rotation, member_flags<flags::DONT_SHOW>>
        , obj_scope<"RotationDegrees", xproperty::settings::vector3_group
            , obj_member<"X", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = xmath::RadToDeg(O.m_EditorRotation.m_Pitch.m_Value);
                    else
                    {
                        O.m_EditorRotation.m_Pitch = xmath::radian{ xmath::DegToRad(V) };
                        O.m_Rotation = xmath::fquat(O.m_EditorRotation);
                    }
                }>
            , obj_member<"Y", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = xmath::RadToDeg(O.m_EditorRotation.m_Yaw.m_Value);
                    else
                    {
                        O.m_EditorRotation.m_Yaw = xmath::radian{ xmath::DegToRad(V) };
                        O.m_Rotation = xmath::fquat(O.m_EditorRotation);
                    }
                }>
            , obj_member<"Z", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = xmath::RadToDeg(O.m_EditorRotation.m_Roll.m_Value);
                    else
                    {
                        O.m_EditorRotation.m_Roll = xmath::radian{ xmath::DegToRad(V) };
                        O.m_Rotation = xmath::fquat(O.m_EditorRotation);
                    }
                }>
            >
        , obj_member<"Scale", &transform::m_Scale>
        )
    };
    XPROPERTY_REG(transform)
}

#endif // XLIONCORE_TRANSFORM_H