// The editor interface of this copy of the core (see xlioncore_editor.h): compiled into LIONCore.dll, so everything it does runs against THIS copy's registry.
#include "xlioncore_editor.h"
#include "../physics/xlioncore_physics_api.h"

// The entry points of this module that register what it defines (xlioncore_plugin_entry.cpp): the editor calls them through the interface, so the types land in THIS copy's registry.
extern "C" void XecsPlugin_RegisterComponents(xecs::game_mgr::instance& GameMgr, xecs::plugin::token Token) noexcept;
extern "C" void XecsPlugin_RegisterSystems(xecs::game_mgr::instance& GameMgr) noexcept;

namespace
{
    struct editor_impl final : xlioncore::xECSEditor
    {
        xlioncore::game m_Game;

        editor_impl() noexcept { m_Game.m_pEditor = this; }

        std::uint32_t Version() const noexcept override { return kVersion; }
        void          Release() noexcept override { delete this; }
        xlioncore::game& Game() noexcept override { return m_Game; }

        xecs::game_mgr::instance& CreateWorld() noexcept override { return m_Game.CreateWorld(); }
        void DestroyWorld() noexcept override { m_Game.DestroyWorld(); }
        void AbandonWorld() noexcept override { m_Game.AbandonWorld(); }
        xecs::game_mgr::instance* Native() noexcept override { return m_Game.m_pGameMgr.get(); }

        void RegisterHostComponents() noexcept override
        {
            auto& World = *m_Game.m_pGameMgr;
            World.RegisterComponents<xecs::editor::prefab_instance, xecs::component::entity_reference>();
            XecsPlugin_RegisterComponents(World, xecs::plugin::host_v);
        }
        void RegisterHostSystems() noexcept override { XecsPlugin_RegisterSystems(*m_Game.m_pGameMgr); }
        void UnregisterPlugin(xecs::plugin::token Token) noexcept override { xecs::component::mgr::UnregisterPlugin(Token); }
        void ResetRegistrations() noexcept override { xecs::component::mgr::resetRegistrations(); }

        void Run() noexcept override { m_Game.Run(); }
        void StepOnce() noexcept override { m_Game.StepOnce(); }
        void RunSystems() noexcept override { if (m_Game.m_pGameMgr) m_Game.m_pGameMgr->Run(); }
        void StopPlay() noexcept override { if (m_Game.m_pGameMgr) m_Game.m_pGameMgr->Stop(); }

        xerr SerializeGameState(const char* pPath, bool bRead, bool bBinary) noexcept override
        {
            return m_Game.m_pGameMgr->SerializeGameState(pPath, bRead, bBinary);
        }

        bool TeleportDynamicBody(xecs::component::entity Entity, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept override
        {
            return m_Game.m_pGameMgr && xlioncore::physics::TeleportDynamicBody(*m_Game.m_pGameMgr, Entity, Position, Rotation);
        }
    };
}

extern "C" __declspec(dllexport) xlioncore::xECSEditor* XLionCore_CreateEditor() noexcept
{
    return new editor_impl;
}
