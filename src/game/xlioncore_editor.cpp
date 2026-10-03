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

        // ---- entities
        template<typename T> T* Get(xecs::component::entity Entity) noexcept
        {
            if (!m_Game.m_pGameMgr || !Entity.isValid()) return nullptr;
            auto& Details = m_Game.m_pGameMgr->m_ComponentMgr.getEntityDetails(Entity);
            if (!Details.m_pPool || !Details.m_pPool->m_pArchetype->getComponentBits().getBit(xecs::component::type::info_v<T>.m_BitID)) return nullptr;
            return &Details.m_pPool->getComponent<T>(Details.m_PoolIndex);
        }

        bool IsAlive(xecs::component::entity Entity) noexcept override
        {
            return m_Game.m_pGameMgr && Entity.isValid() && m_Game.m_pGameMgr->m_ComponentMgr.getEntityDetails(Entity).m_pPool != nullptr;
        }

        void* ResolveComponent(xecs::component::entity Entity, xecs::component::type::guid Type, const xecs::component::type::info*& pInfo) noexcept override
        {
            pInfo = nullptr;
            if (!m_Game.m_pGameMgr || !Entity.isValid()) return nullptr;
            auto& World = *m_Game.m_pGameMgr;
            const auto* pType = World.m_ComponentMgr.findComponentTypeInfo(Type);
            if (!pType) return nullptr;

            auto& Details = World.m_ComponentMgr.getEntityDetails(Entity);
            if (!Details.m_pPool) return nullptr;
            const auto iType = Details.m_pPool->findIndexComponentFromInfo(*pType);
            if (iType >= 0)
            {
                pInfo = pType;
                return &Details.m_pPool->m_pComponent[iType][Details.m_PoolIndex.m_Value * pType->m_Size];
            }
            // SHARE components live on a share-entity referenced by the pool family, not in the entity data pool.
            if (pType->m_TypeID == xecs::component::type::id::SHARE && Details.m_pPool->m_pMyFamily)
            {
                auto* pFamily = Details.m_pPool->m_pMyFamily;
                for (int i = 0, end = static_cast<int>(pFamily->m_ShareInfos.size()); i < end; ++i)
                {
                    if (pFamily->m_ShareInfos[i]->m_Guid.m_Value != pType->m_Guid.m_Value) continue;
                    auto& ShareDetails = World.m_ComponentMgr.getEntityDetails(pFamily->m_ShareDetails[i].m_Entity);
                    if (!ShareDetails.m_pPool) break;
                    const auto iShare = ShareDetails.m_pPool->findIndexComponentFromInfo(*pType);
                    if (iShare < 0) break;
                    pInfo = pType;
                    return &ShareDetails.m_pPool->m_pComponent[iShare][ShareDetails.m_PoolIndex.m_Value * pType->m_Size];
                }
            }
            return nullptr;
        }

        xecs::component::parent*   ParentOf(xecs::component::entity Entity) noexcept override { return Get<xecs::component::parent>(Entity); }
        xecs::component::children* ChildrenOf(xecs::component::entity Entity) noexcept override { return Get<xecs::component::children>(Entity); }
        void DeleteEntity(xecs::component::entity Entity) noexcept override { if (m_Game.m_pGameMgr) m_Game.m_pGameMgr->DeleteEntity(Entity); }

        std::vector<const xecs::component::type::info*> InfosOf(std::span<const xecs::component::type::guid> Guids) noexcept
        {
            std::vector<const xecs::component::type::info*> Infos;
            for (const auto& G : Guids) if (const auto* p = xecs::component::mgr::findComponentTypeInfo(G)) Infos.push_back(p);
            return Infos;
        }
        xecs::component::entity CreateEntity(std::span<const xecs::component::type::guid> Components) noexcept override
        {
            const auto Infos = InfosOf(Components);
            return m_Game.m_pGameMgr->getOrCreateArchetype(Infos).CreateEntity();
        }
        xecs::component::entity ChangeComponents(xecs::component::entity Entity, std::span<const xecs::component::type::guid> Add, std::span<const xecs::component::type::guid> Remove) noexcept override
        {
            const auto AddInfos = InfosOf(Add), RemoveInfos = InfosOf(Remove);
            return m_Game.m_pGameMgr->AddOrRemoveComponents(Entity, AddInfos, RemoveInfos);
        }
        xecs::component::entity AddComponents(xecs::component::entity Entity, std::span<const xecs::component::type::guid> Add) noexcept override { return ChangeComponents(Entity, Add, {}); }
        xecs::component::entity RemoveComponents(xecs::component::entity Entity, std::span<const xecs::component::type::guid> Remove) noexcept override { return ChangeComponents(Entity, {}, Remove); }
        bool ReinternShareComponent(xecs::component::entity Entity, xecs::component::type::guid Type, const void* pData) noexcept override
        {
            if (!m_Game.m_pGameMgr || !Entity.isValid()) return false;
            const auto* pInfo = xecs::component::mgr::findComponentTypeInfo(Type);
            if (!pInfo) return false;
            auto& Details = m_Game.m_pGameMgr->m_ComponentMgr.getEntityDetails(Entity);
            if (Details.m_pPool == nullptr || Details.m_pPool->m_pMyFamily == nullptr || Details.m_pPool->m_pArchetype == nullptr) return false;
            return m_Game.m_pGameMgr->ReinternShareComponent(Entity, *pInfo, static_cast<std::byte*>(const_cast<void*>(pData)));
        }
        void DataComponentsOf(xecs::component::entity Entity, std::vector<component_view>& Out) noexcept override
        {
            if (!m_Game.m_pGameMgr || !Entity.isValid()) return;
            auto& Details = m_Game.m_pGameMgr->m_ComponentMgr.getEntityDetails(Entity);
            if (!Details.m_pPool) return;
            for (const auto* pInfo : Details.m_pPool->m_pArchetype->getDataComponentInfos())
            {
                const auto iType = Details.m_pPool->findIndexComponentFromInfo(*pInfo);
                if (iType < 0) continue;
                Out.push_back({ pInfo, &Details.m_pPool->m_pComponent[iType][Details.m_PoolIndex.m_Value * pInfo->m_Size] });
            }
        }
        const xecs::component::type::info* FindComponentType(xecs::component::type::guid Type) noexcept override { return xecs::component::mgr::findComponentTypeInfo(Type); }

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
