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

        xecs::prefab::mgr::save_redirect* m_pPrefabRedirect = nullptr;      // set on every world this copy makes (a Play rebuilds the world, the redirect stays)

        xecs::game_mgr::instance& CreateWorld() noexcept override
        {
            auto& World = m_Game.CreateWorld();
            World.m_PrefabMgr.m_pRedirect = m_pPrefabRedirect;
            return World;
        }
        void DropPrefabTemplate(xecs::prefab::guid Prefab) noexcept override { if (m_Game.m_pGameMgr) m_Game.m_pGameMgr->m_PrefabMgr.DropTemplate(Prefab); }
        void SetPrefabSaveRedirect(xecs::prefab::mgr::save_redirect* pRedirect) noexcept override
        {
            m_pPrefabRedirect = pRedirect;
            if (m_Game.m_pGameMgr) m_Game.m_pGameMgr->m_PrefabMgr.m_pRedirect = pRedirect;
        }
        void DestroyWorld() noexcept override { m_Game.DestroyWorld(); }
        void AbandonWorld() noexcept override { m_Game.AbandonWorld(); }
        xecs::game_mgr::instance* Native() noexcept override { return m_Game.m_pGameMgr.get(); }

        void RegisterHostComponents() noexcept override
        {
            auto& World = *m_Game.m_pGameMgr;
            World.RegisterComponents<xecs::editor::prefab_instance, xecs::component::entity_reference, xecs::editor::disable_tag, xecs::editor::no_render_tag>();
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

        bool HasComponent(xecs::component::entity Entity, xecs::component::type::guid Type) noexcept override
        {
            if (!m_Game.m_pGameMgr || !Entity.isValid()) return false;
            const auto* pInfo = xecs::component::mgr::findComponentTypeInfo(Type);
            if (!pInfo) return false;
            auto& Details = m_Game.m_pGameMgr->m_ComponentMgr.getEntityDetails(Entity);
            return Details.m_pPool && Details.m_pPool->m_pArchetype->getComponentBits().getBit(pInfo->m_BitID);
        }
        void ComponentTypesOf(xecs::component::entity Entity, std::vector<const xecs::component::type::info*>& Data, std::vector<const xecs::component::type::info*>& Share, std::vector<const xecs::component::type::info*>& Tags) noexcept override
        {
            Data.clear(); Share.clear(); Tags.clear();
            if (!m_Game.m_pGameMgr || !Entity.isValid()) return;
            auto& Details = m_Game.m_pGameMgr->m_ComponentMgr.getEntityDetails(Entity);
            if (!Details.m_pPool || !Details.m_pPool->m_pArchetype) return;
            auto* pArchetype = Details.m_pPool->m_pArchetype;
            for (auto p : pArchetype->getDataComponentInfos()) Data.push_back(p);
            if (Details.m_pPool->m_pMyFamily)
            {
                for (auto* p : Details.m_pPool->m_pMyFamily->m_ShareInfos)
                    if (p && std::find(Share.begin(), Share.end(), p) == Share.end()) Share.push_back(p);
            }
            else for (auto p : pArchetype->getShareComponentInfos()) Share.push_back(p);
            pArchetype->AppendTagComponentInfos(Tags);
        }
        void ListComponentTypes(std::vector<const xecs::component::type::info*>& Out) noexcept override
        {
            Out.clear();
            for (auto& Pair : xecs::component::mgr::s_Registry.m_ComponentInfoMap) Out.push_back(Pair.second);
        }
        void ListSystems(std::vector<xlioncore::system_view>& Out, bool bEntitySystemsOnly) noexcept override
        {
            Out.clear();
            if (!m_Game.m_pGameMgr) return;
            auto& Mgr  = m_Game.m_pGameMgr->m_SystemMgr;
            auto  Rows = Mgr.GetUpdateSystemRows();   // index-aligned with m_UpdaterSystems
            for (std::size_t i = 0; i < Mgr.m_UpdaterSystems.size(); ++i)
            {
                auto* pInfo = Mgr.m_UpdaterSystems[i].first;
                if (bEntitySystemsOnly && pInfo->m_Access.empty()) continue;
                Out.push_back({ pInfo, true, i < Rows.size() ? Rows[i].m_bEnabled : true, static_cast<int>(i) });
            }
            for (auto& E : Mgr.m_NotifierSystems)
            {
                if (bEntitySystemsOnly && E.first->m_Access.empty()) continue;
                Out.push_back({ E.first, false, true, -1 });
            }
        }
        bool SystemMatches(const xlioncore::system_view& System, std::span<const xecs::component::type::guid> Components) noexcept override
        {
            xecs::tools::bits Bits;
            for (const auto& G : Components) if (const auto* p = xecs::component::mgr::findComponentTypeInfo(G)) Bits.setBit(p->m_BitID);
            if (System.m_bUpdate)
            {
                xecs::tools::bits Exclusive;
                Exclusive.setupAnd(Bits, xecs::component::mgr::s_Registry.m_ExclusiveTagsBits);
                return System.m_pInfo->m_Query.Compare(Bits, Exclusive);
            }
            return System.m_pInfo->m_Query.Compare(Bits);
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

        xecs::game_mgr::instance& W() noexcept { return *m_Game.m_pGameMgr; }
        xerr LoadLevel(xecs::level::guid Level) noexcept override { return W().m_LevelMgr.Load(Level); }
        xerr SaveLevel(xecs::level::guid Level) noexcept override { return W().m_LevelMgr.Save(Level); }
        xerr ActivateLevel(xecs::level::guid Level) noexcept override { return W().m_LevelMgr.Activate(Level); }
        xerr RequestLoadScene(xecs::scene::guid Scene) noexcept override { return W().m_SceneMgr.RequestLoad(Scene); }
        xerr ReleaseLoadScene(xecs::scene::guid Scene) noexcept override { return W().m_SceneMgr.ReleaseLoad(Scene); }
        xerr SaveScene(xecs::scene::guid Scene) noexcept override { return W().m_SceneMgr.SaveScene(Scene); }
        xerr SaveSceneEntity(xecs::scene::guid Scene, xecs::scene::permanent_id Id, xecs::component::entity Entity) noexcept override { return W().m_SceneMgr.SaveEntity(Scene, Id, Entity); }
        xecs::scene::instance& FindOrCreateScene(xecs::scene::guid Scene) noexcept override { return W().m_SceneMgr.FindOrCreate(Scene); }
        std::vector<xecs::scene::component_dependency> CollectSceneComponentDependencies(xecs::scene::guid Scene) noexcept override { return W().m_SceneMgr.CollectSceneComponentDependencies(Scene); }
        xerr EnsureLoadedPrefab(xecs::prefab::guid Prefab) noexcept override { return W().m_PrefabMgr.EnsureLoaded(Prefab); }
        xerr SavePrefab(xecs::prefab::guid Prefab) noexcept override { return W().m_PrefabMgr.Save(Prefab); }
        xecs::prefab::guid CreatePrefabFromEntity(xecs::component::entity Source, xecs::prefab::guid Prefab, std::vector<xecs::prefab::outside_reference>* pOutside, std::unordered_map<std::uint64_t, std::uint64_t>* pMemberIds) noexcept override { return W().m_PrefabMgr.CreatePrefabFromEntity(Source, Prefab, pOutside, pMemberIds); }
        xecs::component::entity CreatePrefabInstance(xecs::component::entity PrefabEntity, bool bRemoveRoot) noexcept override { return W().m_PrefabMgr.CreatePrefabInstance(1, PrefabEntity, xecs::tools::empty_lambda{}, bRemoveRoot); }
        void UpdateStructuralChanges() noexcept override { if (m_Game.m_pGameMgr) W().m_ArchetypeMgr.UpdateStructuralChanges(); }
        void EnableBuilders(bool bEnable) noexcept override { if (m_Game.m_pGameMgr) W().EnableBuilders(bEnable); }

        static runtime_kind KindOf(const xecs::archetype::instance& Archetype) noexcept
        {
            runtime_kind Kind = SPAWNED;
            Archetype.getComponentBits().Foreach([&](int, const xecs::component::type::info& Info) noexcept
            {
                if      (xecs::component::type::IsComponentType<xecs::prefab::tag>(&Info))                                Kind = PREFAB;
                else if (xecs::component::type::IsComponentType<xecs::component::share_as_data_exclusive_tag>(&Info)) Kind = SHARE;
            });
            return Kind;
        }
        void CountRuntimeEntities(std::array<int, KIND_COUNT>& Counts) noexcept override
        {
            Counts = {};
            if (!m_Game.m_pGameMgr) return;
            for (auto& pArchetype : W().m_ArchetypeMgr.m_lArchetype)
            {
                auto& N = Counts[KindOf(*pArchetype)];
                for (auto pF = pArchetype->getFamilyHead(); pF; pF = pF->m_Next.get())
                    for (auto pP = &pF->m_DefaultPool; pP; pP = pP->m_Next.get())
                        N += pP->Size();
            }
        }
        void ListRuntimeEntities(runtime_kind Kind, std::vector<runtime_entity>& Out) noexcept override
        {
            if (!m_Game.m_pGameMgr) return;
            for (auto& pArchetype : W().m_ArchetypeMgr.m_lArchetype)
            {
                if (KindOf(*pArchetype) != Kind) continue;

                std::string Components;
                for (auto pInfo : pArchetype->getDataComponentInfos())
                {
                    if (xecs::component::type::IsComponentType<xecs::component::entity>(pInfo)
                     || xecs::component::type::IsComponentType<xecs::component::ref_count>(pInfo)) continue;
                    Components += Components.empty() ? pInfo->m_pName : std::format(", {}", pInfo->m_pName);
                }

                for (auto pF = pArchetype->getFamilyHead(); pF; pF = pF->m_Next.get())
                    for (auto pP = &pF->m_DefaultPool; pP; pP = pP->m_Next.get())
                        for (int i = 0, n = pP->Size(); i < n; ++i)
                        {
                            const auto E = pP->getComponent<xecs::component::entity>(xecs::pool::index{ i });
                            if (E.isZombie()) continue;
                            Out.push_back({ E, Components });
                        }
            }
        }

        xecs::component::entity CloneEntity(xecs::component::entity Source, bool bWithParent) noexcept override
        {
            auto& Details = W().m_ComponentMgr.getEntityDetails(Source);
            if (!Details.m_pPool) return {};
            auto  DataSpan = Details.m_pPool->m_pArchetype->getDataComponentInfos();

            std::vector<const xecs::component::type::info*> Infos;
            Infos.push_back(&xecs::component::type::info_v<xecs::component::entity>);
            for (auto pInfo : DataSpan)
            {
                if (xecs::component::type::IsComponentType<xecs::component::entity>(pInfo)) continue;
                Infos.push_back(pInfo);
            }
            if (bWithParent && std::find_if(Infos.begin(), Infos.end(), [](auto* p) noexcept { return xecs::component::type::IsComponentType<xecs::component::parent>(p); }) == Infos.end())
                Infos.push_back(&xecs::component::type::info_v<xecs::component::parent>);

            auto& NewArchetype = W().getOrCreateArchetype({ Infos.data(), Infos.size() });
            std::vector<const xecs::component::type::info*> DataInfos;
            for (auto pInfo : Infos)
                if (pInfo->m_TypeID != xecs::component::type::id::TAG) DataInfos.push_back(pInfo);
            std::vector<std::byte*> MoveData(DataInfos.size(), nullptr);
            const auto NewEntity = NewArchetype.CreateEntity({ DataInfos.data(), DataInfos.size() }, { MoveData.data(), MoveData.size() });

            auto& NewDetails = W().m_ComponentMgr.getEntityDetails(NewEntity);
            auto& NewPool    = *NewDetails.m_pPool;
            auto& SourceDetails = W().m_ComponentMgr.getEntityDetails(Source);            // the pools may have moved while the entity was made
            for (auto pInfo : DataSpan)
            {
                if (xecs::component::type::IsComponentType<xecs::component::entity>(pInfo)) continue;
                if (xecs::component::type::IsComponentType<xecs::component::parent>(pInfo)) continue;
                if (xecs::component::type::IsComponentType<xecs::component::children>(pInfo)) continue;
                const auto iSrc = SourceDetails.m_pPool->findIndexComponentFromInfo(*pInfo);
                const auto iDst = NewPool.findIndexComponentFromInfo(*pInfo);
                if (iSrc < 0 || iDst < 0) continue;
                auto* pSrc = &SourceDetails.m_pPool->m_pComponent[iSrc][SourceDetails.m_PoolIndex.m_Value * pInfo->m_Size];
                auto* pDst = &NewPool.m_pComponent[iDst][NewDetails.m_PoolIndex.m_Value * pInfo->m_Size];
                if (pInfo->m_pCopyFn) pInfo->m_pCopyFn(pDst, pSrc);
                else std::memcpy(pDst, pSrc, pInfo->m_Size);
            }
            return NewEntity;
        }
        xecs::component::entity ResolvePrefabMember(xecs::prefab::guid Prefab, std::span<const std::uint64_t> Address) noexcept override { return xecs::prefab::recipe::FindTemplate(W(), Prefab, Address); }
        xecs::component::entity ResolveBakedPrefabMember(xecs::prefab::guid Prefab, std::span<const std::uint64_t> Address) noexcept override { return W().m_PrefabMgr.FindBakedMember(Prefab, Address); }
        void PrefabMemberAddresses(xecs::prefab::guid Prefab, std::vector<xecs::editor::member_address>& Out) noexcept override
        {
            Out.clear();
            xecs::prefab::recipe::plan Plan;
            if (xecs::prefab::recipe::MakePlan(W(), Prefab, Plan)) return;
            for (auto& N : Plan.m_Nodes) Out.push_back(N.m_Address);
        }
        xecs::component::entity InstantiatePrefabInScene(xecs::scene::instance& Scene, xecs::prefab::guid Prefab, xecs::scene::permanent_id Id, xecs::component::entity Parent) noexcept override
        {
            return xecs::prefab::recipe::InstantiateInScene(W(), Scene, Prefab, Id, Parent);
        }
        void RefreshPrefabRecipe(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept override
        {
            xecs::prefab::recipe::RefreshRecipe(W(), Scene, Root, [&](xecs::component::entity Target, std::int64_t& Out) noexcept { return xecs::scene::details::ResolveReferenceInScene(W().m_SceneMgr, Scene, Target, Out); });
        }
        int SpawnMissingPrefabMembers(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept override { return xecs::prefab::recipe::SpawnMissingMembers(W(), Scene, Root); }
        void LinkSceneChildren(xecs::scene::instance& Scene) noexcept override { xecs::prefab::recipe::LinkChildren(W(), Scene); }
        void ApplyPrefabInstancePropertyOverrides(xecs::component::entity Entity) noexcept override { xecs::persist::details::ApplyPrefabInstancePropertyOverrides(W(), Entity); }
        void ApplyPrefabRecipeToMembers(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept override
        {
            auto It = Scene.m_LocalToRuntime.find(Root);
            if (It == Scene.m_LocalToRuntime.end()) return;
            const auto* pPI = xecs::prefab::recipe::details::LiveComponent<xecs::editor::prefab_instance>(W(), It->second);
            if (pPI == nullptr || pPI->m_Format == 0) return;
            const auto Recipe = *pPI;
            xecs::prefab::recipe::ApplyLiveOverrides(W(), Recipe, [&](const xecs::editor::member_address& A) noexcept { return xecs::prefab::recipe::details::FindMember(Scene, Root, A); });
        }
        xerr ApplyInstanceOverridesToPrefab(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept override { return xecs::prefab::recipe::ApplyToPrefab(W().m_SceneMgr, Scene, Root); }
        xerr LoadSceneEntity(xecs::scene::instance& Scene, xecs::scene::permanent_id Id) noexcept override { return xecs::scene::details::LoadEntity(W().m_SceneMgr, Scene, Id); }
        void RemapLoadedEntityReferences(xecs::component::entity Entity, const std::function<xecs::component::entity(std::int64_t)>& Resolve) noexcept override
        {
            xecs::persist::details::RemapLoadedEntityReferences(W(), Entity, [&](std::int64_t Encoded) noexcept -> xecs::component::entity { return Resolve(Encoded); });
        }

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
