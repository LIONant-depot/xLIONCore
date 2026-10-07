#ifndef XLIONCORE_EDITOR_H
#define XLIONCORE_EDITOR_H
#pragma once

// xECSEditor: how the editor talks to the ECS of ONE copy of the core.
//
// xECS is mostly header-only, so code that includes its headers (the editor in xLION.exe, a Game.dll) runs against the registry of whichever LIONCore.dll THAT BINARY imported. To have several copies of
// the core in one process, each with its own registry (one for each open Level), the editor must not run xECS code itself: it asks a copy of the core for an xECSEditor, a pure virtual interface whose
// functions run inside that copy, and only calls those. The editor does not link LIONCore.dll for this: it looks up the factory in the copy it was given (CreateEditorName, GetProcAddress on that module).
//
// Every function is noexcept and nothing here throws across the boundary. Types that cross it (xecs::game_mgr::instance, game_time, xerr, std::string) are the same header in the editor and in the core:
// everything is /MD with one CRT, so they can be created in one and freed in the other, and the interface says who frees what (Release).
//
// This is grown a group at a time (see documentation/Editors/ecs_link_gate.md): the first group is the world's life (create, run, step, stop, snapshot) and the engine functions the editor used to import.
// Native() is the way out for what has not been moved behind the interface yet: it is the world of THIS copy, and the code that uses it is what the two ECS gates count.
#include "dependencies/xLIONCore/src/game/xlioncore_game.h"

namespace xlioncore
{
    // One system of the world, as the editor lists it (the info points into the registering image: do not keep it past a reload).
    struct system_view
    {
        const xecs::system::type::info* m_pInfo    = nullptr;
        bool                            m_bUpdate  = true;     // false = notifier (runs on create/destroy/move events)
        bool                            m_bEnabled = true;
        int                             m_Order    = -1;       // execution order among the update systems
    };

    struct xECSEditor
    {
        // Bumped when the interface changes in a way that a binary built against another version cannot use.
        static constexpr std::uint32_t kVersion = 13;       // 13: LiveUpdatePrefab, PrefabUses (prefab plan phase 6); 12: SetPrefabSaveRedirect (prefab plan phase 5); 11: permanent ids are 64 bits (prefab plan phase 2), prefab instances are recipes (phase 3)

        virtual std::uint32_t Version() const noexcept = 0;

        // The editor owns what it was given until it calls this: the interface, the game (time) and the world are gone after it.
        virtual void Release() noexcept = 0;

        // ---- the game of this editor: the time (plain data: the multiplier, the pause, the clocks) and what the systems of the world reach through getUserData<xlioncore::game>()
        virtual game& Game() noexcept = 0;

        // ---- the life of the world
        virtual xecs::game_mgr::instance& CreateWorld() noexcept = 0;     // a new, empty world that knows the game, replacing the one there was (time keeps its multiplier and pause)
        virtual void DestroyWorld() noexcept = 0;
        virtual void AbandonWorld() noexcept = 0;                         // leaks a world whose systems crashed while registering: destroying what the crash left behind is worse
        virtual xecs::game_mgr::instance* Native() noexcept = 0;          // the world of this copy of the core (null when there is none): for what is not behind the interface yet

        // ---- the registry of this copy of the core. Game.dll and the render DLL register themselves through their own XecsPlugin_* entry points, called with Native(): they import the core they were built for.
        virtual void RegisterHostComponents() noexcept = 0;               // into the world: the editor's components (prefab instances, entity references) and the core's own
        virtual void RegisterHostSystems() noexcept = 0;                  // the systems of the core (the demo ones, the physics), which locks the component types
        virtual void UnregisterPlugin(xecs::plugin::token Token) noexcept = 0;   // every world is gone: the registry goes back to nothing (plugin reload)
        virtual void ResetRegistrations() noexcept = 0;

