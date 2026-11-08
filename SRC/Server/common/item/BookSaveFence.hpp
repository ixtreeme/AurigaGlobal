#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace auriga::item
{
// Protect all ten typed progression participant columns in the legacy full-player UPDATE.
// The caller supplies the already SQL-escaped skill literal's exact size; no
// delimiter search inside arbitrary binary data. Missing fields fail closed.
inline std::optional<std::string> ProtectBookSave(std::string query,std::uint64_t revision,std::string_view escapedSkills)
{
    const std::string guard="typed_book_revision<="+std::to_string(revision);
    for(const std::string field:{"exp","skill_point","sub_skill_point","horse_skill_point","skill_level","level","level_step","stat_point","random_hp","random_sp"})
    {
        const std::string prefix=field+" = ";auto start=query.find(prefix);while(start!=std::string::npos && (!start || (query[start-1]!=' ' && query[start-1]!=',')))start=query.find(prefix,start+1);if(start==std::string::npos)return {};
        const auto value=start+prefix.size();const auto end=field=="skill_level" ? value+escapedSkills.size()+2 : query.find(", ",value);
        if(end==std::string::npos || end>=query.size())return {};
        const auto original=query.substr(value,end-value);if(field=="skill_level" && original!="'"+std::string(escapedSkills)+"'")return {};
        query.replace(value,end-value,"IF("+guard+","+original+","+field+")");
    }
    const auto where=query.rfind(" WHERE id=");if(where==std::string::npos)return {};
    query.insert(where,", typed_book_revision=GREATEST(typed_book_revision,"+std::to_string(revision)+")");return query;
}
}
