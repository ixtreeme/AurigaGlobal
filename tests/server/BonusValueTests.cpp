#include "../../SRC/Server/common/bonus_value.h"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <string>
#include <type_traits>
using auriga::BonusValue;
static_assert(!std::is_convertible_v<double, BonusValue>);
static_assert(!std::is_convertible_v<BonusValue, int>);
static_assert(std::is_trivially_copyable_v<BonusValue>);
static void Check(bool value) { if (!value) throw std::runtime_error("Bonus regression failed"); }
int main() {
    const struct { const char* text; int64_t raw; const char* formatted; } valid[] {
        {"1.113",1113,"1.113"}, {"2.224",2224,"2.224"},
        {"5",5000,"5"}, {"0.001",1,"0.001"}, {"-0.001",-1,"-0.001"},
        {"+0001.113000",1113,"1.113"}, {"-.5",-500,"-0.5"},
        {"5.",5000,"5"}, {"-0",0,"0"}, {"1.100",1100,"1.1"},
        {"9223372036854775.807",INT64_MAX,"9223372036854775.807"},
        {"-9223372036854775.808",INT64_MIN,"-9223372036854775.808"}
    };
    for (const auto& v : valid) {
        BonusValue out=BonusValue::FromRaw(77);
        Check(BonusValue::TryParse(v.text,out));
        Check(out.Raw()==v.raw && out.ToString()==v.formatted);
        Check(BonusValue::DecodeRaw(out.EncodeRaw())==out);
        std::cout << "V|" << v.text << "|" << out.Raw() << "|" << out.ToString() << '\n';
    }
    const char* invalid[] {"", "+", "-", ".", "1.1131", "1,113", "1e3", "NaN", "inf",
        "1.2.3", " 1.113", "1.113 ", "9223372036854775.808", "-9223372036854775.809",
        "99999999999999999999999999999999999999999999999999999", "0.00000000001"};
    for (const char* text : invalid) {
        BonusValue out=BonusValue::FromRaw(77);
        Check(!BonusValue::TryParse(text,out));
        Check(out.Raw()==77); // failure cannot partially corrupt a destination
        std::cout << "X|" << text << '\n';
    }
    auto total=BonusValue::FromRaw(0);
    for(int i=0;i<100000;i++) Check(BonusValue::TryAdd(total,BonusValue::FromRaw(1113),total));
    Check(total.Raw()==111300000);
    for(int i=0;i<100000;i++) Check(BonusValue::TrySubtract(total,BonusValue::FromRaw(1113),total));
    Check(total.Raw()==0);
    Check(BonusValue::TryAdd(BonusValue::FromRaw(1113),BonusValue::FromRaw(2224),total));
    Check(total.ToString()=="3.337");
    Check(!BonusValue::TryAdd(BonusValue::FromRaw(INT64_MAX),BonusValue::FromRaw(1),total));
    Check(!BonusValue::TryAdd(BonusValue::FromRaw(INT64_MIN),BonusValue::FromRaw(-1),total));
    Check(!BonusValue::TrySubtract(BonusValue::FromRaw(INT64_MIN),BonusValue::FromRaw(1),total));
    Check(!BonusValue::TrySubtract(BonusValue::FromRaw(INT64_MAX),BonusValue::FromRaw(-1),total));
    Check(BonusValue::TrySubtract(BonusValue::FromRaw(INT64_MIN),BonusValue::FromRaw(INT64_MIN),total));
    Check(total.Raw()==0);
    Check(!BonusValue::TryFromWhole(INT64_MAX,total));
    Check(!BonusValue::TryFromWhole(INT64_MIN,total));
    Check(BonusValue::TryFromWhole(5,total) && total.Raw()==5000);
    for(int64_t raw=-100000;raw<=100000;raw++) {
        auto original=BonusValue::FromRaw(raw);
        BonusValue parsed;
        Check(BonusValue::TryParse(original.ToString(),parsed) && parsed==original);
        Check(BonusValue::DecodeRaw(original.EncodeRaw())==original);
    }
    std::cout << "PASS exact decimal parser, equip/remove sums, overflow rejection, 200001 roundtrips\n";
}