        // ---- one entity, by handle (a per-world value). Pointers into the pools are valid until the next structural change of the world (an entity created, deleted, a component added or removed).
        virtual bool  IsAlive(xecs::component::entity Entity) noexcept = 0;
        // The data of one component of the entity (a SHARE component through the entity that holds it), and what describes it (pInfo); null when the entity has no such component. pInfo points into
        // the registering image (this core or a Game.dll): do not keep it past a reload.
        virtual void* ResolveComponent(xecs::component::entity Entity, xecs::component::type::guid Type, const xecs::component::type::info*& pInfo) noexcept = 0;
        // Whether the entity has the component, tags included (a tag has no data: it is only a bit of the entity's archetype).
        virtual bool HasComponent(xecs::component::entity Entity, xecs::component::type::guid Type) noexcept = 0;
        // The component types of the entity, by kind (what its archetype holds: the data ones in the archetype's order, the share ones of its family, the tags). Empty when the entity is not alive.
        virtual void ComponentTypesOf(xecs::component::entity Entity, std::vector<const xecs::component::type::info*>& Data, std::vector<const xecs::component::type::info*>& Share, std::vector<const xecs::component::type::info*>& Tags) noexcept = 0;
        // Every component type registered in this copy of the core.
        virtual void ListComponentTypes(std::vector<const xecs::component::type::info*>& Out) noexcept = 0;
        // Every system of the world: the update systems in execution order, then the notifiers. Entity systems only leaves out the ones that declare no components (they do not iterate entities).
        virtual void ListSystems(std::vector<system_view>& Out, bool bEntitySystemsOnly) noexcept = 0;
        // Whether the system would run on an entity that has exactly these component types: the test the scheduler makes (update systems also look at the exclusive tags, notifiers do not).
        virtual bool SystemMatches(const system_view& System, std::span<const xecs::component::type::guid> Components) noexcept = 0;
        virtual xecs::component::parent*   ParentOf(xecs::component::entity Entity) noexcept = 0;     // null when the entity has no parent component
        virtual xecs::component::children* ChildrenOf(xecs::component::entity Entity) noexcept = 0;   // null when it has no children component
        virtual void  DeleteEntity(xecs::component::entity Entity) noexcept = 0;
        // A new entity with those components (by guid: a guid this copy of the core does not know is skipped).
        virtual xecs::component::entity CreateEntity(std::span<const xecs::component::type::guid> Components) noexcept = 0;
        // Components added to / removed from an entity: it moves to another archetype, so the handle that comes back is the entity's new one (the old handle is stale).
        virtual xecs::component::entity AddComponents(xecs::component::entity Entity, std::span<const xecs::component::type::guid> Add) noexcept = 0;
        virtual xecs::component::entity RemoveComponents(xecs::component::entity Entity, std::span<const xecs::component::type::guid> Remove) noexcept = 0;
        // Both in one move (one new handle): what an entity gains and what it loses.
        virtual xecs::component::entity ChangeComponents(xecs::component::entity Entity, std::span<const xecs::component::type::guid> Add, std::span<const xecs::component::type::guid> Remove) noexcept = 0;
        // A SHARE component's value changed (the entity moves to the family that holds the new value, which may already exist); pData is the new value, laid out as the type. False when the entity has no family.
        virtual bool ReinternShareComponent(xecs::component::entity Entity, xecs::component::type::guid Type, const void* pData) noexcept = 0;
        // The DATA components of an entity, each with its description and its data (SHARE components: ResolveComponent). Same lifetime rule as ResolveComponent.
        struct component_view { const xecs::component::type::info* m_pInfo; std::byte* m_pData; };
        virtual void DataComponentsOf(xecs::component::entity Entity, std::vector<component_view>& Out) noexcept = 0;
        // What this copy of the core knows about a component type (null: it does not know it). Points into the registering image: do not keep it past a reload.
        virtual const xecs::component::type::info* FindComponentType(xecs::component::type::guid Type) noexcept = 0;

