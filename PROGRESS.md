# Fight Night Round 2 recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Milestone: 3 - the whole game plays on Windows: Play Now, Career (sparring, fights, results saved), Create Champ,
  My Gym, My Corner; no traps seen headless or on GPU-VM
- XDK: 5849 QFE 1 (D3D8 not LTCG), 130 of 278 functions, 10 of 13 data from the scan; 2 more by hand
- Push buffers: none recorded (no RunPushBuffer/CreatePushBuffer, Ghidra scan); EAGL's BeginPush/EndPush at 9 sites,
  decoded by the runtime
- Smoke: `scripts/smoke.sh` (scripts/pad/fight.txt) SMOKE OK
- GitHub: hk21x/fightnight2-recomp (private), main pushed; SDK on fork branch fn2-hold-aspect 6a3d9df (fork main
  5af29c1 + FN2's runtime changes), to gate before fork main
- Windows: 8303063 on GPU-VM (Arc, 1920x800, fps_cap 240): 40 minutes of every mode, smooth motion 170-210 fps in play;
  no sound there (no audio device). `\\192.168.1.229\Testing\fightnight2-test-8303063` ready for Harry
- PAL 50 Hz: vblank_hz = 50, game time at real speed
- Open: achievements (stopped), LT/RT tab markers still the old pad's L and R, video memory kept after textures are
  freed (1.07 GB after 40 min), the gym drills' instruction popups held at 16:9

## Log

### 2026-11-23 - Bodies torn by landed punches (smooth motion); the knockdown vignette at 16:9

Harry's recording (Window 23-11-2026 11-56-38.mp4) and three screenshots on 2c9ef61, 3440x1440.
- **Bodies:** in-between frames as a punch lands showed the hit boxer's torso crumpled, then in pieces. Bones are
  rigid 4x4 groups from register 0 (rotation, position, 0 0 0 1), c-96 onward. Three causes, SDK fork fn2-hold-aspect:
  blended a number at a time a far-turning bone shrinks (03dc67a: rigid groups blended as quaternions); a piece whose
  first bone turned far was taken as a spinning object, only its place blended, its other bones left at the step
  while the body's other pieces blended (310c54e: not for a rigid first matrix); and skin weighted between bones that
  snap round in one step still tore. A step's largest bone turn is 10-30 degrees, a landed punch sometimes 60-180
  (logged); GameInfo::interp_bone_hold (e0204e5), FN2 45: that step shown as drawn, about 5-10 in 500. GPU-VM 302458d,
  marked bursts while punching: the bodies whole in every frame in between.
- **Knockdown square:** the knockdown's spotlight vignette is top and bottom bands, each in two halves from an edge
  to the middle (-107..320, 320..747 of an 854 wide layout mapped onto the screen). Neither half spans the frame, so
  both were squeezed and the vignette stopped at the 16:9 edges. GameInfo::edge_halves (7634f28), on for FN2: a 2D
  piece from one edge to the middle keeps its edge. GPU-VM 302458d, an idle fight until knocked down: the vignette
  across the frame.
- Ruled out on the way: the round-end fade (one quad 0..640, already full width); the ring's reflection pass.

### 2026-11-23 - Ultrawide after a quit and in the pause menu; smooth motion off in the menus

Harry: quitting a fight at ultrawide, the text flashed for a few seconds before 16:9 came back; then the pause menu
flickered (ffd68d5); then Select Boxer's stats flickered after a quit (debfee0).
- The front end/fight decision (src/game/ultrawide.cpp Decide) waited a second without the fight's 2D (0x125280)
  before going back to 16:9. Measured headless with a pad that pauses and quits (.local/quit.txt, pause.txt): in a
  fight the arena's views never stop for more than 60 ms; paused they stop (on Harry's PC they come back about once a
  second); the pause menu draws its 2D every frame; after a quit both stop at once. Now the fight ends when neither
  has been drawn for 200 ms, and a quit's last view no longer brings the fight's shape back for a frame.
