# Dungeon core map routing capacity

The raid remains on CH1 core1, alongside the existing dungeon templates. This
core has 33 configured maps after adding template 430. Its old setup packet
only had 32 map slots, and `map_allow_copy` wrote a 33rd value past the array
before checking capacity. That can overwrite the adjacent authentication flag.
The map-location receiver also stopped after a hard-coded 32 entries.

The shared routing capacity is now 64. The copy routine checks bounds before
writing and zeroes unused entries; the receiver uses the shared limit. Game
configuration loading rejects lists larger than the wire capacity. The new
regression test covers 33 maps, full and overflowing arrays, packet canaries,
the unused tail and zero-capacity output.

This changes the internal GameServer/Database/LoginServer setup packet layout.
Build and install **GameServer, Database, LoginServer and ServerPackageCheck**
together with every process stopped. Do not mix old and new server binaries.
No game-client protocol or Unity build change is required.

The canonical Git/Linux header path is `SRC/Server/common/CommonDefines.h`.
Windows path matching hides the case difference; uploading a new file under
`SRC/Server/Common/` on FreeBSD does not update the header used by the build.

After restarting, check `ECLIPSE_ROUTE_REGISTERED map=430` on the hosting core
and on the other cores, in addition to `ECLIPSE_ATTRIBUTE_OK`. Attribute loading
alone only proves map data was loaded, not that the teleport route was announced.
