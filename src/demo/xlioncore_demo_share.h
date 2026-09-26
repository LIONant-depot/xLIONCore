#ifndef XLIONCORE_DEMO_SHARE_H
#define XLIONCORE_DEMO_SHARE_H
#pragma once

// Smoke/demo SHARE component for SharedComponentTemplate V1.
// Eligible for Add Component (non-internal SHARE with m_pPropertyTable) so Save-as-template
// and drag-instantiate can be exercised. Not a production game type.
#include "dependencies/xECSV2/src/xecs.h"

namespace xlioncore
{
    struct demo_share
    {
        // Stable GUID so the type identity survives renames / TU moves.
        // m_Guid must precede m_pName (declaration order of type::share).
        constexpr static auto typedef_v = xecs::component::type::share
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::demo_share" }
        , .m_pName = "DemoShare"
        };

        // SHARE keys default to HashBytes(sizeof(T)) (has_share_compute_key).
        // RegisterComponent sets m_DefaultShareKey from T{}; pool Append uses ConstructFn
        // `new(p) T`. Both leave trailing padding after `bool` uninitialized, so keys
        // diverge and family::Initialize asserts Key == ShareKeyList
        // (xecs_pool_inline.h:55) when Add Component adds a new SHARE with empty MoveData
        // (DataInOrder=nullptr -> DefaultShareKey + CreateEntity({},{})).
        // Working SHARE pattern (xecs::scene::component): ComputeShareKey over real fields.
        // Here: zero the pad so the default HashBytes path is stable (sizeof stays 20).
        float           m_TintR     = 1.0f;
        float           m_TintG     = 1.0f;
        float           m_TintB     = 1.0f;
        int             m_Layer     = 0;
        bool            m_bEnabled  = true;
        std::uint8_t    m_Pad[3]    = {};

        XPROPERTY_DEF
        ( "DemoShare", demo_share
        , obj_member<"TintR",    &demo_share::m_TintR>
        , obj_member<"TintG",    &demo_share::m_TintG>
        , obj_member<"TintB",    &demo_share::m_TintB>
        , obj_member<"Layer",    &demo_share::m_Layer>
        , obj_member<"Enabled",  &demo_share::m_bEnabled>
        )
    };
    XPROPERTY_REG(demo_share)
}

#endif // XLIONCORE_DEMO_SHARE_H