# Fight Night Round 2 recomp - findings

What the work has established, by topic. PROGRESS.md has the dated log with the evidence for each item. Each item
is marked **[console]** (how the Xbox, its XDK or the runtime behaves: the fix is in the SDK) or **[game]** (this
game's own code and data, with its addresses).

## The image
- **[game]** One xbe, `default.xbe` (SHA-1 8edb378c6f3d9f984751c0a58060485bda189c76, 3,555,328 bytes), title 0x45410055
  "EA SPORTS FN 2", region flags 4 (PAL), built 2005-01-25, from the "Europe (En,Fr,De)" disc. Base 0x10000, entry
  0x2491C4, kernel thunk 0x2D4520.
- **[game]** Sections: .text, D3D, XGRPH, DSOUND, XPP (code); .rdata, .data, DOLBY, $$XTIMAGE, $$XSIMAGE, dspimage,
  .XTLID.
- **[game]** Libraries are all 1.0.5849 QFE 1 (XGRAPHC QFE 9): XAPILIB, XBOXKRNL, D3D8 (not LTCG), XGRAPHC, DSOUND,
  LIBCMT, LIBCPMT. No network libraries. EA code linked in: EAGL (renderer), APT (front end), VP6 (movies), AEMS
  (audio), Dolby.
- **[game]** The scan finds 130 of 278 functions and 10 of 13 data. By hand (config/fightnight2.toml):
  IDirectSoundBuffer_Pause 0x2C4374 (.XTLID 0xB0057) and D3D_PresentInterval 0x2BE72C (render state 127). The two
  DSound stream vtables stay unknown: DirectSoundCreateStream is not found either, and no stream call has trapped.

## Data
- **[game]** VIV4/BIGF archives with `.bh` headers (`render/boxers`, `render/enviro`, `render/actors`, `preload`,
  `feapt`), AEMS banks (`audio/aems`), music .bnk, speech .big/.csi/.evt, `locale/*.loc|euk|fre|ger`. The game opens
  optional `.bh` files that are not on the disc (fe\art\xfonts.bh, feapt\cafe.bh ...) and falls back to the .bin or
  .viv: harmless "missing" lines in the log.
- **[game]** Movies `fe/movies/*.nfx` (English) and `.fre`/`.ger`: EA VP6 in SCHl/MVhd containers, 640x480, decoded by
  the game's own code. They play headless with no SDK support.

## Front end and the smoke route
- **[game]** First boot: EA Sports logo movie, intro movie, title screen (START), an autosave question (A = Yes), main
  menu (Play Now, Create Champ, Game Modes, My Corner, EA Extras). Play Now: Select Corner, Select Boxer (twice;
  Hopkins and Wright by default), Select Venue (Staples Center), loading, walk-in (A skips), round 1.
- **[game]** Draws a frame headless: menus 45 to 130, Select Boxer about 255 (3D boxers), Select Venue about 67, the
  fight about 290 to 320.

## Rendering
- **[game]** Headless at 640x576 (PAL). No RunPushBuffer or CreatePushBuffer linked. EAGL writes raw commands between
  D3DDevice_BeginPush (0x2AF410) and EndPush (0x2AF430) at 9 sites from 0x210F20 to 0x22103C; the runtime's EndPush
  decoder runs them, with no stop or dropped primitive logged. MakeSpace (0x2AF8D0) is called only from the D3D
  library.
