#include "quest_source.h"
#include "../core/lua_incl.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace quest::source {
namespace {
struct Token { std::string word; size_t begin, end; int line; bool quoted = false; };
[[noreturn]] void Fail(const std::string& file, int line, const std::string& message) {
    throw std::runtime_error(file + ":" + std::to_string(line) + ": " + message);
}
std::vector<Token> Lex(const std::string& text, const std::string& file) {
    std::vector<Token> tokens;
    size_t i = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;
    int line = 1;
    auto advance = [&](size_t end) { while (i < end) if (text[i++] == '\n') ++line; };
    auto longEnd = [&](size_t start) -> size_t {
        if (text[start] != '[') return start;
        size_t j = start + 1;
        while (j < text.size() && text[j] == '=') ++j;
        if (j == text.size() || text[j] != '[') return start;
        const std::string close = "]" + std::string(j - start - 1, '=') + "]";
        const size_t end = text.find(close, j + 1);
        if (end == std::string::npos) Fail(file, line, "unfinished long string/comment");
        return end + close.size();
    };
    while (i < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[i]))) { advance(i + 1); continue; }
        if (text.compare(i, 2, "--") == 0) {
            const size_t end = i + 2 < text.size() ? longEnd(i + 2) : i + 2;
            if (end > i + 2) advance(end);
            else { const auto newline = text.find('\n', i); advance(newline == std::string::npos ? text.size() : newline); }
            continue;
        }
        const size_t start = i;
        const int startLine = line;
        bool quoted = false;
        if (text[i] == '"' || text[i] == '\'') {
            const char quote = text[i++];
            bool closed = false;
            while (i < text.size()) {
                if (text[i] == '\\') {
                    const char escaped = i + 1 < text.size() ? text[i + 1] : '\0';
                    if (escaped == 'z') {
                        advance(i + 2);
                        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) advance(i + 1);
                    } else {
                        const bool pair = i + 2 < text.size() &&
                            ((escaped == '\r' && text[i + 2] == '\n') || (escaped == '\n' && text[i + 2] == '\r'));
                        advance(std::min(text.size(), i + (pair ? 3 : 2)));
                    }
                    continue;
                }
                if (text[i] == quote) { ++i; closed = true; break; }
                if (text[i] == '\n' || text[i] == '\r') Fail(file, line, "unfinished quoted string");
                ++i;
            }
            if (!closed) Fail(file, startLine, "unfinished quoted string");
            quoted = true;
        } else if (const size_t end = longEnd(i); end > i) { advance(end); quoted = true; }
        else if (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_') {
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
        } else ++i;
        tokens.push_back({text.substr(start, i - start), start, i, startLine, quoted});
        if (tokens.size() > 1000000) Fail(file, line, "too many quest tokens");
    }
    tokens.push_back({"<eof>", text.size(), text.size(), line});
    return tokens;
}
bool Identifier(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}
std::string Quote(const std::string& value) {
    std::string result = "\"";
    for (const char c : value) { if (c == '\\' || c == '"') result += '\\'; result += c; }
    return result + "\"";
}
std::string Normalize(const std::string& text, const std::string& file) {
    const auto tokens = Lex(text, file);
    std::string result = text;
    for (size_t i = tokens.size() - 1; i-- > 0;) {
        const auto& token = tokens[i];
        if (token.quoted) continue;
        if (token.word == "begin" && !(i && (tokens[i - 1].word == "." || tokens[i - 1].word == ":"))) result.replace(token.begin, 5, "do");
        else if (token.word == "!") {
            const bool unequal = i + 1 < tokens.size() && tokens[i + 1].word == "=" && token.end == tokens[i + 1].begin;
            result.replace(token.begin, 1, unequal ? "~" : "not ");
        }
    }
    return result;
}
int StateCRC(const std::string& name) {
    uint32_t crc = 0xffffffffu;
    for (const unsigned char c : name) {
        crc ^= c;
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return static_cast<int>(crc ^ 0xffffffffu);
}
class Parser {
    const std::string &text, &file;
    std::vector<Token> tokens;
    size_t at = 0;
    std::string functions;
    std::string word() const { return tokens[at].quoted ? "<string>" : tokens[at].word; }
    bool take(const std::string& value) { if (word() != value) return false; ++at; return true; }
    void expect(const std::string& value) { if (!take(value)) Fail(file, tokens[at].line, "expected '" + value + "', got " + word()); }
    std::string name() {
        const auto& token = tokens[at++];
        if (!Identifier(token.word)) Fail(file, token.line, "expected an identifier");
        return token.word;
    }
    std::string untilBegin() {
        const size_t first = at;
        while (word() != "begin" && word() != "do") {
            if (word() == "<eof>") Fail(file, tokens[at].line, "expected begin after condition");
            ++at;
        }
        if (first == at) Fail(file, tokens[at].line, "empty condition");
        return Normalize(text.substr(tokens[first].begin, tokens[at].begin - tokens[first].begin), file);
    }
    std::string block() {
        const size_t first = at;
        std::vector<std::string> stack;
        while (word() != "<eof>") {
            const auto current = word();
            if (at > first && (tokens[at - 1].word == "." || tokens[at - 1].word == ":")) { ++at; continue; }
            if (current == "end") {
                if (stack.empty()) {
                    std::string body = text.substr(tokens[first].begin, tokens[at].begin - tokens[first].begin);
                    ++at;
                    return std::string(tokens[first].line - 1, '\n') + Normalize(body, file);
                }
                if (stack.back() == "repeat") Fail(file, tokens[at].line, "repeat requires until");
                stack.pop_back();
            } else if (current == "until") {
                if (stack.empty() || stack.back() != "repeat") Fail(file, tokens[at].line, "unexpected until");
                stack.pop_back();
            } else if (current == "if" || current == "function" || current == "do" || current == "begin" || current == "repeat") {
                stack.push_back(current);
                if (stack.size() > 150) Fail(file, tokens[at].line, "quest nesting too deep");
            }
            ++at;
        }
        Fail(file, tokens[at].line, "missing end");
    }
    Handler event() {
        Handler handler;
        std::vector<std::string> parts;
        for (;;) {
            const auto token = tokens[at++];
            if (!(token.quoted || Identifier(token.word) || std::all_of(token.word.begin(), token.word.end(), ::isdigit)))
                Fail(file, token.line, "invalid event selector");
            parts.push_back(token.word);
            if (!take(".")) break;
        }
        if (parts.size() == 1) { handler.target = "notarget"; handler.event = parts[0]; }
        else if (parts[1] == "target") {
            handler.target = "notarget"; handler.event = "target"; handler.argument = parts[0];
            for (size_t i = 2; i < parts.size(); ++i) handler.argument += '.' + parts[i];
        } else {
            handler.target = parts[0]; handler.event = parts[1];
            for (size_t i = 2; i < parts.size(); ++i) { if (i > 2) handler.argument += '.'; handler.argument += parts[i]; }
        }
        std::transform(handler.event.begin(), handler.event.end(), handler.event.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return handler;
    }
public:
    Parser(const std::string& text, const std::string& file) : text(text), file(file), tokens(Lex(text, file)) {}
    Definition parse() {
        Definition result;
        result.file = file;
        expect("quest"); result.name = name();
        if (take("with")) result.startCondition = "return " + untilBegin();
        if (!take("begin")) expect("do");
        while (!take("end")) {
            expect("state"); const std::string state = name();
            // The historical compiler merged repeated state declarations.
            result.states.emplace(state, StateId(state));
            if (!take("begin")) expect("do");
            while (!take("end")) {
                if (take("function")) {
                    const std::string function = name();
                    const size_t argumentStart = tokens[at].begin;
                    expect("(");
                    while (!take(")")) {
                        if (word() == "<eof>") Fail(file, tokens[at].line, "unfinished function parameters");
                        ++at;
                    }
                    const auto args = text.substr(argumentStart, tokens[at - 1].end - argumentStart);
                    functions += ',' + function + "=function" + args + '\n' + block() + "\nend";
                } else {
                    expect("when"); std::vector<Handler> handlers;
                    do { handlers.push_back(event()); } while (take("or"));
                    std::string condition;
                    if (take("with")) condition = untilBegin();
                    if (!take("begin")) expect("do");
                    const auto body = block();
                    for (auto& handler : handlers) {
                        handler.state = state; handler.code = body;
                        if (!condition.empty()) handler.condition = "return " + condition;
                        result.handlers.push_back(std::move(handler));
                    }
                }
            }
        }
        expect("<eof>");
        if (!result.states.contains("start")) Fail(file, 1, "start state missing");
        // Match the original compiler's deterministic CRC collision handling.
        std::set<int> used;
        for (auto& [name, id] : result.states) {
            id = StateCRC(name);
            while (used.contains(id)) id = static_cast<int>(static_cast<uint32_t>(id) + 1);
            used.insert(id);
            if (name == "start") id = 0;
        }
        result.stateCode = result.name + "={";
        bool first = true;
        for (const auto& [state, id] : result.states) {
            if (!first) result.stateCode += ',';
            first = false;
            result.stateCode += '[' + Quote(state) + "]=" + std::to_string(id);
        }
        result.stateCode += functions + '}';
        return result;
    }
};
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error(path.generic_string() + ": cannot open source");
    std::string text(std::istreambuf_iterator<char>(input), {});
    if (text.size() > 8 * 1024 * 1024 || text.find('\0') != std::string::npos)
        throw std::runtime_error(path.generic_string() + ": source too large or binary");
    return text;
}
void Check(lua_State* L, const std::string& text, const std::string& name) {
    if (luaL_loadbufferx(L, text.data(), text.size(), name.c_str(), "t") != LUA_OK) {
        const std::string error = lua_tostring(L, -1); lua_pop(L, 1);
        throw std::runtime_error(error);
    }
    lua_pop(L, 1);
}
}
int StateId(const std::string& name) {
    return name == "start" ? 0 : StateCRC(name);
}
Definition ParseLegacy(const std::string& text, const std::string& file) {
    if (text.size() > 8 * 1024 * 1024 || text.find('\0') != std::string::npos) Fail(file, 1, "source too large or binary");
    return Parser(text.starts_with("\xef\xbb\xbf") ? text.substr(3) : text, file).parse();
}
std::string HandlerCode(const Handler& handler) {
    const auto body = "do\n" + handler.code + "\nend\n";
    if (handler.condition.empty()) return body;
    return "if (function() " + handler.condition + " end)() then\n" + body + "return end\n";
}
void Validate(lua_State* L, const Definition& definition) {
    Check(L, definition.stateCode, "@" + definition.file);
    if (!definition.startCondition.empty()) Check(L, definition.startCondition, "@" + definition.file);
    std::map<std::tuple<std::string, std::string, std::string>, std::string> grouped;
    for (const auto& handler : definition.handlers) {
        Check(L, handler.code, "@" + definition.file);
        if (!handler.condition.empty()) Check(L, handler.condition, "@" + definition.file);
        if (!handler.argument.empty() && handler.event == "chat") Check(L, "return " + handler.argument, "@" + definition.file);
        if (handler.argument.empty()) grouped[{handler.target, handler.event, handler.state}] += HandlerCode(handler);
    }
    for (const auto& [key, code] : grouped) Check(L, code, "@" + definition.file);
}
Catalog Load(lua_State* L, const std::filesystem::path& root) {
    const auto canonical = std::filesystem::canonical(root);
    auto listBytes = Read(canonical / "quest_list");
    if (listBytes.starts_with("\xef\xbb\xbf")) listBytes.erase(0, 3);
    std::istringstream list(listBytes);
    Catalog catalog;
    std::set<std::string> names, paths;
    std::string line;
    while (std::getline(list, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        std::filesystem::path relative(line);
        if (relative.is_absolute()) throw std::runtime_error("quest_list: absolute path rejected");
        const auto path = std::filesystem::canonical(canonical / relative);
        const auto within = path.lexically_relative(canonical);
        if (within.empty() || *within.begin() == "..") throw std::runtime_error("quest_list: source outside quest root");
        if (!paths.insert(within.generic_string()).second) throw std::runtime_error("quest_list: duplicate path: " + line);
        auto definition = ParseLegacy(Read(path), within.generic_string());
        if (!names.insert(definition.name).second) throw std::runtime_error(line + ": duplicate quest id: " + definition.name);
        Validate(L, definition);
        catalog.push_back(std::move(definition));
    }
    return catalog;
}
Catalog LoadFiles(lua_State* L, const std::map<std::string, std::string>& files) {
    const auto list = files.find("quest/quest_list");
    if (list == files.end()) throw std::runtime_error("quest_list missing from generation");
    auto listBytes = list->second;
    if (listBytes.starts_with("\xef\xbb\xbf")) listBytes.erase(0, 3);
    std::istringstream input(listBytes);
    Catalog catalog; std::set<std::string> names, paths; std::string row;
    while (std::getline(input, row)) {
        const auto first = row.find_first_not_of(" \t\r");
        if (first == std::string::npos || row[first] == '#') continue;
        row = row.substr(first, row.find_last_not_of(" \t\r") - first + 1);
        const auto path = std::filesystem::path(row).lexically_normal();
        if (path.is_absolute() || path.has_root_name() || path.empty() || *path.begin() == "..") throw std::runtime_error("quest_list path outside quest root");
        const auto key = "quest/" + path.generic_string();
        if (!paths.insert(key).second) throw std::runtime_error("quest_list duplicate source: " + row);
        const auto source = files.find(key);
        if (source == files.end()) throw std::runtime_error("quest source missing: " + row);
        auto definition = ParseLegacy(source->second, path.generic_string());
        if (!names.insert(definition.name).second) throw std::runtime_error("duplicate quest id: " + definition.name);
        Validate(L, definition); catalog.push_back(std::move(definition));
    }
    return catalog;
}
}
