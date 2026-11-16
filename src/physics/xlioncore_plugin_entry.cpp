// Self-registration entry points - same ABI Game.dll implements (xecs_plugin_api.h) plus
// XScript_GetComponentDisplayInfo so the editor can sort/filter inspector categories/priorities.
// Components/systems announce via XSCRIPT_REGISTER_* in their headers; this TU walks the lists.
#include "xlioncore_physics_system.h"
#include "xlioncore_physics_api.h"
#include "xlioncore_ragdoll_api.h"
#include "../transform/xlioncore_transform.h"
#include "../tags/xlioncore_tags.h"
#include "../demo/xlioncore_demo_share.h"
#include "../demo/xlioncore_demo_systems.h"
#include "xecs_plugin_api.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

using physics_system = xlioncore::physics::system;
XSCRIPT_REGISTER_SYSTEM(physics_system)

using physics_destroy_notify = xlioncore::physics::destroy_notify;
XSCRIPT_REGISTER_SYSTEM(physics_destroy_notify)

using physics_body_builder = xlioncore::physics::body_builder;
XSCRIPT_REGISTER_SYSTEM(physics_body_builder)

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterComponents(xecs::game_mgr::instance& GameMgr, xecs::plugin::token Token) noexcept
{
    for (auto* p = xscript::self_registration<xscript::component_entry>::s_pHead; p; p = p->m_pNext)
        p->m_Value.m_pRegisterFn(GameMgr, Token);
}

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterSystems(xecs::game_mgr::instance& GameMgr) noexcept
{
    // Same cross-module BitID lesson as LIONRender: Lock may have assigned BitIDs onto a different
    // info_v* than this DLL's own copies (ptrEq==0). Physics Search uses raw info_v<T>.m_BitID -
    // sync from the registry by GUID before any system is created/queries.
    GameMgr.m_ComponentMgr.LockComponentTypes();
    xecs::component::mgr::SyncLocalBitIDs
        < xlioncore::transform
        , xlioncore::render_transform
        , xlioncore::physics::physics_body_properties
        , xlioncore::physics::physics_collider_box
        , xlioncore::physics::physics_collider_sphere
        , xlioncore::physics::physics_collider_capsule
        , xlioncore::physics::physics_collider_cylinder
        , xlioncore::physics::physics_dynamics
        , xlioncore::physics::physics_body
        , xlioncore::demo_share
        >();

    // The events of the physics are registered before any system (a system that listens to one finds it there).
    GameMgr.RegisterGlobalEvents<xlioncore::physics::sensor_begin_event, xlioncore::physics::sensor_end_event, xlioncore::physics::contact_begin_event, xlioncore::physics::contact_end_event, xlioncore::physics::contact_hit_event>();

    for (auto* p = xscript::self_registration<xscript::system_entry>::s_pHead; p; p = p->m_pNext)
        p->m_Value.m_pRegisterFn(GameMgr);
}

extern "C" __declspec(dllexport)
void XecsPlugin_Unregister(xecs::plugin::token /*Token*/) noexcept
{
}

extern "C" __declspec(dllexport)
void XScript_GetComponentDisplayInfo(xscript::pfn_component_display_visitor pVisitor, void* pUserData) noexcept
{
    for (auto* p = xscript::self_registration<xscript::component_entry>::s_pHead; p; p = p->m_pNext)
        pVisitor(pUserData, p->m_Value.m_Guid, p->m_Value.m_pName, p->m_Value.m_pCategory, p->m_Value.m_Priority);

    // The hierarchy is xECS's own (it registers parent and children itself), so it announces itself here: with the Transform it belongs to, in the Basics.
    const auto& Parent   = xecs::component::type::info_v<xecs::component::parent>;
    const auto& Children = xecs::component::type::info_v<xecs::component::children>;
    pVisitor(pUserData, Parent.m_Guid.m_Value,   Parent.m_pName,   "Basics", 2);
    pVisitor(pUserData, Children.m_Guid.m_Value, Children.m_pName, "Basics", 3);

    // The state of the entity in the editor (the Level Tree's power and eye): in the Basics too, after the runtime ones.
    const auto& EditorDisable  = xecs::component::type::info_v<xecs::editor::disable_tag>;
    const auto& EditorNoRender = xecs::component::type::info_v<xecs::editor::no_render_tag>;
    pVisitor(pUserData, EditorDisable.m_Guid.m_Value,  EditorDisable.m_pName,  "Basics", 6);
    pVisitor(pUserData, EditorNoRender.m_Guid.m_Value, EditorNoRender.m_pName, "Basics", 7);
}

