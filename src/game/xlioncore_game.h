#ifndef XLIONCORE_GAME_H
#define XLIONCORE_GAME_H
#pragma once

// The game: the one place that officially has the xecs::game_mgr::instance, and the services the systems of that instance need (the time, the
// physics, ...). xECS itself knows nothing about any of this; a system reaches the game through its game manager:
//
//      auto& Game = *GameMgr.getUserData<xlioncore::game>();       // GameMgr is what the system was constructed with
//      Game.m_Time.m_FixedSteps                                    // how many fixed steps are due this frame
//
// The editor holds a game (one for each Level Editor); the level is data that the game loads into its game manager.
#include "dependencies/xECSV2/src/xecs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <memory>

namespace xlioncore::physics { struct system; }
namespace xlioncore { struct xECSEditor; }

namespace xlioncore
{
    //---------------------------------------------------------------------------------------------
    // The constraints of the systems of a game (see xecs::system::constraint): what a system needs of the place it is placed in. The connectors of the physics system give them.
    //---------------------------------------------------------------------------------------------
    namespace constraint
    {
        // Every time the system runs the game moves forward by exactly m_FixedDeltaTime. Where the system runs relative to the step (before it, after it) is a choice of where it is placed, not
        // a constraint: any place that gives this one works.
        struct fixed_delta_time
        {
            static constexpr auto typedef_v = xecs::system::constraint::def
            { .m_pName          = "Fixed Delta Time"
            , .m_pDescription   = "Every time the system runs, the game moves forward by exactly one fixed delta time (1/60 s): it runs once for each fixed step the game's time says is due, as many times in a frame as that, none when no step is due. A system that works with the physics needs this: its delta time is always the same" };
        };
    }

    //---------------------------------------------------------------------------------------------
    // The time of a game. The editor sets m_TimeScale (the speed slider) and m_bPaused; Advance() is called once a frame by game::Run.
    //
    //   m_RealDeltaTime   what the last frame really took (never more than m_MaxRealDeltaTime: a breakpoint is not game time)
    //   m_DeltaTime       the same times m_TimeScale: what gameplay that follows the frame reads
    //   m_FixedDeltaTime  the size of one fixed step (1/60): what the physics and everything that works with it reads
    //   m_FixedInterpolate how far between the last two fixed states this frame is drawn: m_FixedAccumulator / m_FixedDeltaTime (0..1), 1 when there is nothing to blend (a game that has not
    //                     started, a single Step). What draws a thing that the fixed steps move blends its previous pose and its current one with it: what is drawn is then always one
    //                     step behind the game, and never jumps. Gameplay never reads it: the rules work on the state of the last step.
    //   m_FixedSteps      how many fixed steps are due this frame:
    //                         m_FixedAccumulator += m_DeltaTime;  while( m_FixedAccumulator >= m_FixedDeltaTime ) { run a fixed step; m_FixedAccumulator -= m_FixedDeltaTime; }
    //                     what the accumulator may hold is capped (m_MaxFixedAccumulated): past that the game runs slower than real time
    //                     instead of spending longer and longer in each frame.
    //---------------------------------------------------------------------------------------------
    struct game_time
    {
        static constexpr float kDefaultFixedDeltaTime = 1.0f / 60.0f;

        // The speeds the multiplier takes (the slider next to Play snaps to them, and so does SetTimeScale): discrete, so 1x is always one stop away.
        static constexpr std::array<float, 7> kScaleSteps = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f };

        static float NearestScaleStep( float Scale ) noexcept
        {
            float Best = kScaleSteps[0];
            for (float Step : kScaleSteps) if (std::fabs(Step - Scale) < std::fabs(Best - Scale)) Best = Step;
            return Best;
        }

        // set from outside
        float           m_TimeScale             = 1.0f;                     // the multiplier: slower below 1, faster above
        bool            m_bPaused               = false;                    // a paused game computes no frames (Step still moves one fixed step)
        float           m_FixedDeltaTime        = kDefaultFixedDeltaTime;   // m_FixedDeltaTimeStepSize
        float           m_MaxFixedAccumulated   = 0.25f;                    // seconds of game time the accumulator may hold
        float           m_MaxRealDeltaTime      = 0.10f;

        // what Advance() computes
        float           m_RealDeltaTime         = 0.0f;
        float           m_DeltaTime             = 0.0f;
        float           m_FixedAccumulator      = 0.0f;
        int             m_FixedSteps            = 0;
        float           m_FixedInterpolate      = 1.0f;                     // see above
        double          m_Time                  = 0.0;                      // game time: the sum of m_DeltaTime
        double          m_FixedTime             = 0.0;                      // the sum of the fixed steps taken
        std::uint64_t   m_FramesComputed        = 0;                        // frames the game really ran (a paused game does not count)
        std::uint64_t   m_FixedStepsComputed    = 0;

