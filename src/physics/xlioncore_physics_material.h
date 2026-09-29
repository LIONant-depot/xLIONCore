#ifndef XLIONCORE_PHYSICS_MATERIAL_H
#define XLIONCORE_PHYSICS_MATERIAL_H
#pragma once

// PhysicsMaterial - a descriptor-only resource (no compiler), like SharedComponentTemplate: the
// surface properties a collider shape hands to Box3D. Colliders reference one by guid; editing the
// asset changes every shape built from it afterwards. Listed in the asset browser through
// Plugins/xPhysicsMaterial.plugin (its TypeGUID must match type_guid_v).
#include "dependencies/xECSV2/src/xecs.h"

namespace xlioncore::physics::material
{
    inline constexpr auto type_guid_v = xresource::type_guid(xresource::guid_generator::Instance64FromString("PhysicsMaterial"));
    using ref = xresource::def_guid<type_guid_v>;

    struct descriptor : xresource_pipeline::descriptor::base
    {
        void SetupFromSource( std::string_view ) override {}
        void Validate       ( std::vector<std::string>& ) const noexcept override {}

        float   m_Friction      = 0.6f;
        float   m_Restitution   = 0.0f;
        float   m_Density       = 1000.0f;      // Not the mass authority - PhysicsDynamics::Mass is

        XPROPERTY_VDEF
        ( "PhysicsMaterial", descriptor
        , obj_member<"Friction",    &descriptor::m_Friction>
        , obj_member<"Restitution", &descriptor::m_Restitution>
        , obj_member<"Density",     &descriptor::m_Density>
        )
    };
    XPROPERTY_VREG(descriptor)

    struct factory final : xresource_pipeline::factory_base
    {
        using xresource_pipeline::factory_base::factory_base;

        std::unique_ptr<xresource_pipeline::descriptor::base> CreateDescriptor( void ) const noexcept override
        {
            return std::make_unique<descriptor>();
        }

        xresource::type_guid ResourceTypeGUID( void ) const noexcept override
        {
            return type_guid_v;
        }

        const char* ResourceTypeName( void ) const noexcept override
        {
            return "PhysicsMaterial";
        }

        const xproperty::type::object& ResourceXPropertyObject( void ) const noexcept override
        {
            return *xproperty::getObjectByType<descriptor>();
        }
    };
    namespace details { struct factory_holder { inline static factory s_Instance{}; }; }

    // Reads a material from the project library (Descriptors/PhysicsMaterial/<b0>/<b1>/<guid>.desc).
    // Returns defaults when the guid is empty or the file can't be read.
    inline descriptor Load( std::wstring_view ProjectPath, std::uint64_t Value ) noexcept
    {
        descriptor Material;
        if( Value == 0 ) return Material;

        const auto Path  = std::format( L"{}/Descriptors/PhysicsMaterial/{:02X}/{:02X}/{:016X}.desc/Descriptor.txt"
                                      , ProjectPath, Value & 0xFF, (Value >> 8) & 0xFF, Value );

        xproperty::settings::context Context;
        if( auto Err = Material.Serialize( true, Path, Context ); Err )
        {
            std::printf("[PhysicsMaterial] ERROR: could not read material %016llX - using defaults\n", static_cast<unsigned long long>(Value));
            std::fflush(stdout);
            Material = descriptor{};
        }
        return Material;
    }
}

#endif // XLIONCORE_PHYSICS_MATERIAL_H
