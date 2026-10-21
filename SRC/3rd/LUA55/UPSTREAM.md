Lua 5.5.1 from https://www.lua.org/ftp/lua-5.5.1.tar.gz

SHA256: `1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce`

The vendored source is built as C++ for exception unwinding. The legacy
quest compiler has a separate Lua 5.0 library; no executable links both VMs.

Applied runtime-only official fixes listed at https://www.lua.org/bugs.html:
- https://github.com/lua/lua/commit/0b29f408433e92953cc72b1d3e06c7ac8139e439
- https://github.com/lua/lua/commit/e8533201cd2448d668c9b64758fc44205544b27f
- https://github.com/lua/lua/commit/b35736b9d71182bae67492af2a387b3f6e89fd05
- https://github.com/lua/lua/commit/15b2a78106f85a3f8ef2efe5683148d9dad84b33
