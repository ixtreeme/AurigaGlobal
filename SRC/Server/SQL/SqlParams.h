#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Out-of-band parameter for server-side prepared statements. The value is
// sent through the MySQL binary protocol, so no quoting/escaping is needed
// and the statement text never changes between executions (statement cache).
enum class SqlParamType : uint8_t
{
    Null,
    Signed,
    Unsigned,
    Real,
    Text,
    Blob,
};

struct SqlParam
{
    SqlParamType type{SqlParamType::Null};
    int64_t i64{0};
    uint64_t u64{0};
    double f64{0.0};
    std::string bytes;

    static SqlParam Null()
    {
        return SqlParam{};
    }
    static SqlParam Int(int64_t v)
    {
        SqlParam p;
        p.type = SqlParamType::Signed;
        p.i64 = v;
        return p;
    }
    static SqlParam UInt(uint64_t v)
    {
        SqlParam p;
        p.type = SqlParamType::Unsigned;
        p.u64 = v;
        return p;
    }
    static SqlParam Real(double v)
    {
        SqlParam p;
        p.type = SqlParamType::Real;
        p.f64 = v;
        return p;
    }
    static SqlParam Text(std::string v)
    {
        SqlParam p;
        p.type = SqlParamType::Text;
        p.bytes = std::move(v);
        return p;
    }
    static SqlParam Text(const char *v)
    {
        return v ? Text(std::string(v)) : Null();
    }
    static SqlParam Blob(std::string v)
    {
        SqlParam p;
        p.type = SqlParamType::Blob;
        p.bytes = std::move(v);
        return p;
    }
};

using SqlParams = std::vector<SqlParam>;