namespace xlioncore::physics
{
    bool TeleportDynamicBody
    ( xecs::game_mgr::instance&  GameMgr
    , xecs::component::entity    Entity
    , const xmath::fvec3&        Position
    , const xmath::fquat&        Rotation
    ) noexcept
    {
        auto* pGame   = game::From(GameMgr);
        auto* pSystem = pGame ? pGame->m_pPhysics : nullptr;
        if (!pSystem) return false;

        auto* pBody = GetComponentPtr<physics_body>(GameMgr, Entity);
        if (!pBody || pBody->m_CachedBodyType != b3_dynamicBody) return false;

        pSystem->TeleportBody(*pBody, Position, Rotation);
        return true;
    }

    ragdoll* ActivateRagdoll( xecs::game_mgr::instance& GameMgr, const xanimation::body_graph& Graph
                            , std::span<const xanimation::rigid> RestModel, std::span<const xanimation::rigid> BonePrev, std::span<const xanimation::rigid> BoneCur
                            , float Dt, std::span<const std::pair<int, int>> Filter, const ragdoll_params& Params, std::string* pNote ) noexcept
    {
        auto* pGame   = game::From(GameMgr);
        auto* pSystem = pGame ? pGame->m_pPhysics : nullptr;
        if (!pSystem || Dt <= 0) return nullptr;
        auto p = std::make_unique<ragdoll>();
        p->m_Graph = Graph;
        p->m_SubSteps = std::max(1, Params.m_SubSteps);
        xanimation::ragdoll_runtime_settings Set;
        Set.m_Box.m_PassiveStiffness = Params.m_PassiveStiffness; Set.m_Box.m_FrictionFactor = Params.m_FrictionFactor; Set.m_Box.m_JointHertz = Params.m_JointHertz;
        Set.m_Size = Params.m_Size;
        p->m_Runtime = xanimation::ActivateRagdoll(pSystem->m_Backend.World(), p->m_Graph, RestModel, BonePrev, BoneCur, Dt, Filter, Set);
        if (pNote) *pNote = p->m_Runtime.m_Note;
        pSystem->m_Ragdolls.push_back(std::move(p));
        pSystem->UpdateSubSteps();
        return pSystem->m_Ragdolls.back().get();
    }

    void DeactivateRagdoll( xecs::game_mgr::instance& GameMgr, ragdoll* pRagdoll ) noexcept
    {
        auto* pGame   = game::From(GameMgr);
        auto* pSystem = pGame ? pGame->m_pPhysics : nullptr;
        if (!pSystem || !pRagdoll) return;
        auto& V = pSystem->m_Ragdolls;
        for (auto It = V.begin(); It != V.end(); ++It) if (It->get() == pRagdoll) { xanimation::DeactivateRagdoll((*It)->m_Runtime); V.erase(It); break; }
        pSystem->UpdateSubSteps();
    }

    void PoseRagdoll( const ragdoll& R, const xanimation::skeleton_view& Skel, float Alpha, std::span<const xanimation::rigid> Animated, std::span<xanimation::rigid> Out ) noexcept
    {
        xanimation::PoseRagdollBones(R.m_Runtime, Skel, std::clamp(Alpha, 0.0f, 1.0f), Animated, Out);
    }

    void RagdollStats( const ragdoll& R, ragdoll_stats& Out ) noexcept
    {
        Out = {};
        Out.m_MaxAnchor = xanimation::MeasureJoints(R.m_Graph, R.m_Runtime.m_Cur, {}).m_MaxAnchor;
        double MY = 0;
        for (auto Id : R.m_Runtime.m_Box.m_Bodies)
        {
            if (!b3Body_IsValid(Id)) continue;
            const auto D = b3Body_GetMassData(Id);
            const auto V = xanimation::FromB3(b3Body_GetLinearVelocity(Id)), Wl = xanimation::FromB3(b3Body_GetRotation(Id)).ConjugateCopy() * xanimation::FromB3(b3Body_GetAngularVelocity(Id));
            const auto& I = D.inertia;
            const xmath::fvec3 IW(I.cx.x * Wl.m_X + I.cy.x * Wl.m_Y + I.cz.x * Wl.m_Z, I.cx.y * Wl.m_X + I.cy.y * Wl.m_Y + I.cz.y * Wl.m_Z, I.cx.z * Wl.m_X + I.cy.z * Wl.m_Y + I.cz.z * Wl.m_Z);
            Out.m_KE += 0.5f * D.mass * V.Dot(V) + 0.5f * Wl.Dot(IW);
            Out.m_Mass += D.mass; MY += double(D.mass) * double(b3Body_GetWorldCenter(Id).y);
            Out.m_MaxSpeed = std::max(Out.m_MaxSpeed, V.Length());
            Out.m_nAwake += b3Body_IsAwake(Id) ? 1 : 0; ++Out.m_nBodies;
        }
        Out.m_ComY = Out.m_Mass > 0 ? float(MY / Out.m_Mass) : 0.0f;
    }

