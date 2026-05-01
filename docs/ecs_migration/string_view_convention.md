# std::string_view Usage Convention

Established: Phase 17a (May 2026)

## When To Use std::string_view

Use `std::string_view` for function parameters when:

- The function only reads the string.
- The function does not store the string beyond its scope.
- The function does not pass the string to a C API requiring null termination.

Examples:

```cpp
bool ParseLine(std::string_view line); // Good
void LogMessage(std::string_view msg); // Good
```

Do not use `std::string_view` for parameters when:

- The function passes the value to `fopen`, `strstr`, `strcmp` without size, Lua C API, MySQL, or another null-terminated C API.
- The function stores the value in a class member or global.
- The function returns a pointer or reference into the string.
- The function builds network packets with fixed-size protocol fields.

Examples:

```cpp
void OpenFile(std::string_view path); // Bad: fopen needs null termination.
void StoreName(std::string_view name) { m_name = name; } // Store as std::string instead.
```

## When To Use std::string

Class members:

- Always use `std::string` for owning storage.
- Never use `std::string_view` as a member unless the owner lifetime is explicit and reviewed.

Function returns:

- Return `std::string` when the caller needs to own the result.
- Return `std::string_view` only when returning a view of stable memory, such as a long-lived class member.

## Conversion Patterns

From `std::string` to `std::string_view`:

```cpp
std::string s = "hello";
Process(s); // implicit conversion, OK
```

From `std::string_view` to `std::string`:

```cpp
std::string_view sv = ...;
std::string s(sv); // explicit copy
```

To a null-terminated C API:

```cpp
std::string_view sv = ...;
std::string tmp(sv);
fopen(tmp.c_str(), "r"); // tmp ensures null termination
```

Comparisons:

```cpp
if (sv == "constant") { ... }
if (sv == otherSv) { ... }
```

String view literals:

```cpp
using namespace std::string_view_literals;

if (name == "admin"sv) { ... }
constexpr auto path = "config/server.conf"sv;
```

The `sv` suffix creates a `std::string_view` directly. Prefer it in new code when comparing a `std::string_view` against a literal, because it avoids treating the literal as a null-terminated `const char*`.

Substring:

```cpp
std::string_view ext = sv.substr(sv.rfind('.') + 1); // zero-copy
```

## ECS Component Accessors

Returning `std::string_view` is allowed when the returned view points at stable, owning storage.

Primary safe case:

```cpp
namespace ecs::PlayerRuntime {
    std::string_view GetName(entt::entity e) {
        return registry.get<PlayerName>(e).name;
    }
}
```

Rules:

- The component or class must own the backing `std::string`.
- The backing storage must live at least as long as the returned view is used.
- Entity-bound component storage is acceptable while the entity exists and the component is not removed.
- Never return `std::string_view` from a stack-local `std::string`.
- Never return `std::string_view` from a temporary constructed inside the function.

## Phase 17 Guardrails

- Do not migrate packet structs or DB serialization fields.
- Do not migrate Lua C API boundaries.
- Do not migrate SQL, socket, or file API boundaries without an explicit owning `std::string` adapter.
- Prefer small, file-local migrations with a build gate after each file pair.
