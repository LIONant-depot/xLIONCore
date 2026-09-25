// Self-registration entry points - the same ABI Game.dll already implements
// (dependencies/xECSV2/src/xecs_plugin_api.h), so xLION.exe resolves and calls these exactly the way
// it already resolves Game.dll's own (GetModuleHandle + GetProcAddress by fixed name, cast to
// xecs_plugin_pfn_register_components/_systems), never naming xlioncore::physics::rigid_body/system
// directly. This is the ONLY translation unit that calls RegisterComponents<rigid_body>()/
// RegisterSystems<system>() - both compiled into THIS DLL, so xecs::component::type::info_v<rigid_body>
// (a per-binary global) gets populated in the same binary every query/entity-creation call in
// xlioncore_physics_system.h also runs in. xLIONCore's own component/system set is small and known at
// its own compile time, so - unlike Game.dll, whose set of script types isn't known until the user
// writes them - this just calls RegisterComponents/RegisterSystems directly, no self-registration
// linked-list codegen needed.
#include "xlioncore_physics_system.h"
#include "xecs_plugin_api.h"

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterComponents(xecs::game_mgr::instance& GameMgr, xecs::plugin::token Token) noexcept
{
    GameMgr.RegisterComponents<xlioncore::physics::rigid_body>(Token);
}

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterSystems(xecs::game_mgr::instance& GameMgr) noexcept
{
    GameMgr.RegisterSystems<xlioncore::physics::system>();
}
