# Fight Night Round 2 recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 1 - set up; headless from a first boot through the front end into a fight, with no game code
- XDK: 5849 QFE 1 (D3D8 not LTCG), 130 of 278 functions, 10 of 13 data from the scan; 2 more by hand
- Push buffers: none recorded (no RunPushBuffer/CreatePushBuffer, Ghidra scan); EAGL's BeginPush/EndPush at 9 sites,
  decoded by the runtime
- Smoke: `scripts/smoke.sh` (scripts/pad/fight.txt) SMOKE OK; boot.txt alone fails it (130 draws, the main menu)
- GitHub: hk21x/fightnight2-recomp, not created yet (local repository only); SDK at fork main c06f201
- Windows: not built yet

## Log

### 2026-11-21 - Ghidra: no raw NV2A writes outside BeginPush; the SSE2 opcodes are an unreachable path

Ghidra project `~/ghidra-projects/fightnight2` (game.exe from xbe2pe, 20,769 exports, full analysis).
- `scripts/ghidra.sh PushBufferScan`: in the game's .text, the only stores through a pointer register with an NV2A
  method shape are `0x417FC` (SET_BEGIN_END, one dword) in the 9 functions that call BeginPush/EndPush (0x210EB0,
  0x210F30, 0x211140, 0x2111C0, 0x211460, 0x211630, 0x211740, 0x220840, 0x220C00). The 0x284790 and 0x2893xx..0x289Dxx
  stores are code addresses (callback tables), the rest vtables. So everything EAGL writes by hand goes through
  BeginPush/EndPush, which the runtime decodes. **Plan item 2 is answered: no recorded push buffers.**
- The unimplemented SSE2 integer ops (pxor, psraw, psubw, paddw, pmulhuw on xmm) are all in sub_25AFA3, a real
  quantiser loop (abs, add rounding, pmulhuw, sign back). The Xbox's Pentium III has no SSE2, so the console never
  runs it (a CPU-specific path in a shared library); it has no direct callers, and its traps were never hit.

Next: the Windows build through recomp-vm (a bundle, as the GitHub repository does not exist yet), then gpu-vm.

### 2026-11-21 - Repository set up from the SDK template; a fight headless; smoke route

Harry asked to start the Fight Night Round 2 recomp after the research pass (`~/fightnight2-research/RESEARCH.md`).
This commit adds the SDK template, the SDK submodule on fork main (c06f201) and the game's names: target and exe
`FightNight2`, config/fightnight2.toml (hand entries; it includes the scan's config/game.toml), the launcher's release
table (8edb378c... "Europe"), the release workflow, README and CLAUDE.md.

Disc: `C:\ISOs\Fight Night Round 2 (Europe) (En,Fr,De).xiso.iso` on recomp-vm and gpu-vm (SHA-1 6e7e75de..., checked
on both), extracted with extract-xiso into assets/ (666 files, 2.1 GB, one xbe).

Evidence, with no game code or hooks:
- Recompile: 20,769 functions, 448 jump tables, 17 C++ catch blocks (2 continuations), 32 overlapping decodes.
  Unimplemented opcodes: the usual data-as-code set plus single MMX ops (paddw, pmulhuw, psraw, psubw, pxor, 1 to 3
  each); the movies decode, so probably data too (to confirm in Ghidra).
- Headless with START/A presses: no traps, no unimplemented kernel exports. EA Sports and intro movies (VP6) play,
  then title, autosave question, main menu, Play Now (Select Corner, Select Boxer, Select Venue), the walk-in and round
  1 of Hopkins v Wright at the Staples Center, at about 300 draws a frame. Log warnings: `wbinvd` ignored (5 sites)
  and DSOUND export B0057 running its original code.
- B0057 is IDirectSoundBuffer_Pause (the 5849 signature set has no body for it; True Crime names it the same way):
  0x2C4374, 0x1C bytes shaped like Play 0x2C4314 and Stop 0x2C4338. Named by hand; the warning is gone.
- D3D_PresentInterval, the scan's missing data symbol, is render state 127 at 0x2BE72C: the present code compares it
  with 0x80000000 (0x2B23FA, 0x2B25F7), 0x2B5384 stores it. Named by hand.
- Push buffers (plan item 2): RunPushBuffer and CreatePushBuffer are not linked. EAGL calls BeginPush/EndPush at 9
  sites (0x210F20 to 0x22103C); the runtime's EndPush decoder ran them with no stop or dropped primitive logged.
- Smoke route `scripts/pad/fight.txt`: SMOKE OK, menu (Select Venue, vertical blanks 2500 to 2680) 67 draws a frame,
  the fight 274 over the last 300 frames, no traps, 150 s. With `PAD=scripts/pad/boot.txt` it fails: 130 draws a frame
  in that window (the main menu).

Next: Ghidra project `fightnight2` (PushBufferScan for raw NV2A writes outside BeginPush; the MMX opcodes), a Windows
build through recomp-vm, then the picture and audio on gpu-vm.
