#ifndef XLIONCORE_HIERARCHY_H
#define XLIONCORE_HIERARCHY_H
#pragma once

// The hierarchy of the transforms.
//
//   - An entity WITHOUT a parent component is a root: its Transform is its world pose (physics, render and picking read it as they always did).
//   - An entity WITH a parent component is a child: its Transform is relative to its parent, and the parent component holds what is derived from it - the child's world pose
//     (xecs::component::parent::m_WorldPosition/Rotation/Scale), written by PropagateHierarchy and never saved. What a child takes from its parent (the three position axes, the
//     rotation, the scale) is the parent component's m_Follow: what it does not take is its own Transform value, as a WORLD value (a shadow follows x and z, not y).
//   - Physics ignores children (none_of<parent>): only roots are bodies.
//
// Anything that needs the world pose of an entity that may be a child calls WorldOf(transform, parent*) with the parent component when the entity has one (null when not):
// it never reads Transform::m_Position as world. The pose is the one of the last PropagateHierarchy: the render system runs it every frame, after everything that moves things.
#include "xlioncore_transform.h"

namespace xlioncore
{
    struct world_pose
    {
        xmath::fvec3    m_Position  = xmath::fvec3::fromZero();
        xmath::fquat    m_Rotation  = xmath::fquat::fromIdentity();
        xmath::fvec3    m_Scale     = xmath::fvec3::fromOne();

        xmath::fmat4 Matrix(void) const noexcept
        {
            xmath::fmat4 M;
            M.setupSRT(m_Scale, m_Rotation, m_Position);
            return M;
        }
    };

    // A root's world pose is its Transform.
    inline world_pose PoseOf(const transform& T) noexcept { return { T.m_Position, T.m_Rotation, T.m_Scale }; }

    // A child's world pose, as it was last derived.
    inline world_pose PoseOf(const xecs::component::parent& P) noexcept
    {
        return { xmath::fvec3(P.m_WorldPosition[0], P.m_WorldPosition[1], P.m_WorldPosition[2])
               , xmath::fquat(P.m_WorldRotation[0], P.m_WorldRotation[1], P.m_WorldRotation[2], P.m_WorldRotation[3])
               , xmath::fvec3(P.m_WorldScale[0], P.m_WorldScale[1], P.m_WorldScale[2]) };
    }

    inline void Store(xecs::component::parent& P, const world_pose& W) noexcept
    {
        P.m_WorldPosition[0] = W.m_Position.m_X; P.m_WorldPosition[1] = W.m_Position.m_Y; P.m_WorldPosition[2] = W.m_Position.m_Z;
        P.m_WorldRotation[0] = W.m_Rotation.m_X; P.m_WorldRotation[1] = W.m_Rotation.m_Y; P.m_WorldRotation[2] = W.m_Rotation.m_Z; P.m_WorldRotation[3] = W.m_Rotation.m_W;
        P.m_WorldScale[0]    = W.m_Scale.m_X;    P.m_WorldScale[1]    = W.m_Scale.m_Y;    P.m_WorldScale[2]    = W.m_Scale.m_Z;
    }

    // The world pose of an entity that is a root (pParent null: its Transform is the world pose) or a child (its parent component has it).
    inline world_pose WorldOf(const transform& T, const xecs::component::parent* pParent) noexcept { return pParent ? PoseOf(*pParent) : PoseOf(T); }

    // The world pose of a child whose parent is at Parent and whose own (relative) Transform is Local, taking from the parent what Follow says.
    inline world_pose Compose(const world_pose& Parent, const transform& Local, std::uint8_t Follow) noexcept
    {
        using P = xecs::component::parent;
        const bool bScale = (Follow & P::FOLLOW_SCALE)    != 0;
        const bool bRot   = (Follow & P::FOLLOW_ROTATION) != 0;

        world_pose W;
        W.m_Scale    = bScale ? xmath::fvec3(Parent.m_Scale.m_X * Local.m_Scale.m_X, Parent.m_Scale.m_Y * Local.m_Scale.m_Y, Parent.m_Scale.m_Z * Local.m_Scale.m_Z) : Local.m_Scale;
        W.m_Rotation = bRot ? Parent.m_Rotation * Local.m_Rotation : Local.m_Rotation;

        xmath::fvec3 Offset = Local.m_Position;
        if (bScale) Offset = xmath::fvec3(Offset.m_X * Parent.m_Scale.m_X, Offset.m_Y * Parent.m_Scale.m_Y, Offset.m_Z * Parent.m_Scale.m_Z);
        if (bRot)   Offset = Parent.m_Rotation.RotateVector(Offset);
        W.m_Position = Parent.m_Position + Offset;

        // an axis that does not follow: the child's own value, in the world
        if (!(Follow & P::FOLLOW_X)) W.m_Position.m_X = Local.m_Position.m_X;
        if (!(Follow & P::FOLLOW_Y)) W.m_Position.m_Y = Local.m_Position.m_Y;
        if (!(Follow & P::FOLLOW_Z)) W.m_Position.m_Z = Local.m_Position.m_Z;
        return W;
    }

