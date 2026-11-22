# Fight Night Round 2 recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 1 - set up; headless from a first boot through the front end into a fight, with no game code
- XDK: 5849 QFE 1 (D3D8 not LTCG), 130 of 278 functions, 10 of 13 data from the scan; 2 more by hand
- Push buffers: none recorded (no RunPushBuffer/CreatePushBuffer, Ghidra scan); EAGL's BeginPush/EndPush at 9 sites,
  decoded by the runtime
- Smoke: `scripts/smoke.sh` (scripts/pad/fight.txt) SMOKE OK; boot.txt alone fails it (130 draws, the main menu)
- GitHub: hk21x/fightnight2-recomp (private), main pushed; SDK on fork branch fn2-hold-aspect 8fa489f (fork main
  5af29c1 + HoldAspect for any title + GameInfo::vblank_hz), to gate before fork main
- Windows: 58a9ed1 runs on GPU-VM (Arc): menus and two rounds of a fight, native 16:9, no crash; no sound there (no
  audio device). `\\192.168.1.229\Testing\fightnight2-test-b574ce7` ready for Harry
- PAL 50 Hz: vblank_hz = 50, game time at real speed

## Log

### 2026-11-22 - Quality of life: skip intro, hide HUD, sharp 2D, mouse in menus; HUD position and achievements not done

Harry asked for items 1 to 6 of the list of other ports' features. Branch qol.
- **Skip intro movies** (`skip_intro`): hiding the files does not work (the game asks for a missing movie for ever), so
  src/game/movies.cpp returns from the front end movie player 0xCD9B0 (stdcall name, 0, 0; ret 12) for easports and
  intro. GPU-VM (6cd9b45): straight to the title.
- **Hide the HUD** (`hide_hud`, F9): the game's own bHud setting is bit 0 of 0x37FFEF (settings 0x37FFD0 from 0x1659B0,
  +0x1C bit 24; 0xC60D0 saves it as "bHud"; dumps either side of the pause menu's switch read 0x3F, 0x3E). Held clear
  while on. Headless fight: no HUD. (0x39DF36/46 are only the pause menu's item; 0x37E24C a frame parity.)
- **Sharp 2D** (`sharpen_2d`): the SDK's sharp redraw of 2D textures, on (Settings::sharp_2d).
- **Mouse in menus** (src/common/menu_mouse.cpp): left click A, right click B, wheel up and down, out of a fight only (the
  mouse punches and its right button blocks in one). The APT menus have no pointer, so no on-screen cursor.
- **HUD position (not done):** a "HUD at the screen's edges" setting moving the pieces in with State::hud_layout did
  nothing on GPU-VM (the fight's bars are not HudMove's flat matrix pieces); removed. The HUD stays at the edges.
- **Achievements (not done):** found the Career boxer's record (bytes per boxer: wins +5, losses +6, draws +7, KOs +8,
  written as nRecW/L/D/KO by 0xAA300), not yet the pointer to the current boxer nor a fight's result; needs a Career
  boxer to verify on.

### 2026-11-22 - Smooth motion: in-between frames in a fight, the clock unchanged

Harry asked for a higher frame rate with the clock kept right. The game presents every 20 ms in a fight (50 fps at
50 Hz, not 25 as assumed). The SDK's in-between frames (gpu/interp.cpp, GameInfo::step_interpolation) blend each
step's vertex constants with the step before's and draw extra frames up to the frame cap; the game still steps 50
times a second. Setting `smooth_motion` (on), only in a fight (fn2::InFight).
- SDK (fork branch fn2-hold-aspect bf205db): no draw ever matched, because the game rotates three back buffers
  (00A73000, 00BDC000, 0090A000) and the match key held the target's memory; presented surfaces now count as one target
  (other targets keep their address, EoN's environment maps). And they switch on above GameInfo::vblank_hz, not 60.
- GPU-VM (Arc, 1920x800, fps_cap 150, vsync off): 1,245 to 1,279 frames for about 500 steps each 10 s (2.5 to 3 a
  step), 116 to 132 fps shown, the title's step 3 ms, a replay 2 ms; pictures clean in four shots (fight, round start,
  a punch landing). At fps_cap 60 it plans one frame a step: no change from before.
