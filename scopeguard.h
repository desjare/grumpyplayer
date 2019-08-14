
#pragma once

namespace scopeguard 
{
    template<typename ValueHolderType, typename ValueType>
    struct SetValue
    {
        explicit SetValue(ValueHolderType& h, const ValueType initialValue, const ValueType exitValue)
        : valueHolder(h),
          value(exitValue)
        {
            valueHolder = initialValue;
        }

        ~SetValue()
        {
            valueHolder = value;
        }

        ValueHolderType& valueHolder;
        ValueType value;
    };

}
