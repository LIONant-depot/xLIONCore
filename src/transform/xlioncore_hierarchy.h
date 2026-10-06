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
#include "../game/xlioncore_game.h"

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

    // How far between the last two fixed steps the frame is drawn (game_time::m_FixedInterpolate; 1 when the world has no game).
    inline float FixedInterpolateOf(xecs::game_mgr::instance& GameMgr) noexcept
    {
        const auto* pGame = game::From(GameMgr);
        return pGame ? pGame->m_Time.m_FixedInterpolate : 1.0f;
    }

    // What to DRAW: the same, but a root that has a render_transform is drawn between the pose before its last fixed step and its Transform, Interpolate (the game's m_FixedInterpolate) of the
    // way. A body with no previous pose yet, or further from it than render_transform::kSnapDistance (teleported, or too fast to smooth), is drawn at its Transform. Only the render asks this.
    inline world_pose DrawnPoseOf(const transform& T, const render_transform* pRender, float Interpolate) noexcept
    {
        if (!pRender || !pRender->m_PrevPosition.isFinite()) return PoseOf(T);
        const xmath::fvec3 Move = T.m_Position - pRender->m_PrevPosition;
        if (Move.Dot(Move) > render_transform::kSnapDistance * render_transform::kSnapDistance) return PoseOf(T);
        return { pRender->m_PrevPosition + Move * Interpolate
               , xmath::fquat::Slerp(pRender->m_PrevRotation, T.m_Rotation, Interpolate)
               , T.m_Scale };
    }
    inline world_pose WorldOf(const transform& T, const xecs::component::parent* pParent, const render_transform* pRender, float Interpolate) noexcept { return pParent ? PoseOf(*pParent) : DrawnPoseOf(T, pRender, Interpolate); }

    // The turn of Rotation around the vertical (y) axis alone: its twist (swing-twist split), the heading of what it turns. Identity when there is none to take (it points up or down).
    inline xmath::fquat HeadingOf(const xmath::fquat& Rotation) noexcept
    {
        const float L = std::sqrt(Rotation.m_Y * Rotation.m_Y + Rotation.m_W * Rotation.m_W);
        return L < 1.0e-6f ? xmath::fquat::fromIdentity() : xmath::fquat(0.0f, Rotation.m_Y / L, 0.0f, Rotation.m_W / L);
    }

    // The rotation of the parent that the child takes: all of it, only its heading, or none (identity).
    inline xmath::fquat FollowedRotation(const world_pose& Parent, std::uint8_t Follow) noexcept
    {
        using P = xecs::component::parent;
        if (Follow & P::FOLLOW_ROTATION) return Parent.m_Rotation;
        if (Follow & P::FOLLOW_HEADING)  return HeadingOf(Parent.m_Rotation);
        return xmath::fquat::fromIdentity();
    }

    // The scale of the parent that the child takes, axis by axis (1 for an axis it does not take).
    inline xmath::fvec3 FollowedScale(const world_pose& Parent, std::uint8_t Follow) noexcept
    {
        using P = xecs::component::parent;
        return xmath::fvec3((Follow & P::FOLLOW_SCALE_X) ? Parent.m_Scale.m_X : 1.0f, (Follow & P::FOLLOW_SCALE_Y) ? Parent.m_Scale.m_Y : 1.0f, (Follow & P::FOLLOW_SCALE_Z) ? Parent.m_Scale.m_Z : 1.0f);
    }

    // The world pose of a child whose parent is at Parent and whose own (relative) Transform is Local, taking from the parent what Follow says.
    inline world_pose Compose(const world_pose& Parent, const transform& Local, std::uint8_t Follow) noexcept
    {
        using P = xecs::component::parent;
        const xmath::fquat  Turn  = FollowedRotation(Parent, Follow);
        const xmath::fvec3  Scale = FollowedScale(Parent, Follow);

        world_pose W;
        W.m_Scale    = xmath::fvec3(Scale.m_X * Local.m_Scale.m_X, Scale.m_Y * Local.m_Scale.m_Y, Scale.m_Z * Local.m_Scale.m_Z);
        W.m_Rotation = Turn * Local.m_Rotation;

        // the offset is in the frame of the parent: scaled by what is followed of its scale, turned by what is followed of its rotation
        const xmath::fvec3 Scaled(Local.m_Position.m_X * Scale.m_X, Local.m_Position.m_Y * Scale.m_Y, Local.m_Position.m_Z * Scale.m_Z);
        W.m_Position = Parent.m_Position + Turn.RotateVector(Scaled);

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
        const xmath::fquat Turn  = FollowedRotation(Parent, Follow);
        const xmath::fvec3 Scale = FollowedScale(Parent, Follow);
        if (bScale)    Local.m_Scale    = xmath::fvec3(New.m_Scale.m_X / Scale.m_X, New.m_Scale.m_Y / Scale.m_Y, New.m_Scale.m_Z / Scale.m_Z);
        if (bRotation) Local.m_Rotation = Turn.InverseCopy() * New.m_Rotation;
        if (bPosition)
        {
            const xmath::fvec3 Offset = Turn.InverseCopy().RotateVector(New.m_Position - Parent.m_Position);
            const xmath::fvec3 Unscaled(Offset.m_X / Scale.m_X, Offset.m_Y / Scale.m_Y, Offset.m_Z / Scale.m_Z);
            Local.m_Position.m_X = (Follow & P::FOLLOW_X) ? Unscaled.m_X : New.m_Position.m_X;
            Local.m_Position.m_Y = (Follow & P::FOLLOW_Y) ? Unscaled.m_Y : New.m_Position.m_Y;
            Local.m_Position.m_Z = (Follow & P::FOLLOW_Z) ? Unscaled.m_Z : New.m_Position.m_Z;
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
        const float Interpolate = FixedInterpolateOf(System.getGameMgr());
        System.Foreach(S, [&](const transform& T, const xecs::component::children& C, const render_transform* pRender) noexcept { PropagateFrom(System, DrawnPoseOf(T, pRender, Interpolate), C.m_List, 0); });
    }
}

#endif // XLIONCORE_HIERARCHY_H