- fn2::InFight (smooth motion's switch) had its own one-second test, true in the menus on Harry's PC (his debfee0 log
  blends 31 frames in its first 10 s, all menus): it is now the same decision. GPU-VM 2c9ef61: no blending in 71 s of
  menus, smooth motion in the fight, none after the quit; Select Boxer's stats steady over 160 grabs from the quit.
- Tried: an arena-views-only rule (ffd68d5: the pause flicker); memory dumps for a pause or fight flag (the fight
  clock 0x37DE1C stops both paused and after a quit; nothing clean found).
- Test folders on recomp-vm tidied (Harry): builds go to C:\Testing\fightnight2-test, captures in fightnight2-captures.

### 2026-11-23 - Ropes: the Staples Center's skinned ropes named; all four rings and the gym validated on GPU-VM

Harry: keep looking. Found by leaving programs out on GPU-VM (XBE_DRAW_SKIP, bisecting): the Staples Center draws its
visible ropes with program AF16804AB89E15AF (131, 463, 529 indices, four of each), skinned: sag bones in the first
registers, the transform at c84 (register 180). Matched by the bone matrices, sections were paired with their
neighbours (the pops at the lens); named but read at register 0 they had no perspective row and were held every step.
- SDK fork fn2-hold-aspect (b485667, 291051a): GameInfo::interp_named_transform (where a named program keeps its
  transform); a step with the camera's move is held only when a named vertex crosses the near plane between the steps
  (behind the camera in both stays behind in between, the blend being linear), and only where the transform is known
  (the 653-index program does more than transform v0: never held by the near plane, as before).
- XBE_INTERP_MARK=1 now works (the console's D3DCLEAR_TARGET is 0xF0): each shown frame marked, the title's own red,
  in between green; ~/scratch/fn2win/spikemarks.py pairs each spike with the marks.
- GPU-VM 4e56296, smooth motion on, 60-grab bursts while punching: Staples Center 6 bursts, Fairgrounds, Viceroy,
  Elevese 3 each, My Gym sparring 1. No rope pops at the lens in any; the highest spikes (at most 21) are camera
  swings, a light flare, or runs of the game's own frames. Held steps are real crossings (the camera against the
  ropes): blended 223-357 of 500 a 10 s at the Fairgrounds, 156-272 the Viceroy, 307-416 the Elevese; those moments
  show at the game's own 50 a second.
- Not done: holding just the crossing sections instead of the whole step (more smoothness against the ropes).

### 2026-11-23 - Rope flicker: one cause fixed, the Staples Center's ropes are another program (stopped)

All four Play Now rings (Staples Center, County Fairgrounds, The Viceroy, The Elevese; the venue list wraps after four)
and My Gym, on GPU-VM with desktop bursts (60 grabs in 1.5 s, FN2Burst; ~/scratch/fn2win/ring.sh, ring2.sh, spike.py
scores each grab against its neighbours).
- **Fixed (SDK df47063):** the game rewrites its 653-index rope sections' vertices in place for its next step; one
  stream in six had changed before the next step's first draw (XBE_INTERP_ROPE_CHECK=1). The render thread, behind by
  the in-between frames, drew torn ropes. Interpolating::Present now waits for the renderer to have read the step's
  own draws. Fairgrounds: spike median 10.6 -> 3.9 (the flicker in Harry's video gone); Viceroy, Elevese, gym clean.
  Game speed unchanged without the grabs (500 steps a 10 s; the grabs themselves slow the VM).
- **Not fixed, Staples Center:** single grabs still show thick ropes and the blue padded rope at the lens with smooth
  motion on, none in 6 bursts with it off. Holding every step with a named draw (XBE_INTERP_NEAR_W=100000) removes
  them; but leaving the 653-index program out altogether (XBE_DRAW_SKIP=B0103131) leaves the ring ropes on screen
  and the pops too: at the Staples Center the visible ropes are drawn by another program, not named, so matched to
  their neighbours like any scenery (the fault fixed for the 653-index ropes two attempts ago). The nearest
  653-index vertex is never under w 100 there (logged); those draws are probably its reflection pass (a 256x256 target).
- Tried and left off by default (SDK 87cc7a4, 62d4608): interp_near_margin (XBE_INTERP_NEAR_W), interp_near_jump
  (XBE_INTERP_NEAR_JUMP 0.3: no change), XBE_INTERP_MARK (draws no mark yet).
- Next: find the Staples ropes' program (XBE_DRAW_SKIP one candidate at a time: B0103671, B0104091, B0103C31,
  B0103171, B01028D1), check whether each venue draws its ropes differently, and name them by code hash in RopeDraw
  (or name rope draws by their texture instead).

### 2026-11-22 - Ropes still flicker with smooth motion (stopped: write-up)

Harry, 9078288 at 3440x1440, fps_cap 240: the ropes flicker on every ring whenever smooth motion is on; his video
(Fairgrounds, 2 s at 240 fps) shows the left and far ropes missing in some frames and drawn at different lengths in
others while the camera barely moves. Reproduced on GPU-VM 9078288 (Fairgrounds, smooth motion on) with 60 desktop
grabs in 1.5 s (a second scheduled task, FN2Burst, C:\fn2ctl\burst.ps1): near rope sections present in some grabs,
gone in the next. The smooth-motion-off comparison was not made: another session started Bulletproof on GPU-VM at 23:15
and its window covered ours (our pad input only reaches Fight Night; our run was stopped by PID).
F3 captures (XBE_CAPTURE_FRAMES) of the same places never show it, in-between frames included (interp_table: 3 replays
a step, every rope section moved by the camera's move). So it shows only at full speed: a race is the lead. Likely:
something a replay reads from the game's memory after the game has gone on to its next step and rebuilt the ropes
(vertices over kCopyBytes 64 KB, or from a base vertex far into a shared buffer, are not kept; an index or constant
read late), which the capture's stalls hide.
Attempts on the ropes so far: interp_pieces off; unblended; named draws by the camera's move only; held at either
camera's near plane; named by program hash (the handle moved between runs). Five; stopped per the rule.
Next: log, for the rope draws, whether KeepVertices kept their stream (bytes, base vertex) at full speed on GPU-VM;
if not kept, keep it (or copy named draws' streams always); then compare against smooth motion off. Until then,
smooth motion off avoids it.

### 2026-11-22 - The ropes shook because they were not recognised: named by program code, not handle

Harry's three shots on 8303063 (3440x1440, Staples Center): rope pieces at different angles. A GPU-VM Play Now at the
Staples Center, three bursts of 30 renderer frames (XBE_CAPTURE_FRAMES): the near red rope jumped from frame to frame.
The capture named the rope program B0103131; RopeDraw asked for B0003131, the handle in the runs it was checked on. A
handle is an address the game allocates, so in these runs the ropes were never named and went back to being matched
section to section, the fault fixed two attempts ago. SDK fork fn2-hold-aspect 2ddd97e: GameInfo::interp_near_hold
gets the program's code hash; RopeDraw names 4D20D4D40AC2A60F with 653 indices and logs any other 653-index program.
Headless: "the ring ropes named" in the fight, steps held as before. SMOKE OK. Main 9078288; Windows build asked.
GPU-VM 9078288, the same Play Now at the Staples Center, three bursts of 30 renderer frames while punching: "the ring
ropes named" in the log; every rope whole and moving smoothly in all 90 frames.
More places, GPU-VM 9078288, smooth motion on, bursts of 30 renderer frames while punching (F3 in the key script):
County Fairgrounds (3 bursts; all 20 ring sections named, 653 indices, every in-between rope section moved by the
camera's move: interp_table 337 of 337 "derived"), The Elevese (3 bursts), My Gym sparring (1 burst in play, 2 on a
trainer tip). Ropes whole and smooth in all of them. One frame at the Fairgrounds (the first of a burst) has a dark
rope-like stick near the camera at the bottom left, gone the next frame; the same kind of piece shows with smooth
motion off, so taken for the game's own near-camera clipping, not proven. The walk-in at the Fairgrounds draws only 2
rope sections.

### 2026-11-22 - Every mode on GPU-VM; the pause menu's untextured pieces kept 16:9

Harry away; tests with no one at the pad (GPU-VM, scripted pad and keys, desktop shots), build 605cf6b then 8303063:
- **Icon:** launcher/icon.ico from Harry's badge (cut from its black, 256 to 16): in the title bar and taskbar.
- **Saves:** a profile made on 2026-11-21 loads after restarts; a new Career (Rebuild Legend: Hopkins) is there after a
  restart; sparring, a fight's result and a created boxer each write profile.sav (19:23, 19:34, 19:38).
- **Career:** Career Central, Trophy Case, Schedule (a fight booked), sparring (jabs, hooks, uppercuts landed from the
  keyboard: IJKL gestures), Training Complete and its unlock, Go Fight: four 2-minute amateur rounds against Romero to
  the judges' scores, corner scenes and round stats, Fight Totals, back to Career Central at 0-1-0, week 5, $300.
- **Create Champ:** all seven tabs (info, physique, build, head shape grid, head features, accessories, ratings), the
  on-screen typewriter, accept and save. **My Gym:** weight lifting, heavy bag (scores), combo dummy (scores).
  **My Corner:** Options, Settings, Controls page list, Profile Manager.
- **Found and fixed:** on a wide frame the pause menu's (and every popup's) orange side strips, selection brackets and
  row bands stood out to the sides of the 16:9 box. They are untextured 2D drawn by program B0101FB1, which computes w
  first, then x, y (beside an rcc), z; gpu::MatrixProgram only knew x, y, z, w in order, so they were never squeezed.
  SDK fork fn2-hold-aspect 6a3d9df: the four dp4s in any order, each into its own lane of oPos. Seen fixed on 8303063
  (pause, quit training, training tips, the gym's pause). SMOKE OK.
- **Seen, not fixed:** Create Champ's tab markers show the old pad's L and R (modern: LT and RT); the gym drills'
  instruction popups are held at 16:9 with the blurred sides until the drill starts; video memory 584 MB through the
  fights, 951 MB after Create Champ (1,870 textures), 1,073 MB with 519 textures left (kept, not returned; a longer
  soak would tell). Log: no errors but the VM's missing audio device; 1 or 2 frames a 10 s over 30 ms.

### 2026-11-22 - The ropes: matched no more, camera's move only, held at either camera's near plane

Harry's shots of 9750208: the left-hand ropes broken into pieces at different angles (no step held). Two causes:
- The rope sections (49 alike, 25 to 50 in view, plus a pass into a 256x256 target) were paired with the wrong
  sections by the nearest-matrix match and blended toward their neighbours. Named draws (interp_near_hold) are no
  longer matched: each takes the camera's move (was = move * now), so a rope moves as one with the scene. A step with
  no camera move found is held.
- A section beside the camera, in front of it now but behind where it was a step before, was blended through w 0.
  Each named draw's v0 bounds are kept and tested against the derived earlier matrix too.
GPU-VM 84a135a, fps_cap 240, 12 bursts of 30 renderer frames (XBE_CAPTURE_DIR, F3) while moving: the ropes whole and
continuous; the only rope pieces near the camera are the game's own (the same 12 bursts with smooth motion off show
them too, e.g. a black rope across the top when the camera sits behind the ropes). SDK fork fn2-hold-aspect 249a020.

### 2026-11-22 - Smooth motion and the ropes: a near-plane hold instead of special blending

Harry: get the ropes working. The earlier fixes (leave the ropes unblended, or move them with the camera alone) put
parts the game drew from one camera into frames drawn from another. Now the ropes blend like everything else, and a
step whose rope sections reach the near plane (or whose matches in the step before did) is shown as drawn, with no
frames in between: SDK GameInfo::interp_near_hold (fork fn2-hold-aspect dfc2bb1) checks each named draw's vertices
(v0 SHORT3/FLOAT3/FLOAT4 through the program's position matrix c0-c3, gpu::MatrixProgram) as it is recorded.
interp_unblended is gone. The rope program B0003131 reads v0 as SHORT3 (D3DVSDT 0x35), which the check needed.
- Headless CPU-vs-CPU fights: the nearest rope vertex w is 62 (never at the near plane), so no step held.
- GPU-VM 9750208, fps_cap 240, punching: 1,700-1,780 frames per 500 steps. 36 desktop shots and 177 consecutive frames
  from XBE_CAPTURE_DIR/F3 (6 runs of 30): the ropes whole in every one. No step held in that run (the camera did not
  pass through a rope); Harry's recording is what to compare against.

### 2026-11-22 - Smooth motion: the ring ropes, stopped after three attempts

Harry: the ropes are floppy above 60 fps (his 240 fps recording: rope sections near the camera swept across the screen
at wrong angles). The ropes are 49 draws alike (vertex program B0003131, texture 81985300, 653 indices), told apart
only by their matrices; the game rebuilds them each step. GPU-VM (Arc, fps_cap 240) runs a fight at 160 to 180 fps.
1. `interp_pieces` off (SDK GameInfo::interp_pieces): unmatched draws no longer take the camera's move. Headless the
   ropes stayed whole; Harry's recording still showed them flying.
2. The ropes unblended, drawn at the step's place (SDK GameInfo::interp_unblended). GPU-VM: near rope sections floated
   beside the ring, ending in mid-air: the step's camera against the in-between camera the rest is drawn from.
3. The ropes moved by the camera's move alone (interp_unblended draws derive `was` from the most-voted camera move of
   their target). GPU-VM 6e71dab: still floating near sections in front of the boxers.
Ruled out: mismatched sections (in-order and nearest-matrix matching agree on every sample), the "turning" path (0.15%
of rope matches). With smooth motion off the camera also passes close behind the ropes, but the sections are whole.
Thinking: the sections near the camera are clipped by the near plane; an in-between matrix (blended, or the camera's
move applied to the step's) puts their clipped ends somewhere the game never drew them. A fix probably means not
blending a frame whose camera is that close to a rope (hold the step's whole picture), or the camera's real position.
Smooth motion is still on by default; the ropes look right with it off.

### 2026-11-22 - Harry's run of 0c79d15; blurred sides beside the 16:9 menus

Harry: skip intro, modern buttons and mouse clicks in menus work; frame rate stays at 50 with smooth motion; the menus
stay 16:9. His log: a 280 Hz display, fps_cap 60, "frames planned 1.00" (one frame a 20 ms step fits under 60): smooth
motion needs a cap of 100 or more (the setting's help says so now). Menus: the frame is held at 16:9 because their
backgrounds are 16:9 art; Juiced's GameInfo::bars_backdrop (SDK 52e445f, cherry-picked onto fn2-hold-aspect as
65f976b, its local renamed from `small`, a Windows SDK macro, 6876345) shows a dimmed, blurred copy of the frame at
the sides while held (fn2::FrameHeld). GPU-VM e4b208a at 2.4:1: the main menu with blurred sides. SMOKE OK.

### 2026-11-22 - Ultrawide HUD no longer stretched; achievements stopped after five attempts

- **HUD stretched on ultrawide (fixed):** measured on GPU-VM, the fight's HUD box was 69x69 at 16:9 and 69x52 at 2.4:1:
  stretched 1.33 times across. It is an APT overlay movie drawn through EAGL's model drawer (0x98270 > 0x217F80, a 2D
  camera at 0x362028) with flat matrix programs, which the SDK squeezes only with `squeeze_screen_programs`, now on.
  GPU-VM 88f02aa: the box square (50x52), the HUD at its 16:9 place in the middle. A "HUD at the screen's edges" option
  was tried twice (State::hud_layout: skipped, the HUD draws are depth tested; moving shapes in 0x125280: the HUD is not
  drawn there, only the round card and letterbox) and dropped: it would mean moving the APT movie's own objects.
- **Achievements (stopped, 5 attempts):** need a fight's result and the player's boxer.
  1. Int/short/byte counters rising with 20 scripted punches: only sound slots (0x3453F0 table) and input buffers.
  2. The Fight Totals screen's numbers (Hopkins 14/6, Wright 14/10, haymakers 6/2 and 12/9) as a fingerprint in
     virtual and physical memory: no clean block (the totals look computed from a punch log).
  3. A judges' decision (39-37 on three cards, rounds 10,10,10,9 / 9,9,9,10) in either layout: no match but textures.
  4. APT functions FEPM_Result1-4 (0x98FF0 > 0xD3BE0): the memory card dialog's choices, not the fight.
  5. The crowd's fight messages, 0x1732D0 (thiscall: crowd, message; type at +4): 0 fight start, 1 round end, 2
     knockdown, 3-7 the outcome (C_FightDecision), 8 KO moment, 9 before the outcome. They name objects 0x82E85000 and
     0x82E85120 (globals 0x37FC8C, 0x37FC90, 0x37FC7C), but those are allocator blocks, not boxer records, so no winner.
  Useful for a next try: with no corner picked in Select Corner both boxers are the computer's and a fight plays to its
  end headless (four 1-minute rounds with the settings 0x37FFEC bits 18-23 cleared, about 8 minutes); with player 1 in
  a corner, Play Now offers training first (Focus, Quick, Skip). Career boxer records: wins +5, losses +6, draws +7, KOs
  +8 (0xAA300 saves them as nRecW/L/D/KO). Next: a write watch (gdb) on the Fight Totals' source when a punch lands.

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