        // Back to the start of a game (Play): the multiplier and the pause stay as the user left them.
        void Reset() noexcept
        {
            m_RealDeltaTime = m_DeltaTime = m_FixedAccumulator = 0.0f;
            m_FixedInterpolate = 1.0f;
            m_FixedSteps = 0; m_Time = m_FixedTime = 0.0; m_FramesComputed = m_FixedStepsComputed = 0;
        }

        // One frame of RealDt seconds went by.
        void Advance( float RealDt ) noexcept
        {
            m_RealDeltaTime = std::clamp(RealDt, 0.0f, m_MaxRealDeltaTime);
            m_DeltaTime     = m_RealDeltaTime * m_TimeScale;
            m_Time         += m_DeltaTime;

            m_FixedAccumulator = std::min(m_FixedAccumulator + m_DeltaTime, m_MaxFixedAccumulated);
            m_FixedSteps = 0;
            while( m_FixedAccumulator >= m_FixedDeltaTime ) { m_FixedAccumulator -= m_FixedDeltaTime; ++m_FixedSteps; }
            m_FixedInterpolate = m_FixedDeltaTime > 0.0f ? std::clamp(m_FixedAccumulator / m_FixedDeltaTime, 0.0f, 1.0f) : 1.0f;

            m_FixedTime         += m_FixedSteps * static_cast<double>(m_FixedDeltaTime);
            m_FixedStepsComputed += static_cast<std::uint64_t>(m_FixedSteps);
            ++m_FramesComputed;
        }

        // Exactly one fixed step, whatever the multiplier (the Step button).
        void AdvanceOneFixedStep() noexcept
        {
            m_RealDeltaTime = 0.0f;
            m_DeltaTime     = m_FixedDeltaTime;
            m_Time         += m_DeltaTime;
            m_FixedSteps    = 1;
            m_FixedInterpolate = 1.0f;
            m_FixedTime    += m_FixedDeltaTime;
            ++m_FixedStepsComputed;
            ++m_FramesComputed;
        }
    };

    //---------------------------------------------------------------------------------------------
    struct game
    {
        std::unique_ptr<xecs::game_mgr::instance>   m_pGameMgr;
        game_time                                   m_Time;
        physics::system*                            m_pPhysics = nullptr;       // set by the physics system while it is registered in m_pGameMgr
        xECSEditor*                                 m_pEditor  = nullptr;       // the editor interface that made this game (null when the game is made directly): see xlioncore_editor.h

        // A new, empty game manager that knows this game (user data), replacing the previous one. The time keeps its multiplier and pause.
        xecs::game_mgr::instance& CreateWorld() noexcept
        {
            m_pPhysics = nullptr;
            m_pGameMgr = std::make_unique<xecs::game_mgr::instance>();
            m_pGameMgr->setUserData(this);
            m_Time.Reset();
            m_LastTick = {};
            return *m_pGameMgr;
        }

        void DestroyWorld() noexcept { m_pGameMgr.reset(); m_pPhysics = nullptr; }

        // A world whose systems crashed while they were being registered is half built: destroying it would run the destructors of
        // what the crash left behind, so it is leaked instead (it holds nothing yet: no level is loaded when systems register).
        void AbandonWorld() noexcept { (void)m_pGameMgr.release(); m_pPhysics = nullptr; }

        // One frame: the time moves (by the real time since the last call), then the game manager runs its systems.
        void Run() noexcept
        {
            const auto Now = std::chrono::steady_clock::now();
            const float Dt = m_LastTick.time_since_epoch().count() == 0 ? game_time::kDefaultFixedDeltaTime : std::chrono::duration<float>(Now - m_LastTick).count();
            m_LastTick = Now;
            if (m_Time.m_bPaused) return;
            m_Time.Advance(Dt);
            m_pGameMgr->Run();
        }

        // Exactly one fixed step (1/60 s), for the Step button while the game is paused.
        void StepOnce() noexcept
        {
            m_Time.AdvanceOneFixedStep();
            m_pGameMgr->Run();
            m_LastTick = {};                // the next Run starts counting from there
        }

        // The game a game manager belongs to (null when it has none, for example a manager only used to register components).
        static game* From( xecs::game_mgr::instance& GameMgr ) noexcept { return GameMgr.getUserData<game>(); }

    private:
        std::chrono::steady_clock::time_point m_LastTick{};
    };
}

#endif // XLIONCORE_GAME_H