    xanimation::rigid RagdollRootBody( const ragdoll& R, float Alpha ) noexcept
    {
        if (R.m_Runtime.m_Cur.empty()) return {};
        const auto& A = R.m_Runtime.m_Prev[0]; const auto& B = R.m_Runtime.m_Cur[0];
        const float t = std::clamp(Alpha, 0.0f, 1.0f);
        return { A.m_P + (B.m_P - A.m_P) * t, xmath::fquat::SlerpAccurate(A.m_Q, B.m_Q, t).NormalizeSafeCopy() };
    }

    // The editor's world (R5 Drop): a Box3D world with a floor, its ragdolls made and captured exactly as the game's (ActivateRagdoll above)
    struct ragdoll_world
    {
        b3WorldId                               m_World;
        std::vector<std::unique_ptr<ragdoll>>   m_Ragdolls;
    };

    ragdoll_world* CreateRagdollWorld( float FloorY, float Gravity ) noexcept
    {
        auto p = new ragdoll_world;
        b3WorldDef WD = b3DefaultWorldDef(); WD.gravity = { 0, -Gravity, 0 };
        p->m_World = b3CreateWorld(&WD);
        b3BodyDef FD = b3DefaultBodyDef(); FD.position = { 0, FloorY - 0.5f, 0 };
        const b3BodyId Floor = b3CreateBody(p->m_World, &FD);
        b3ShapeDef SD = b3DefaultShapeDef(); const auto Box = b3MakeBoxHull(500, 0.5f, 500);
        b3CreateHullShape(Floor, &SD, &Box.base);
        return p;
    }

    void DestroyRagdollWorld( ragdoll_world* pWorld ) noexcept
    {
        if (!pWorld) return;
        for (auto& p : pWorld->m_Ragdolls) xanimation::DeactivateRagdoll(p->m_Runtime);
        b3DestroyWorld(pWorld->m_World);
        delete pWorld;
    }

    ragdoll* ActivateRagdoll( ragdoll_world& World, const xanimation::body_graph& Graph
                            , std::span<const xanimation::rigid> RestModel, std::span<const xanimation::rigid> BonePrev, std::span<const xanimation::rigid> BoneCur
                            , float Dt, std::span<const std::pair<int, int>> Filter, const ragdoll_params& Params, std::string* pNote ) noexcept
    {
        if (Dt <= 0) return nullptr;
        auto p = std::make_unique<ragdoll>();
        p->m_Graph = Graph;
        p->m_SubSteps = std::max(1, Params.m_SubSteps);
        xanimation::ragdoll_runtime_settings Set;
        Set.m_Box.m_PassiveStiffness = Params.m_PassiveStiffness; Set.m_Box.m_FrictionFactor = Params.m_FrictionFactor; Set.m_Box.m_JointHertz = Params.m_JointHertz;
        Set.m_Size = Params.m_Size;
        p->m_Runtime = xanimation::ActivateRagdoll(World.m_World, p->m_Graph, RestModel, BonePrev, BoneCur, Dt, Filter, Set);
        if (pNote) *pNote = p->m_Runtime.m_Note;
        World.m_Ragdolls.push_back(std::move(p));
        return World.m_Ragdolls.back().get();
    }

    void StepRagdollWorld( ragdoll_world& World, float Dt ) noexcept
    {
        int n = 4;      // the game world's rule (system::UpdateSubSteps): max(4, the active ragdolls' sub-steps)
        for (const auto& p : World.m_Ragdolls) n = std::max(n, p->m_SubSteps);
        b3World_Step(World.m_World, Dt, n);
        for (auto& p : World.m_Ragdolls) xanimation::CaptureRagdoll(p->m_Runtime);
    }
}
