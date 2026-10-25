# Initial alpha limitations

- Partial recovered-source implementation; no universal gameplay, cycle,
  raster, sound-output or hardware parity claim.
- Original arcade supports the factory paid USA profile. Full operator/service
  menus, active battery-statistics and board/card hardware are not implemented.
- Enhanced quick matches preserve intentional interface and round-music
  conveniences separately from original arcade flow. Testing macro is not the
  recovered Classic CPU.
- High-level QSound uses plain panning instead of the original Q1 spatial
  filter and does not implement the chip's three ADPCM voices. Sound register
  timing and host buffering are approximate. Minor non-disruptive clicks,
  timing differences and dropouts are accepted initial-alpha limitations.
- Reported sprite-facing, Akuma voice selection and F9 figure-centering issues
  remain queued. A game-only third layout and dither smoothing are future work.
- Container-built executables run on the host and require a compatible Linux
  loader/runtime. SDL3, its audio-backend client dependencies/data and C++ runtime
  are bundled. The OS loader/glibc, graphics drivers and working audio service
  or sound device are host requirements. See CONTAINERS.md for the Linux baseline.
- Linux is the initial tested platform. Multiplayer is local; no network play.
- Source release only. Supply original data locally; generated game data and
  tutorial code must not be committed or included in source archives.
