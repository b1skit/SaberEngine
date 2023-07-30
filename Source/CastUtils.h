// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "DebugConfiguration.h"


namespace util
{
    template<typename To, typename From>
    static inline To CheckedCast(From value)
    {
        SEAssert("Casted value is out of range of the destination type", 
            value >= std::numeric_limits<To>::min() && value <= std::numeric_limits<To>::max());

        return static_cast<To>(value);
    }
}