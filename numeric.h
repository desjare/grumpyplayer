
#pragma once
#include <limits>

namespace numeric
{
    template<typename T>
    bool equals(T a, T b)
    {
      return std::nextafter(a, std::numeric_limits<T>::lowest()) <= b &&
             std::nextafter(a, std::numeric_limits<T>::max()) >= b;
    }

}
