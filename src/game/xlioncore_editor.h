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
    struct xECSEditor
    {
        // Bumped when the interface changes in a way that a binary built against another version cannot use.
        static constexpr std::uint32_t kVersion = 5;

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
