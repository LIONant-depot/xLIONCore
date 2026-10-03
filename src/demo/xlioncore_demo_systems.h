#ifndef XLIONCORE_DEMO_SYSTEMS_H
#define XLIONCORE_DEMO_SYSTEMS_H
#pragma once

// Two trivial demo Update systems. The editor otherwise has no system of its own to list, reorder, enable or watch in the System Registry panel and the Play/Stop toggle; each one prints a line
// once per tick, so reordering them changes which line comes first and disabling one stops its line - the whole verification surface of those features.
//
// They live in the core (they were in the editor): xECS code that runs systems has to be compiled into the copy of the core that owns the registry, never into the editor, which is not bound to any one copy.
// The guids are the ones the editor's System Registry already saved (Project.config/SystemOrder.config.txt): the default guid of a system is a hash of its type's name, which moved with the type.
#include "dependencies/xECSV2/src/xecs.h"
#include "../physics/xlioncore_physics.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"
#include <cstdio>

namespace xlioncore::demo
{
    struct tick_logger_a : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::update{ .m_pName = "Tick Logger A", .m_Guid = xecs::system::type::guid{ 0xFE5EC75487196B0Eull } };

        tick_logger_a(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnUpdate(void) noexcept
        {
            std::printf("[System] Tick Logger A\n");
            std::fflush(stdout);
        }
    };

    struct tick_logger_b : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::update{ .m_pName = "Tick Logger B", .m_Guid = xecs::system::type::guid{ 0xCD201CFEDF3D5639ull } };

        tick_logger_b(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnUpdate(void) noexcept
        {
            std::printf("[System] Tick Logger B\n");
            std::fflush(stdout);
        }
    };

    // The two listeners of the physics events: global event systems (a system that runs when the physics says something, not once per tick). They say what they were told, which is how the
    // events are seen from outside (the tests read these lines) and the example of how a game listens.
    struct sensor_begin_logger : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::global_event<xlioncore::physics::sensor_begin_event>{ .m_pName = "Sensor Begin Logger" };

        sensor_begin_logger(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnEvent(xecs::component::entity Sensor, xecs::component::entity Visitor) noexcept
        {
            std::printf("[System] Sensor begin sensor=%llX visitor=%llX\n", static_cast<unsigned long long>(Sensor.m_Value), static_cast<unsigned long long>(Visitor.m_Value));
            std::fflush(stdout);
        }
    };

    struct sensor_end_logger : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::global_event<xlioncore::physics::sensor_end_event>{ .m_pName = "Sensor End Logger" };

        sensor_end_logger(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnEvent(xecs::component::entity Sensor, xecs::component::entity Visitor) noexcept
        {
            std::printf("[System] Sensor end sensor=%llX visitor=%llX\n", static_cast<unsigned long long>(Sensor.m_Value), static_cast<unsigned long long>(Visitor.m_Value));
            std::fflush(stdout);
        }
    };
}

using demo_tick_logger_a = xlioncore::demo::tick_logger_a;
XSCRIPT_REGISTER_SYSTEM(demo_tick_logger_a)

using demo_tick_logger_b = xlioncore::demo::tick_logger_b;
XSCRIPT_REGISTER_SYSTEM(demo_tick_logger_b)

using demo_sensor_begin_logger = xlioncore::demo::sensor_begin_logger;
XSCRIPT_REGISTER_SYSTEM(demo_sensor_begin_logger)

using demo_sensor_end_logger = xlioncore::demo::sensor_end_logger;
XSCRIPT_REGISTER_SYSTEM(demo_sensor_end_logger)

#endif // XLIONCORE_DEMO_SYSTEMS_H