- Needs a cap of 100 or more (2 frames a step) to show: 100, 150, ... with vsync on a high refresh screen.

### 2026-11-21 - Harry's full fight: too fast, ultrawide boxed in; both fixed; on GitHub

Harry created hk21x/fightnight2-recomp and played a whole fight on 163b7f1 at 3440x1440 ("works great"), with two
problems:
- **Too fast:** the 50 Hz finding. SDK `GameInfo::vblank_hz` (fork branch fn2-hold-aspect 8fa489f, default 60): the
  vertical blank thread and the present pacing (app::g_pace_hz) at the title's rate. FN2 sets 50. Headless fight, two
  memory dumps 20 s apart: game time 0x37DE1C advances 0.99 s a real second (1.19 at 60). SMOKE OK at 50.
- **Ultrawide sides:** his screenshot is a knockdown count, letterboxed and held at 16:9, so boxed in on all four
  sides; his log shows every knockdown, replay and round end held the same way. In-fight cutscenes are the arena's 3D
  (widened as play is) with bars past both edges, so they now fill the screen as play does. "Left the fight" needs a
  second of neither the arena's views nor the fight's 2D (a quarter second dropped a walk-in camera cut to 16:9).
- main fast-forwarded to buttons (cf65fc8) and pushed with buttons; the SDK pin is on the fork branch fn2-hold-aspect,
  not fork main yet (gate and Harry's go).

### 2026-11-21 - GPU-VM unattended: keyboard play, a 15-minute fight, pause and quit; two fixes

Harry asked for further steps diagnosed on GPU-VM without him. Driver (`C:\fn2ctl\run.ps1`, copy in .local/): the
game's own scheduled task plays `cmd.txt` (key/down/up/wait); a key reaches the game only when the window is brought
forward with an F24 tap before SetForegroundWindow (an Alt tap opened the window menu and ate keys).
- **Keyboard:** boot to a fight on Enter alone; the prompts show ENTER, BKSP, C, ESC; Select Corner shows a keyboard and
  mouse; W walks in, I/J/K/L throw punches (hits landed), Shift leans, Esc pauses, the arrows move menus.
- **Soak:** 20 chunks of 40 s of random keyboard play (punches, moves, leans, blocks, the right mouse button) in one fight,
  15 minutes: no crash, private memory flat at about 520 to 550 MB, the end-of-round replays letterboxed at 16:9 and the
  corner scenes filling the screen.
- **Fix 1:** the fight's menus (pause, throw in the towel) use their own A, a 32x32 P8 texture (0x0B, FNV
  0249657C4873E4CD, palette applied at bind) the first pass missed; now replaced too. FN2_PICS_LOG applies P8 palettes.
- **Fix 2:** quitting a fight left the frame filled, so Select Boxer stretched. The fight is now the arena's wide
  views or the fight's own 2D (shapes through 0x125280: HUD, pause, towel; the APT front end never uses it): neither
  for a quarter second holds 16:9 again. Seen on GPU-VM (163b7f1): pause full width, Select Boxer at 16:9 after quitting.
- Polish: a prompt's key cap kept within the disc's width (ENTER crowded the menu text).
- Checked: Game Modes > Hard Hits reaches Select Corner. Not reached: Career, Create Champ, My Gym (left for later).
SMOKE OK.

### 2026-11-21 - GPU-VM at 2.4:1: menus and cutscenes at 16:9, the fight filling the screen

