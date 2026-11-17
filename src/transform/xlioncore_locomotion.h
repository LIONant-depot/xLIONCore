#ifndef XLIONCORE_LOCOMOTION_H
#define XLIONCORE_LOCOMOTION_H
#pragma once
// Locomotion (2026-11-17, animation G0): where a character wants to go, said by the game, and how fast its feet actually carry it, said back by
// its animation - the meeting point of a game's movement code and the animation without either knowing the other (a game module does not see
// the renderer's Animation component). The game writes Move (and Face); an Animation on this entity or on a child of it reads them (its Move
// in the world, FaceMove, or Face) and writes FeetVelocity: the clip's root motion this update as a world velocity (warped and matched). A game
// that moves a physics body holds the body to FeetVelocity: the body goes where the feet go, and the feet do not slide.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xmath/source/xmath.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

namespace xlioncore
{
    struct locomotion
    {
        constexpr static auto typedef_v = xecs::component::type::data{ .m_pName = "Locomotion" };

        xmath::fvec3    m_Move          {0, 0, 0};      // world units per second, horizontal: where the game wants it to go (zero: stand)
        xmath::fvec3    m_Face          {0, 0, 0};      // a world direction to face (zero: face where it goes)
        xmath::fvec3    m_FeetVelocity  {0, 0, 0};      // (written by the animation: how fast its feet carry it this update, world)

        XPROPERTY_DEF
        ( "Locomotion", locomotion
        , obj_member<"Move", &locomotion::m_Move, member_help<"Where the game wants the character to go: a world velocity (units per second). An Animation on this entity or a child of it walks there">>
        , obj_member<"Face", &locomotion::m_Face, member_help<"A world direction to face while it goes (zero: it faces where it goes)">>
        , obj_member<"FeetVelocity", &locomotion::m_FeetVelocity, member_flags<flags::SHOW_READONLY, flags::DONT_SAVE>, member_help<"Written by the animation: how fast its feet carry it this update (world). A game moving a physics body holds the body to it">>
        )
    };
    XSCRIPT_REGISTER_COMPONENT(locomotion, "Animation", 58)
}

#endif
