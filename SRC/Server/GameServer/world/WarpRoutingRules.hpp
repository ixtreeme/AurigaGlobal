#pragma once
#include <cstdint>
namespace auriga::world {
constexpr bool OwnsPrivateWarp(int privateMap,bool localMap){return privateMap>=10000&&localMap;}
// A general-channel entrance may create a private instance while the account
// retains its original public channel. That instance belongs to this core.
constexpr std::uint8_t WarpChannel(std::uint8_t account,std::uint8_t core,int privateMap,bool localMap){return OwnsPrivateWarp(privateMap,localMap)?core:account;}
}
