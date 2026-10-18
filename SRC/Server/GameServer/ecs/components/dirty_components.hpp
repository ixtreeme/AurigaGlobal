#pragma once

namespace ecs {

// Generic mutation marker shared by gameplay services. It carries no field or
// recipient information and does not schedule packets. Each state owner must
// explicitly publish its committed change through its protocol-specific path.
struct DirtyTag {};

} // namespace ecs
