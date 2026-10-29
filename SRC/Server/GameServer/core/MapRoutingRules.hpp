#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace auriga {
// The setup packet owns a fixed-size array. Never write the first entry beyond
// it: that overwrites the following authentication-server flag on the wire.
template<class Range>
size_t CopyMapAllow(int32_t* output,int capacity,const Range& maps)
{
    if(!output||capacity<=0)return 0;
    std::fill_n(output,capacity,0);
    size_t copied=0;
    for(auto map:maps){if(copied==size_t(capacity))break;output[copied++]=map;}
    return copied;
}
}
