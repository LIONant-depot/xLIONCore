// Self-registration entry points - the same ABI Game.dll already implements
// (dependencies/xECSV2/src/xecs_plugin_api.h), so xLION.exe resolves and calls these exactly the way
// it already resolves Game.dll's own (GetModuleHandle + GetProcAddress by fixed name, cast to
// xecs_plugin_pfn_register_components/_systems), never naming xlioncore types directly. This is the
// ONLY translation unit that calls RegisterComponents/RegisterSystems for LIONCore's own types -
// both compiled into THIS DLL, so info_v<> (per-binary globals) get populated in the same binary
// every query/entity-creation call also runs in.
#include "xlioncore_physics_system.h"
#include "../transform/xlioncore_transform.h"
#include "xecs_plugin_api.h"

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterComponents(xecs::game_mgr::instance& GameMgr, xecs::plugin::token Token) noexcept
{
    GameMgr.RegisterComponents<xlioncore::transform, xlioncore::physics::rigid_body>(Token);
}

extern "C" __declspec(dllexport)
void XecsPlugin_RegisterSystems(xecs::game_mgr::instance& GameMgr) noexcept
{
    GameMgr.RegisterSystems<xlioncore::physics::system>();
}
