#include <item/BookAids.hpp>
#include "ContentValidation.hpp"
namespace auriga::item
{
bool ValidBookAidProfile(const BookAidProfile& p) noexcept
{
    return p.id && detail::ContentCode(p.code) && p.kind>=BookAidKind::Chance &&
        p.kind<=BookAidKind::DelayBypass && p.unitsPerUse && p.chargesPerUse &&
        p.chargesPerUse<=p.maximumCharges;
}
}