        // ---- levels, scenes and prefabs: what the managers of the world do that builds, loads or saves entities and components (the editor only reads and marks their bookkeeping itself)
        virtual xerr LoadLevel(xecs::level::guid Level) noexcept = 0;
        virtual xerr SaveLevel(xecs::level::guid Level) noexcept = 0;
        virtual xerr ActivateLevel(xecs::level::guid Level) noexcept = 0;                  // loads every scene of the Level
        virtual xerr RequestLoadScene(xecs::scene::guid Scene) noexcept = 0;
        virtual xerr ReleaseLoadScene(xecs::scene::guid Scene) noexcept = 0;
        virtual xerr SaveScene(xecs::scene::guid Scene) noexcept = 0;
        virtual xerr SaveSceneEntity(xecs::scene::guid Scene, xecs::scene::permanent_id Id, xecs::component::entity Entity) noexcept = 0;
        virtual xecs::scene::instance& FindOrCreateScene(xecs::scene::guid Scene) noexcept = 0;
        virtual std::vector<xecs::scene::component_dependency> CollectSceneComponentDependencies(xecs::scene::guid Scene) noexcept = 0;
        virtual xerr EnsureLoadedPrefab(xecs::prefab::guid Prefab) noexcept = 0;
        virtual xerr SavePrefab(xecs::prefab::guid Prefab) noexcept = 0;
        // One writer per prefab (documentation/Editors/prefabs_plan.md 3.7): while another editor holds a prefab as its document, SavePrefab does not write the file, it hands the saved state to that editor
        // (xecs::prefab::mgr::save_redirect). The redirect belongs to the caller, who keeps it alive and sets null before it goes; it stays on the worlds this copy of the core makes from here on.
        virtual void SetPrefabSaveRedirect(xecs::prefab::mgr::save_redirect* pRedirect) noexcept = 0;
        // The prefab changed on disk (another editor saved it): this world forgets the template it holds, so that the next instance reads the file. Instances already made are untouched.
        virtual void DropPrefabTemplate(xecs::prefab::guid Prefab) noexcept = 0;
        // Live update (documentation/Editors/prefabs_plan.md 3.6, phase 6): the instances in this world's scenes made of the prefab (it is theirs, or one they nest) are spawned again
        // from it with their recipes - same ids, overrides kept, nothing written or marked dirty. bFromFile: the prefab changed on disk (the template is read again); otherwise the
        // template in memory is the new one (the undo of an Apply). Never on a world that is playing. Returns how many instances were spawned again.
        virtual int LiveUpdatePrefab(xecs::prefab::guid Prefab, bool bFromFile) noexcept = 0;
        // True when an instance of Prefab is made of Used: it is Used, or nests it however deep (what a prefab cannot hold: itself).
        virtual bool PrefabUses(xecs::prefab::guid Prefab, xecs::prefab::guid Used) noexcept = 0;
        // pOutside (optional): the references the group's members held to entities outside it (the prefab keeps them null; the instance the group becomes keeps them as overrides).
        // pMemberIds (optional): each source entity's id in the prefab (keyed by the source's entity value): what the instance the group becomes calls it.
        virtual xecs::prefab::guid CreatePrefabFromEntity(xecs::component::entity Source, xecs::prefab::guid Prefab, std::vector<xecs::prefab::outside_reference>* pOutside, std::unordered_map<std::uint64_t, std::uint64_t>* pMemberIds) noexcept = 0;
        virtual xecs::component::entity CreatePrefabInstance(xecs::component::entity PrefabEntity, bool bRemoveRoot) noexcept = 0;     // one instance of a resident prefab
        virtual void UpdateStructuralChanges() noexcept = 0;                               // the entities created/deleted/moved this frame take effect now (normally once a frame inside Run)
        virtual void EnableBuilders(bool bEnable) noexcept = 0;