99e55f3 (built by recomp-vm) on GPU-VM in a 1920x800 window, aspect auto. The front end is held at 16:9 from the
start: the full-screen 2D backgrounds stretched across the whole width in 6122d15, so now the frame fills the screen
only once wide (over 20 degrees) 16:9 views have been set up for 300 ms, and is held again by a cutscene's bars or a
menu's 8-degree boxer view. The pause menu leaves the camera alone and stays filled. Seen: the main menu pillarboxed with
the modern A and Y prompts; the "Tonight's Fight" cutscene letterboxed and pillarboxed at 16:9; the fight filling
2.4:1 with the arena wider at the sides and the HUD at the screen's edges; Select Boxer's A B Y prompts modern.
Not checkable on GPU-VM: the keyboard pictures (injected keys do not reach the game there; Enter passed the title once,
nothing more); the headless dumps show them.

### 2026-11-21 - Modern button pictures, keyboard and mouse controls, ultrawide

Harry asked for the Xbox buttons mapped and drawn as today's pads, PC controls with their own pictures, and ultrawide
with cutscenes kept at their own shape. Branch `buttons`.
- **Button pictures** (src/common/buttons.cpp, from Juiced's and 50 Cent's): the prompts' A B X Y and START are separate
  32x32 DXT3 textures loaded with the front end; Select Corner's pad is 128x64 (the pad in its upper left), the
  Controls page's 128x128. Found with FN2_PICS_LOG (each bound texture logged with level 0's FNV-1a and dumped). When
  one is bound the renderers get a 4x picture (gpu::SetReplacement): A B X Y as coloured letters on dark discs, START
  as Menu, the pads as today's pad; with the keyboard the bound keys (Enter, Backspace, T, C, Esc) and a keyboard and
  mouse. The Controls page's lines are the game's draws, so each control is drawn where its line ends (measured on
  GPU-VM's 1920x1080 frame); White and Black are shown as LB and RB. Setting `modern_buttons` (on). Headless:
  FN2_KEY_PICTURES=1, FN2_BUTTONS_DUMP=dir.
- **Controls** (My Corner > Options > Controls, needs a profile): configuration 1 (default) has LT lean/body punch, RT
  block, the right stick Total Punch Control, A signature, B illegal blow, X taunt, Y clinch, White signature, Black
  illegal blow, START pause, the left stick and d-pad movement; configuration 4 puts the punches on A B X Y.
- **Keyboard and mouse** (GameInfo::default_keys, mouse_look): WASD moves, the mouse is the right stick (IJKL too),
  Shift leans, Space and the right button block, Enter/Backspace choose and go back in menus and are A/B, F and G
  are A and B in a fight, T taunt, C clinch, E and R White and Black, Esc/P pause, arrows the d-pad.
- **Ultrawide** (src/game/ultrawide.cpp): 0x213400 builds every 3D view's projection and culling planes (camera;
  horizontal fov in degrees, aspect, near, far), about once a frame; in widescreen mode the views ask for 16:9 with the
  fov widened by 4/3 in its tangent (0x133040). A 16:9 view is given the frame's shape and its fov widened the same
  way; `auto_fills_screen`. Headless at FRAME_ASPECT_X100=239: "3D views at 2.390:1 (57.8 degrees across shown at
  73.2)". The 2D is 16:9 and kept in the middle by the renderers.
- **Cutscenes**: the in-engine ones are letterboxed by untextured shapes 854 wide (x -107, 33.5 high) through the 2D
  shape drawer 0x125280 (found with gdb: EndPush from 0x210EB0 from 0x125280 from the frame 0x169200). While one is on
  screen the frame is held at 16:9 (app::HoldAspect): headless, "cutscene begins" at 47.5 s and "ends" at 52.1 s when
  round 1 starts. HoldAspect only worked with aspect_live, so the SDK change is on fork branch `fn2-hold-aspect`
  (08c1db7, over fork main 5af29c1); 50 Cent, the only other caller, is unchanged. It needs the all-games gate and
  Harry's go before fork main.
- SMOKE OK (menu 67, fight 296 draws a frame). Windows build of 6122d15 asked of recomp-vm, for GPU-VM.

### 2026-11-21 - Harry: the Windows build plays into a fight, with sound

Harry ran the Windows build on his PC: "Game launches into a fight, perfect." and "Audio works also". Not yet
reported: whether the game looks 20% fast (the 50 Hz finding below).

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
