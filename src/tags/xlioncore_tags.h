#ifndef XLIONCORE_TAGS_H
#define XLIONCORE_TAGS_H
#pragma once

// Cross-cutting, zero-property marker components - not owned by physics, rendering, or any one
// system, so they live here rather than under one system's own folder (direct user note: "static
// is going to influence many things like rendering... so is not about physics is about knowing
// that is truly static"). The Inspector renders a zero-property component as a compact "[name][x]"
// chip instead of a foldout section (xscene_panel_entity_properties.h) - there are no properties to
// show, so a full header+body row is wasted space.
#include "dependencies/xECSV2/src/xecs.h"
#include "plugins/xscript_module.plugin/source/Runtime/xscript_registration.h"

namespace xlioncore
{
    // "This entity never moves." Physics resolves body type from its presence instead of a boolean
    // flag (xlioncore_physics_system.h::ResolveBodyType) - any other system that cares whether
    // something is truly static (occlusion culling, navmesh baking, static batching, ...) can query
    // the same tag directly, without depending on physics at all.
    struct static_tag
    {
        // Real xecs::component::type::tag (id::TAG, max_size_v=1) - zero per-entity storage, not
        // just a data component with no fields. Not exclusive_tag: nothing about being static
        // conflicts with holding other tags later.
        constexpr static auto typedef_v = xecs::component::type::tag
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::static_tag" }
        , .m_pName = "static"
        };

        XPROPERTY_DEF("static", static_tag)
    };
    // Basics category, same as Transform - whether an entity moves at all is as fundamental as its
    // pose, not a physics implementation detail. Priority 1 sorts it right after Transform (0).
    XSCRIPT_REGISTER_COMPONENT(static_tag, "Basics", 1)
}

#endif // XLIONCORE_TAGS_H
