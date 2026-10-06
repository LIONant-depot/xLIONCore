#ifndef XLIONCORE_TRANSFORM_H
#define XLIONCORE_TRANSFORM_H
#pragma once

// Transform shared by physics + render: the WORLD pose of an entity that has no parent component (a root), RELATIVE to its parent for one that has (a child; its world pose is derived,
// see xlioncore_hierarchy.h: ask WorldOf, never read m_Position as world). Registered by LIONCore.dll (xlioncore_plugin_entry.cpp); xLIONRender SyncLocalBitIDs's it.
//
// Physics sync (V1 follow-up): m_DirtyToPhysics. Editor/gameplay pose edits set Dirty=1 via
// MarkDirtyToPhysics(). Physics Box3D->ECS writeback must NEVER set Dirty. No physics_teleportation_tag.
// Dynamic bodies ignore DirtyToPhysics pushes while the physics system is simulating (inspector/gizmo
// MarkDirty was resetting Box3D every frame and freezing crates). Kinematic/Static still honor Dirty.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

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
        bool            m_EditorLockScale       = false;

        // ECS -> Box3D dirty flag. Physics writeback never touches this.
        std::uint8_t    m_DirtyToPhysics        : 1 = 0;

        void MarkDirtyToPhysics(void) noexcept
        {
            m_DirtyToPhysics = 1;
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
        // With m_EditorLockScale on, editing one axis scales the other two by the same ratio (proportional).
        inline void             setScaleAxis        (float& Axis, const float New)      noexcept
        {
            if (m_EditorLockScale && Axis != 0.0f) m_Scale *= New / Axis;   // ponytail: old axis 0 has no ratio, falls to plain set
            else                                   Axis = New;
            MarkDirtyToPhysics();
        }
        inline void             setScaleX           (const float X)                     noexcept { setScaleAxis(m_Scale.m_X, X); }
        inline void             setScaleY           (const float Y)                     noexcept { setScaleAxis(m_Scale.m_Y, Y); }
        inline void             setScaleZ           (const float Z)                     noexcept { setScaleAxis(m_Scale.m_Z, Z); }

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
        , obj_member<"EditorLockScale", &transform::m_EditorLockScale, member_flags<flags::DONT_SHOW>>
        )
    };
    // Inspector: Transform category, priority 0 = top of entity inspector.
    XSCRIPT_REGISTER_COMPONENT(transform, "Basics", 0)

    // Draw this physics body between its last two fixed steps (opt-in: the presence of the component is the switch). The physics runs at a fixed rate and the frames do not, so a body drawn at
    // its last step moves in little jumps (a 60 Hz game on a 144 Hz screen shows each pose for 2-3 frames). With this component the physics keeps the pose before the last step and blends it
    // with the current one by game_time::m_FixedInterpolate: what is drawn is m_Position/m_Rotation here. VISUAL ONLY - the Transform stays the pose of the last step, and it is the one
    // gameplay reads and writes; nothing but the render (WorldOf with this component) reads the blended pose. A body that is teleported, or whose Transform is edited, is drawn at its new
    // pose at once (no blend across the jump). The scale is the Transform's. Written by the physics system, never saved.
    struct render_transform
    {
        constexpr static auto typedef_v = xecs::component::type::data
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::render_transform" }
        , .m_pName = "RenderTransform"
        };

        xmath::fvec3    m_PrevPosition  = xmath::fvec3::fromZero();             // the pose before the last fixed step
        xmath::fquat    m_PrevRotation  = xmath::fquat::fromIdentity();
        xmath::fvec3    m_Position      = xmath::fvec3::fromZero();             // the pose to draw
        xmath::fquat    m_Rotation      = xmath::fquat::fromIdentity();
        xmath::fvec3    m_CurPosition   = xmath::fvec3::fromZero();             // the Transform the pose to draw was blended towards: a Transform that is not this one anymore was edited since (a paused game is edited too), and is drawn as it is
        xmath::fquat    m_CurRotation   = xmath::fquat::fromIdentity();
        bool            m_bActive       = false;                               // m_Position/m_Rotation hold a blended pose (the physics has run): until then the Transform is drawn
        bool            m_bSnap         = false;                                // the body was moved without motion (teleport): draw it where it is, once

        // What the Inspector (and a test) can see of it while it plays: where it is drawn, and from where it was blended. Never saved.
        XPROPERTY_DEF
        ( "RenderTransform", render_transform
        , obj_member<"Position",     &render_transform::m_Position,     member_flags<flags::SHOW_READONLY, flags::DONT_SAVE>>
        , obj_member<"PrevPosition", &render_transform::m_PrevPosition, member_flags<flags::SHOW_READONLY, flags::DONT_SAVE>>
        , obj_member<"Active",       &render_transform::m_bActive,      member_flags<flags::SHOW_READONLY, flags::DONT_SAVE>>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(render_transform, "Basics", 8)
}

#endif // XLIONCORE_TRANSFORM_H
