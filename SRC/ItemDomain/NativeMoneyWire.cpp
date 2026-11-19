#include <item/NativeMoneyWire.hpp>
namespace auriga::item {
namespace {
bool Valid(const NativeMoneySnapshot& s){
    if(static_cast<unsigned>(s.status)>7 || !s.cap || s.cap>INT64_MAX || s.balance>s.cap || s.amount>s.cap || (!s.nonce)!=(!s.amount))return false;
    const bool active=s.status==NativeMoneyStatus::Ready || s.status==NativeMoneyStatus::Applied || s.status==NativeMoneyStatus::Pending;
    return !active || (s.nonce && s.revision && (s.status!=NativeMoneyStatus::Ready || s.amount<=s.balance));
}
}
std::expected<std::vector<std::byte>,NativeMoneyWireError> EncodeNativeMoney(const NativeMoneySnapshot& s){
    if(!Valid(s))return std::unexpected(NativeMoneyWireError::Invalid);
    std::vector<std::byte> out;const auto put=[&](std::uint64_t value,unsigned size){for(unsigned n=0;n<size;++n)out.push_back(std::byte((value>>(8*n))&255));};
    put(NativeMoneyHeader,1);put(NativeMoneyWireSize,4);put(1,1);put(static_cast<unsigned>(s.status),1);put(s.nonce,8);put(s.amount,8);put(s.balance,8);put(s.revision,8);put(s.cap,8);return out;
}
std::expected<NativeMoneySnapshot,NativeMoneyWireError> DecodeNativeMoney(std::span<const std::byte> bytes){
    if(bytes.size()!=NativeMoneyWireSize)return std::unexpected(NativeMoneyWireError::Truncated);unsigned at=0;
    const auto get=[&](unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(std::to_integer<unsigned>(bytes[at++]))<<(8*i);return v;};
    if(get(1)!=NativeMoneyHeader || get(4)!=NativeMoneyWireSize || get(1)!=1)return std::unexpected(NativeMoneyWireError::Invalid);
    NativeMoneySnapshot s{static_cast<NativeMoneyStatus>(get(1)),get(8),get(8),get(8),get(8),get(8)};if(!Valid(s))return std::unexpected(NativeMoneyWireError::Invalid);return s;
}
std::expected<NativeMoneyRequest,NativeMoneyWireError> DecodeNativeMoneyRequest(std::span<const std::byte> b){
    if(b.size()!=NativeMoneyRequestBodySize)return std::unexpected(NativeMoneyWireError::Truncated);
    const auto get=[&](unsigned at){std::uint64_t v=0;for(unsigned n=0;n<8;++n)v|=std::uint64_t(std::to_integer<unsigned>(b[at+n]))<<(8*n);return v;};
    NativeMoneyRequest r{std::to_integer<std::uint8_t>(b[1]),get(2),get(10)};
    if(b[0]!=std::byte{NativeMoneyRequestHeader} || r.action<1 || r.action>3 || (r.action==1?r.nonce!=0:r.nonce==0) || (r.action==3?r.amount!=0:!r.amount || r.amount>INT64_MAX))return std::unexpected(NativeMoneyWireError::Invalid);return r;
}
}
