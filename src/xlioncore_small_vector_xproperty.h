#ifndef XLIONCORE_SMALL_VECTOR_XPROPERTY_H
#define XLIONCORE_SMALL_VECTOR_XPROPERTY_H
#pragma once

// xproperty list support for xcontainer::small_vector - same shape as xproperty's own std::vector
// support (xproperty/source/xcore/my_properties.h), which can't see xcontainer. Must be included
// before any XPROPERTY_DEF that reflects a small_vector member.
#include "dependencies/xECSV2/src/xecs.h"
#include "dependencies/xcontainer/source/xcontainer.h"

namespace xproperty::settings
{
    template< typename T, std::size_t N >
    struct var_type<xcontainer::small_vector<T, N>> : var_list_defaults< "small_vector", xcontainer::small_vector<T, N>, T >
    {
        using type = typename var_list_defaults< "small_vector", xcontainer::small_vector<T, N>, T >::type;
        inline constexpr static bool has_real_setSize_v = true;
        constexpr static void setSize(type& MemberVar, const std::size_t Size, context&) noexcept
        {
            MemberVar.resize(Size);
        }
    };
}

#endif // XLIONCORE_SMALL_VECTOR_XPROPERTY_H
