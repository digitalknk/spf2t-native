# Source origins

The native host consumes the complete spf2t-decomp-v1 handoff, API1, from the
recovered-source project. It needs no source checkout or exact Git pin. SoundDriver, QSound and the archive helper are from
tmcleroy/cps2recomp at `145314f81e18d05040fcbe9e87c4df50a57e9a28`, copyright 2026 Tommy McLeroy, GPL-3.0-only.
driver.h adds read-only MusicPlaying/EffectsPlaying/TempoAdjustment accessors.
qsound.h explicitly includes cstddef and uses std::size_t for portable compilation.
Both changes carry dated notices; sequencing/mixing behavior is unchanged.
https://github.com/tmcleroy/cps2recomp/tree/145314f81e18d05040fcbe9e87c4df50a57e9a28

SDL3 is an external dependency and is not vendored in this source release.

SDL3-LICENSE.txt is the upstream SDL3.4.16 license. GCC-runtime-copyright.txt
contains Debian GCC12 runtime notices/exception for the tested container
toolchain. Native host builds retain these GNU runtime terms; other compiler
licenses must also be respected when using an alternate toolchain.
