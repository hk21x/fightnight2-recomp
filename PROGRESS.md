# Fight Night Round 2 recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 1 - set up; headless from a first boot through the front end into a fight, with no game code
- XDK: 5849 QFE 1 (D3D8 not LTCG), 130 of 278 functions, 10 of 13 data from the scan; 2 more by hand
- Push buffers: none recorded (no RunPushBuffer/CreatePushBuffer, Ghidra scan); EAGL's BeginPush/EndPush at 9 sites,
  decoded by the runtime
- Smoke: `scripts/smoke.sh` (scripts/pad/fight.txt) SMOKE OK; boot.txt alone fails it (130 draws, the main menu)
- GitHub: hk21x/fightnight2-recomp, not created yet (local repository only); SDK at fork main c06f201
- Windows: 58a9ed1 runs on GPU-VM (Arc): menus and two rounds of a fight, native 16:9, no crash; no sound there (no
  audio device). `\\192.168.1.229\Testing\fightnight2-test-b574ce7` ready for Harry
- Open: the game steps 0.02 s a frame for PAL 50 Hz but gets 60 Hz vertical blanks: about 20% fast (SDK fix)

## Log

### 2026-11-21 - GPU-VM: a fight on the Arc; the game runs 20% fast (50 Hz game on 60 Hz vertical blanks)

58a9ed1 (built by recomp-vm) on GPU-VM (Intel Arc A750, 1920x1080), in `C:\Testing\fightnight2-recomp` with the disc
from `C:\ISOs`, started on the desktop by the scheduled task FN2Run (`C:\fn2ctl\run.ps1`: env.txt, shot.req, stop.req)
and driven through `FN2_PAD_FILE=C:\fn2ctl\pad.txt` (src/common/test_pad.cpp, this commit's parent).
- **Works:** the VP6 movies, title, autosave question, main menu, Play Now, Select Boxer and Select Venue, the "Tonight's
  Fight" intro, two full rounds of Hopkins v Wright with right-stick punches landing, the pause menu, corner scenes
  between rounds. 30 minutes, then a clean exit (code 0). No device loss, no crash, no warnings but one (below).
- **Native widescreen:** the front end and the fight fill 16:9 with round circles and the HUD at the edges.
- Frames: about 600 in 10 s, 1 or 2 over 25 ms; 356 MB of video memory; 29 pipelines. `SHOT` screenshots work.
- **No sound on GPU-VM:** `XAudio2 unavailable` (the VM has no audio device), so sound is for Harry's PC.
- Two small things to look at: a faint box edge left of Hopkins on Select Boxer and pale triangles over the venue
  preview (both in the headless CPU picture too, so probably the game's art), and a pale-blue fragment beside Wright's
  glove in a between-rounds close-up (Windows only so far).
- **Timing bug [console]:** round 2 was on screen (HUD up) for 3:38 to 4:04 real time. In a headless fight, memory dumps
  20.18 s and 18.70 s apart show a game-time float at 0x37DE1C advancing exactly 0.02 a frame (+24.000 over 1,200
  frames, +22.198 over 1,110), so 1.19 s a second, while the millisecond counters 0x39D2C8 and 0x39D778 advance 1,000 a
  second. The game creates a 640x576 device with refresh 0 (the default), which on the Xbox is PAL 50 Hz, and waits on
  the vertical blank (BlockUntilVerticalBlank from 0x169200 and 0xCAC60); the runtime's vertical blank thread
  (sdk runtime/xdk/d3d8.cpp VerticalBlank) always ticks at 60 Hz and presents are paced to 60. So the simulation likely
  runs 20% fast. The game has no 60 Hz option of its own, and PAL 60 on (pal_console) does not change its mode.
  Fix (SDK, all-games gate): vertical blanks and present pacing at 50 Hz for a 576-line (or 50 Hz) mode, ideally with
  the runtime's frame blending for a 60 Hz screen. Harry to confirm the speed by eye on his PC first.

Next: Harry plays the test build for sound and speed; then the 50 Hz vertical blank in the SDK.

### 2026-11-21 - Windows build passes (recomp-vm); widescreen looks native

- recomp-vm built b574ce7 from a git bundle (the GitHub repository does not exist yet): clang 22.1.3, Release, 429
  steps, no errors, "Fight Night Round 2 built." FightNight2.exe 19,705,856 bytes, SHA-256 9886cf1f...c08cac.
  Ready to run, not run: `\\192.168.1.229\Testing\fightnight2-test-b574ce7` (exe, pdb, assets from the ISO, SHA-1s
  checked).
- Widescreen: the xbe has an APT variable `gWidescreenMode` and a `bWidescreen` option in its settings block. Headless
  the default display is 1280x720, so the game has been told "widescreen" all along. A fight frame at OUTPUT=1280x720
  against OUTPUT=1024x768 (vblank 3900; not the same moment) shows narrower boxers and HUD bars at 16:9: anamorphic
  16:9 in the 640x576 buffer, so probably native. Confirm on Windows (circles stay round at 16:9, the HUD in place).

Next: Harry runs the test folder (a fight, the picture at 16:9, the sound, the fight clock against the wall clock for a
50 Hz assumption). Push once hk21x/fightnight2-recomp exists.

### 2026-11-21 - Audio headless: sound from the logo through the fight

`scripts/harness.sh 120 scripts/pad/fight.txt WAV=...`, then `scripts/wavstats.py`: 118 s at 48 kHz stereo, sound from
the EA Sports logo movie (log 2.3 s) on: the intro, the menus' music, the walk-in and the fight, about -20 to -30 dB
RMS, peaks to -5.5 dB, **no clipped samples**. Silent: the first second, and 6 to 7 s, where START cuts the logo and
the intro starts. DSOUND loads its effects image from memory (13 effects). Whether it sounds right is for Windows.

### 2026-11-21 - A PAL console, and the disc's three languages

`src/common/game_info.cpp`: `.pal_console = true` (a PAL-only disc, region 4, like True Crime, NightFire and EoN's PAL
builds: AV region PAL-I with PAL 60 Hz on) and `.languages = {1, 3, 4}` (English, German, French: the disc's
`locale` files and the movies' .nfx/.ger/.fre), so the settings window offers them.
- Log: `kernel: game regions 4: a PAL (60 Hz) console`. The game creates the same 640x576 device either way
  (interval 0 in the menus, 2 in the fight, so 30 fps at 60 Hz). SMOKE OK (menu 67, fight 294 draws a frame).
- Not known yet: whether any game timing assumes 50 Hz. Check the fight clock against the wall clock on Windows.

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
