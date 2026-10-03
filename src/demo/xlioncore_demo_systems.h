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

        void OnEvent(const xlioncore::physics::sensor_touch& Touch) noexcept
        {
            std::printf("[System] Sensor begin sensor=%llX visitor=%llX\n", static_cast<unsigned long long>(Touch.m_Sensor.m_Value), static_cast<unsigned long long>(Touch.m_Visitor.m_Value));
            std::fflush(stdout);
        }
    };

    struct sensor_end_logger : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::global_event<xlioncore::physics::sensor_end_event>{ .m_pName = "Sensor End Logger" };

        sensor_end_logger(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnEvent(const xlioncore::physics::sensor_touch& Touch) noexcept
        {
            std::printf("[System] Sensor end sensor=%llX visitor=%llX\n", static_cast<unsigned long long>(Touch.m_Sensor.m_Value), static_cast<unsigned long long>(Touch.m_Visitor.m_Value));
            std::fflush(stdout);
        }
    };

    // The same for the solid contacts of the shapes with ContactEvents on.
    struct contact_begin_logger : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::global_event<xlioncore::physics::contact_begin_event>{ .m_pName = "Contact Begin Logger" };

        contact_begin_logger(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnEvent(const xlioncore::physics::contact_touch& Touch) noexcept
        {
            std::printf("[System] Contact begin a=%llX b=%llX\n", static_cast<unsigned long long>(Touch.m_A.m_Value), static_cast<unsigned long long>(Touch.m_B.m_Value));
            std::fflush(stdout);
        }
    };

    struct contact_end_logger : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::global_event<xlioncore::physics::contact_end_event>{ .m_pName = "Contact End Logger" };

        contact_end_logger(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnEvent(const xlioncore::physics::contact_touch& Touch) noexcept
        {
            std::printf("[System] Contact end a=%llX b=%llX\n", static_cast<unsigned long long>(Touch.m_A.m_Value), static_cast<unsigned long long>(Touch.m_B.m_Value));
            std::fflush(stdout);
        }
    };

    struct contact_hit_logger : xecs::system::instance
    {
        constexpr static auto typedef_v = xecs::system::type::global_event<xlioncore::physics::contact_hit_event>{ .m_pName = "Contact Hit Logger" };

        contact_hit_logger(xecs::game_mgr::instance& GameMgr) noexcept : xecs::system::instance(GameMgr) {}

        void OnEvent(const xlioncore::physics::contact_hit& Hit) noexcept
        {
            std::printf("[System] Contact hit a=%llX b=%llX speed=%.2f point=%.2f,%.2f,%.2f normal=%.2f,%.2f,%.2f\n", static_cast<unsigned long long>(Hit.m_A.m_Value), static_cast<unsigned long long>(Hit.m_B.m_Value)
                       , Hit.m_ApproachSpeed, static_cast<double>(Hit.m_Point.x), static_cast<double>(Hit.m_Point.y), static_cast<double>(Hit.m_Point.z), Hit.m_Normal.x, Hit.m_Normal.y, Hit.m_Normal.z);
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

using demo_contact_begin_logger = xlioncore::demo::contact_begin_logger;
XSCRIPT_REGISTER_SYSTEM(demo_contact_begin_logger)

using demo_contact_end_logger = xlioncore::demo::contact_end_logger;
XSCRIPT_REGISTER_SYSTEM(demo_contact_end_logger)

using demo_contact_hit_logger = xlioncore::demo::contact_hit_logger;
XSCRIPT_REGISTER_SYSTEM(demo_contact_hit_logger)

#endif // XLIONCORE_DEMO_SYSTEMS_H
