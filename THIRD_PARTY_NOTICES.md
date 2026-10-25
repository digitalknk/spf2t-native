# Third-party notices

Project code and cps2recomp SoundDriver/QSound/archive helper: GPL-3.0-only,
copyright 2026 Tommy McLeroy for imported source. See LICENSE and UPSTREAM.md.
QSound follows MAME's BSD-3-Clause high-level implementation by superctr and
Valley Bell; retain third_party/BSD-3-Clause.txt and this attribution.
SDL3: zlib license, copyright Sam Lantinga and contributors. SDL is discovered
externally; source is available at https://github.com/libsdl-org/SDL. Binary
distributions bundling SDL must include its license and matching notices.
Container build output also bundles the GCC C++ runtime libraries. Their
copyright and Runtime Library Exception text are copied from the toolchain
package into third_party/GCC-runtime-copyright.txt and copied into output/licenses/; the
full GPLv3 text is LICENSE. SDL3-LICENSE.txt preserves the SDL license.
Original game content remains owned by its respective rights holders and is
not licensed by this project's GPL grant.

Exported runtime packages also collect the selected SDL audio-client libraries
and their transitive dependencies. Installed package copyright/license notices
are copied where available into licenses/runtime/, with an index by library.
These generated libraries remain outside the public source repository.