    // The inverse of Compose: the relative Transform that puts a child of Parent at the world pose New (what the editor's gizmo needs: it moves the child in the world and the Transform
    // keeps the relative value). Only the channels in Channels (a mask of FOLLOW_ROTATION, FOLLOW_SCALE and the three axes, as in the parent's m_Follow, plus 0x20 for "the position")
    // are written, so the other ones stay bit-exact.
    inline void LocalFromWorld(const world_pose& Parent, const world_pose& New, std::uint8_t Follow, bool bPosition, bool bRotation, bool bScale, transform& Local) noexcept
    {
        using P = xecs::component::parent;
        const bool bFollowScale = (Follow & P::FOLLOW_SCALE)    != 0;
        const bool bFollowRot   = (Follow & P::FOLLOW_ROTATION) != 0;
        if (bScale)    Local.m_Scale = bFollowScale ? xmath::fvec3(New.m_Scale.m_X / Parent.m_Scale.m_X, New.m_Scale.m_Y / Parent.m_Scale.m_Y, New.m_Scale.m_Z / Parent.m_Scale.m_Z) : New.m_Scale;
        if (bRotation) Local.m_Rotation = bFollowRot ? Parent.m_Rotation.InverseCopy() * New.m_Rotation : New.m_Rotation;
        if (bPosition)
        {
            xmath::fvec3 Offset = New.m_Position - Parent.m_Position;
            if (bFollowRot)   Offset = Parent.m_Rotation.InverseCopy().RotateVector(Offset);
            if (bFollowScale) Offset = xmath::fvec3(Offset.m_X / Parent.m_Scale.m_X, Offset.m_Y / Parent.m_Scale.m_Y, Offset.m_Z / Parent.m_Scale.m_Z);
            Local.m_Position.m_X = (Follow & P::FOLLOW_X) ? Offset.m_X : New.m_Position.m_X;
            Local.m_Position.m_Y = (Follow & P::FOLLOW_Y) ? Offset.m_Y : New.m_Position.m_Y;
            Local.m_Position.m_Z = (Follow & P::FOLLOW_Z) ? Offset.m_Z : New.m_Position.m_Z;
        }
    }

    // Walks down from a parent whose world pose is known, deriving the pose of each child (and of theirs). A cycle in the data (a bug) ends at Depth 64 instead of hanging the frame.
    template< typename T_SYSTEM >
    inline void PropagateFrom(T_SYSTEM& System, const world_pose& ParentWorld, const std::vector<xecs::component::entity>& Kids, int Depth) noexcept
    {
        if (Depth > 64) return;
        for (const auto& Kid : Kids)
        {
            // a child that was deleted (a zombie until the end of the frame) is still in the list of its parent for a while: skip it
            if (!System.getGameMgr().m_ComponentMgr.isEntityValid(Kid)) continue;
            // findEntity asks for components the entity must have (it aborts otherwise): the archetype says first what it has
            const auto& Bits = System.getArchetype(Kid).getComponentBits();
            if (!Bits.getBit(xecs::component::type::info_v<transform>.m_BitID) || !Bits.getBit(xecs::component::type::info_v<xecs::component::parent>.m_BitID)) continue;

            world_pose Mine;
            (void)System.findEntity(Kid, [&](const transform& T, xecs::component::parent& P) noexcept
            {
                Mine = Compose(ParentWorld, T, P.m_Follow);
                Store(P, Mine);
            });
            if (!Bits.getBit(xecs::component::type::info_v<xecs::component::children>.m_BitID)) continue;
            (void)System.findEntity(Kid, [&](const xecs::component::children& C) noexcept { PropagateFrom(System, Mine, C.m_List, Depth + 1); });
        }
    }

    // Derives the world pose of every child of the world the system belongs to: from each root that has children, down. Cheap when there is no hierarchy (one query that matches nothing).
    template< typename T_SYSTEM >
    inline void PropagateHierarchy(T_SYSTEM& System) noexcept
    {
        xecs::query::instance Query;
        Query.m_Must.AddFromComponents<transform, xecs::component::children>();
        Query.m_NoneOf.AddFromComponents<xecs::component::parent>();
        auto S = System.Search(Query);
        System.Foreach(S, [&](const transform& T, const xecs::component::children& C) noexcept { PropagateFrom(System, PoseOf(T), C.m_List, 0); });
    }
}

#endif // XLIONCORE_HIERARCHY_H
