// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "../Assert.h"


namespace util
{
    template<typename To, typename From>
    inline To CheckedCast(From value)
    {
        const To result = static_cast<To>(value);
        SEAssert(static_cast<From>(result) == value,
            "Casted value is out of range of the destination type");

        return result;
    }
}