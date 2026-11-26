# Typed mount runtime retirement

CostumeMountRuntime and its item-entity/socket-backed actors are removed.
Mount followers use PetSystem::SummonTyped; riding, lifetime, bonuses, skin
and recall intent are typed persistent item state.

TypedMountCommands bridges /ride, /unmount, /user_horse_ride and
/user_horse_back to ApplyCompanionAction. It therefore shares the CG249
resource admission, session fence and transactional receipt. Regular horses
retain their own horse runtime and fallback commands.

Unity's mount hotkey and target-board dismount choose the selected typed
mount seal from an authoritative inventory snapshot and send CG249 directly.
Dormant seals and pets are not implicitly summoned by the hotkey.

Flight uses only TypedMountFlightPermission. Raw prototype value5, legacy
equipment slots, sockets and transmutation vnums cannot grant flight.
The existing flight protocol remains on 221; typed inventory remains on 225.

The old affect-based AutoRecallProcess and pet recall item locks are retired.
Typed restoration runs through the existing companion reconciliation after
login and through its periodic retry; resurrection invokes it explicitly.
Retired pet recall affect rows are ignored during hydration. No SQL/catalog
rows are deleted or activated by this code change.
