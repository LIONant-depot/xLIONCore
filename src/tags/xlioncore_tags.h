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

    // "This entity is disabled": it is out of the game. Authored state, saved with the scene (the editor's own, xecs::editor::disable_tag, is saved with the scene too and left out of the game by the scene compiler). An EXCLUSIVE tag - an entity that has one only matches the queries of the systems that name that tag, so every system (physics, the
    // render, the scripts of a game, ...) skips it without a line of code in any of them: disabling is adding this, enabling is removing it. Nothing is destroyed or lost, the entity and all its
    // components are as they were.
    // (A physics body that already exists is not touched yet: see the note in the docs, documentation/Editors/disable_tags.md.)
    struct disable_tag
    {
        constexpr static auto typedef_v = xecs::component::type::exclusive_tag
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::disable" }
        , .m_pName = "disable"
        };

        XPROPERTY_DEF("disable", disable_tag)
    };
    XSCRIPT_REGISTER_COMPONENT(disable_tag, "Basics", 4)

    // "Do not draw this entity." A regular tag the render systems exclude (none_of<no_render_tag>) in every view: the entity goes on existing and running - physics, scripts - it is only not drawn
    // (and not picked in the viewport, where there is nothing to click). Authored state, saved with the scene; the editor has its own (xecs::editor::no_render_tag) that only its scene view honors.
    struct no_render_tag
    {
        constexpr static auto typedef_v = xecs::component::type::tag
        { .m_Guid  = xecs::component::type::guid{ "xlioncore::no_render" }
        , .m_pName = "no_render"
        };

        XPROPERTY_DEF("no_render", no_render_tag)
    };
    XSCRIPT_REGISTER_COMPONENT(no_render_tag, "Basics", 5)
}

#endif // XLIONCORE_TAGS_H