        // ---- the prefab and scene model on top of entities
        // A new entity with the same data components as Source (and a parent component when asked): their data copied, except the entity itself, the parent and the children (a children component starts empty).
        virtual xecs::component::entity CloneEntity(xecs::component::entity Source, bool bWithParent) noexcept = 0;
        // A prefab instance is a recipe (documentation/Editors/prefabs_plan.md, phase 3): its members are spawned from the prefab, with ids derived
        // from the instance's id and their address (xecs::editor::member_address), and the scene knows them (instance::m_InstanceMembers).
        // The template entity of a prefab at an address (what a member of an instance starts from); invalid when the prefab has no such member.
        virtual xecs::component::entity ResolvePrefabMember(xecs::prefab::guid Prefab, std::span<const std::uint64_t> Address) noexcept = 0;
        // The same member as an instance of the prefab starts from it: the nested instances' recipes of the prefab applied (what "revert to the prefab's value" goes back to). Invalid when there is none.
        virtual xecs::component::entity ResolveBakedPrefabMember(xecs::prefab::guid Prefab, std::span<const std::uint64_t> Address) noexcept = 0;
        // Every member address of a prefab (its own and its nested instances' members), the root's (empty) first.
        virtual void PrefabMemberAddresses(xecs::prefab::guid Prefab, std::vector<xecs::editor::member_address>& Out) noexcept = 0;
        // A new instance in the scene under Id (and under Parent when it is valid): its root and its members registered. Invalid when it could not be made.
        virtual xecs::component::entity InstantiatePrefabInScene(xecs::scene::instance& Scene, xecs::prefab::guid Prefab, xecs::scene::permanent_id Id, xecs::component::entity Parent) noexcept = 0;
        // The instance's recipe refreshed from what its members are now (before it is cloned into a prefab, or its members are respawned).
        virtual void RefreshPrefabRecipe(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept = 0;
        // The members the instance should have and does not (its recipe's removals aside) are spawned; how many.
        virtual int SpawnMissingPrefabMembers(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept = 0;
        // Every entity of the scene whose parent's children list does not hold it joins that list (after entities were restored one by one).
        virtual void LinkSceneChildren(xecs::scene::instance& Scene) noexcept = 0;
        // The overrides of the instance on the entity itself (its members' need the scene: SpawnMissingPrefabMembers, a load).
        virtual void ApplyPrefabInstancePropertyOverrides(xecs::component::entity Entity) noexcept = 0;
        // The recipe's overrides on its live members (after members were restored one by one).
        virtual void ApplyPrefabRecipeToMembers(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept = 0;
        // What the instance does differently becomes its prefab's, and the prefab is saved (Unity's Apply).
        virtual xerr ApplyInstanceOverridesToPrefab(xecs::scene::instance& Scene, xecs::scene::permanent_id Root) noexcept = 0;
        virtual xerr LoadSceneEntity(xecs::scene::instance& Scene, xecs::scene::permanent_id Id) noexcept = 0;           // one entity of a scene, from its file (the undo of a delete)
        // The references to other entities of a loaded entity, resolved with what Resolve says for each encoded reference.
        virtual void RemapLoadedEntityReferences(xecs::component::entity Entity, const std::function<xecs::component::entity(std::int64_t)>& Resolve) noexcept = 0;

        // ---- every live entity of the world, by what it is: the Level tree's Runtime folder (entities spawned while the game runs, the prefab templates, the entities that hold share components)
        enum runtime_kind : int { SPAWNED, PREFAB, SHARE, KIND_COUNT };
        struct runtime_entity { xecs::component::entity m_Entity; std::string m_Components; };
        virtual void CountRuntimeEntities(std::array<int, KIND_COUNT>& Counts) noexcept = 0;
        virtual void ListRuntimeEntities(runtime_kind Kind, std::vector<runtime_entity>& Out) noexcept = 0;     // the live ones (no zombies), with the names of their data components

        // ---- running it
        virtual void Run() noexcept = 0;                                  // one frame: the time moves by the real time since the last call, then the systems run
        virtual void StepOnce() noexcept = 0;                             // exactly one fixed step (1/60 s)
        virtual void RunSystems() noexcept = 0;                           // the systems once, without moving the time (the headless editor, which has its own clock)
        virtual void StopPlay() noexcept = 0;                             // the world goes back to what it was when it started to run (Stop)

        // ---- the snapshot of the whole game state (Play's reload bridge): binary or text, to or from a file
        virtual xerr SerializeGameState(const char* pPath, bool bRead, bool bBinary) noexcept = 0;

        // ---- physics
        virtual bool TeleportDynamicBody(xecs::component::entity Entity, const xmath::fvec3& Position, const xmath::fquat& Rotation) noexcept = 0;
    };

    // The guid of a component type: a compile-time constant of the type, the same in every binary (the bit id is what is per binary, and is never needed outside the core).
    template<typename T_COMPONENT>
    inline xecs::component::type::guid GuidOf() noexcept { return xecs::component::type::info_v<T_COMPONENT>.m_Guid; }

    // A typed view of ResolveComponent for the types whose guid is the compile-time constant of the type (every component: the same in every binary, only the bit id is per binary).
    template<typename T_COMPONENT>
    inline T_COMPONENT* ComponentOf( xECSEditor& Ecs, xecs::component::entity Entity ) noexcept
    {
        const xecs::component::type::info* pInfo = nullptr;
        return static_cast<T_COMPONENT*>(Ecs.ResolveComponent(Entity, xecs::component::type::info_v<T_COMPONENT>.m_Guid, pInfo));
    }

    // The same with types: the guid of a component type is the compile-time constant of the type (the same in every binary), only the bit id is per binary.
    template<typename... T_COMPONENTS>
    inline xecs::component::entity CreateEntityOf( xECSEditor& Ecs ) noexcept
    {
        const std::array<xecs::component::type::guid, sizeof...(T_COMPONENTS)> Guids{ xecs::component::type::info_v<T_COMPONENTS>.m_Guid... };
        return Ecs.CreateEntity(Guids);
    }
    template<typename... T_COMPONENTS>
    inline xecs::component::entity AddComponentsOf( xECSEditor& Ecs, xecs::component::entity Entity ) noexcept
    {
        const std::array<xecs::component::type::guid, sizeof...(T_COMPONENTS)> Guids{ xecs::component::type::info_v<T_COMPONENTS>.m_Guid... };
        return Ecs.AddComponents(Entity, Guids);
    }

    // The editor of the game a world belongs to, for a world that an editor made (every world of the Level editors).
    inline xECSEditor& Ecs( xecs::game_mgr::instance& World ) noexcept;

    // The factory every copy of the core exports (extern "C"): a new editor for a new game, owned by the caller (Release).
    constexpr const char* kCreateEditorName = "XLionCore_CreateEditor";
    using pfn_create_editor = xECSEditor* (*)() noexcept;

    // The editor of the game a world belongs to (null for a world that no editor made, e.g. one only used to register components).
    inline xECSEditor* EditorOf( xecs::game_mgr::instance& World ) noexcept
    {
        auto* pGame = game::From(World);
        return pGame ? pGame->m_pEditor : nullptr;
    }

    inline xECSEditor& Ecs( xecs::game_mgr::instance& World ) noexcept { return *EditorOf(World); }
}

#endif // XLIONCORE_EDITOR_H
