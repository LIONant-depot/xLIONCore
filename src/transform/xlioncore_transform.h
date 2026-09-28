#ifndef XLIONCORE_TRANSFORM_H
#define XLIONCORE_TRANSFORM_H
#pragma once

// World-space Transform shared by physics + render. No hierarchy this pass - pose is always
// world. Registered by LIONCore.dll (xlioncore_plugin_entry.cpp); xLIONRender SyncLocalBitIDs's it.
//
// Physics sync (V1 follow-up): m_DirtyToPhysics + m_PhysicsSyncCoolDown. Editor/gameplay pose
// edits set Dirty=1 and CoolDown=N via MarkDirtyToPhysics(). Physics Box3D->ECS writeback must NEVER set Dirty. No physics_teleportation_tag.
// Dynamic bodies ignore DirtyToPhysics pushes while the physics system is simulating (inspector/gizmo
// MarkDirty was resetting Box3D every frame and freezing crates). Kinematic/Static still honor Dirty.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

namespace xlioncore
{
    struct transform
    {
        // CoolDown ticks to re-arm after a Dirty push (physics ticks). Start N in 4..8.
        static constexpr std::uint8_t kPhysicsSyncCoolDownN = 6;

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

        // ECS -> Box3D dirty/cooldown (bitfield). Physics writeback never touches these.
        std::uint8_t    m_DirtyToPhysics        : 1 = 0;
        std::uint8_t    m_PhysicsSyncCoolDown   : 7 = 0;

        void MarkDirtyToPhysics(void) noexcept
        {
            m_DirtyToPhysics      = 1;
            m_PhysicsSyncCoolDown = kPhysicsSyncCoolDownN;
        }

        inline void             setPosition         (const xmath::fvec3& V )            noexcept { m_Position     = V; MarkDirtyToPhysics(); }
        inline void             setPositionX        (const float X)                     noexcept { m_Position.m_X = X; MarkDirtyToPhysics(); }
        inline void             setPositionY        (const float Y)                     noexcept { m_Position.m_Y = Y; MarkDirtyToPhysics(); }
        inline void             setPositionZ        (const float Z)                     noexcept { m_Position.m_Z = Z; MarkDirtyToPhysics(); }
        inline void             setRotation         (const xmath::fquat& Q )            noexcept { m_Rotation     = Q; MarkDirtyToPhysics(); }
        inline xmath::radian3   getEditorRotation   (void)                              noexcept { if ( xmath::Abs(m_Rotation.Dot(m_EditorRotation)) < 0.9999 ){ m_EditorRotation = m_Rotation.ToEuler();} return m_EditorRotation;}
        inline void             setRoll             (const xmath::radian Angle)         noexcept { auto R = getEditorRotation(); R.m_Roll  = Angle; setRotation(R); }
        inline void             setPitch            (const xmath::radian Angle)         noexcept { auto R = getEditorRotation(); R.m_Pitch = Angle; setRotation(R); }
        inline void             setYaw              (const xmath::radian Angle)         noexcept { auto R = getEditorRotation(); R.m_Yaw   = Angle; setRotation(R); }
        inline void             setRotation         (const xmath::radian3& Radian3)     noexcept { m_EditorRotation          = Radian3; setRotation(xmath::fquat{m_EditorRotation}); }
        inline void             setScale            (const xmath::fvec3& V )            noexcept { m_Scale     = V; MarkDirtyToPhysics(); }
        inline void             setScaleX           (const float X)                     noexcept { m_Scale.m_X = X; MarkDirtyToPhysics(); }
        inline void             setScaleY           (const float Y)                     noexcept { m_Scale.m_Y = Y; MarkDirtyToPhysics(); }
        inline void             setScaleZ           (const float Z)                     noexcept { m_Scale.m_Z = Z; MarkDirtyToPhysics(); }

        // EditorRotationDegrees: ZXY roll-pitch-yaw in degrees (X=pitch, Y=yaw, Z=roll).
        // Position/Scale/RotationDegrees writers call MarkDirtyToPhysics so editor edits push to Box3D.
        XPROPERTY_DEF
        ( "Transform", transform
        , obj_scope<"Position", xproperty::settings::vector3_group
            , obj_member<"X", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = O.m_Position.m_X;
                    else O.setPositionX(V);
                }>
            , obj_member<"Y", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = O.m_Position.m_Y;
                    else O.setPositionY(V);
                }>
            , obj_member<"Z", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = O.m_Position.m_Z;
                    else O.setPositionZ(V);
                }>
            >
        , obj_member<"Rotation", &transform::m_Rotation, member_flags<flags::DONT_SHOW>>
        , obj_scope<"RotationDegrees", xproperty::settings::vector3_group
            , obj_member<"X", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = xmath::RadToDeg(O.getEditorRotation().m_Pitch.m_Value);
                    else       O.setPitch(xmath::radian{ xmath::DegToRad(V) });
                }>
            , obj_member<"Y", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = xmath::RadToDeg(O.getEditorRotation().m_Yaw.m_Value);
                    else       O.setYaw(xmath::radian{ xmath::DegToRad(V) });
                }>
            , obj_member<"Z", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = xmath::RadToDeg(O.getEditorRotation().m_Roll.m_Value);
                    else       O.setRoll(xmath::radian{ xmath::DegToRad(V) });
                }>
            >
        , obj_scope<"Scale", xproperty::settings::vector3_group
            , obj_member<"X", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = O.m_Scale.m_X;
                    else O.setScaleX(V);
                }>
            , obj_member<"Y", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = O.m_Scale.m_Y;
                    else O.setScaleY(V);
                }>
            , obj_member<"Z", +[](transform& O, bool bRead, float& V)
                {
                    if (bRead) V = O.m_Scale.m_Z;
                    else O.setScaleZ(V);
                }>
            >
        )
    };
    // Inspector: Transform category, priority 0 = top of entity inspector.
    XSCRIPT_REGISTER_COMPONENT(transform, "Transform", 0)
}

#endif // XLIONCORE_TRANSFORM_H
