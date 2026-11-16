# PROJECT NOISE

**A new native Nintendo DSi homebrew music player.** This is a clean DSi-targeted codebase. The supplied 3DS project was used only as visual reference material; its 3DS application code is not used by this DSi build.

## Visual direction

The boot sequence is adapted from the supplied PROJECT NOISE boot artwork and reduced to the DSi's native 256×192 screen size. The player UI uses the same black / electric-violet palette, crown identity, sharp cover framing, and compact music-device layout. The top screen is a compact Cover Flow / Now Playing view; the bottom screen combines the library, search, liked songs, queue, and a touch-functional iPod-style click wheel.

## Build

The project targets **BlocksDS 1.25+** and the Wonderful Toolchain. It uses the official `skylyrac/blocksds:slim-latest` Docker image in GitHub Actions and builds fixed-point ARM9 **libmad 0.15.1b** from its upstream release tarball.

Required BlocksDS packages:

- `blocksds-toolchain` / the BlocksDS slim container
- `blocksds-libcurl`
- `blocksds-mbedtls`

Build locally from a BlocksDS shell:

```sh
bash tools/build_libmad.sh
make
```

The output is `PROJECT_NOISE_DSi.nds`. A GitHub Actions workflow is included at `.github/workflows/build.yml`; push this folder to a GitHub repository and run **Build PROJECT NOISE DSi** to obtain the NDS artifact.

## Installation

1. Upload the **contents** of this project folder to a GitHub repository (not just the ZIP as one file).
2. Open **Actions → Build PROJECT NOISE DSi → Run workflow**.
3. When the job succeeds, download the `PROJECT-NOISE-DSi` artifact. It contains `PROJECT_NOISE_DSi.nds`.
4. Copy the `.nds` to the DSi SD card, for example `/roms/apps/PROJECT_NOISE_DSi.nds`.
5. Launch it in **DSi mode**, not DS compatibility mode, if your launcher offers that choice. This is required for WPA2 DSi-mode networking.
6. Put MP3 files in `/Music/` and launch PROJECT NOISE.

The source package in this handoff does **not** contain a compiled `.nds`; the build artifact only exists after the GitHub Actions job succeeds. If the build fails, the Actions log is the actual compiler output and must be fixed before installing.

## SD card setup

Create this folder on the DSi SD card and put MP3 files inside it:

```text
/Music/
```

The app stores settings, liked songs, queue state, and cached lyrics here:

```text
/3ds/PROJECT_NOISE/
```

The HTTPS CA bundle is embedded in NitroFS so LRCLIB TLS verification does not depend on disabling certificate checks. The app uses `cothread_yield_irq(IRQ_VBLANK)` in its frame loop so DSWiFi/lwIP can process network work while audio and UI continue.

## Controls

| Control | Action |
|---|---|
| L | Previous track |
| R | Next track / next queued track |
| A | Play selected track / resume |
| B | Pause |
| Y | Like or unlike the playing track |
| X | Add playing track to queue |
| Double X | Insert playing track at the front of the queue (play next) |
| D-pad Up / Down | Move library or settings selection |
| D-pad Left / Right | Move the search caret while search is active; cycle Library / Liked / Queue otherwise |
| SELECT | Open / close settings |
| START | Exit search and return to the full library |
| Touch | Select tabs, enter search, tap tracks, and use the player controls |

The single-X action is delayed briefly to distinguish it from a double press. It never replaces the currently playing track. The bottom touchscreen click wheel is also active: top opens search, left/right skip tracks, bottom likes the current track, and center plays/pauses.

## Current implementation

- Native dual-screen 256×192 bitmap UI, custom compact bitmap font, and a touch-functional iPod-style click wheel with animated Cover Flow-style artwork transitions.
- Adapted two-screen boot artwork and branded cover placeholder.
- Recursive MP3 library scan from `/Music`.
- ID3v2 text frames and ID3v1 fallback for title, artist, and album; filename fallback for damaged or missing tags.
- Library search by title, artist, and path; sort by title, artist, or album.
- Persistent liked songs, queue, and settings, including accent theme, sort order, offline lyrics mode, and audio volume.
- Exact physical controls listed above, including X double-press handling.
- MP3 decoding pipeline using libmad 0.15.1b built for ARM9 `FPM_ARM`, its low-level frame API, linear resampling to a Maxmod-supported rate, and Maxmod's documented PCM streaming callback / circular-buffer model. File reads and frame decoding stay out of Maxmod's interrupt callback.
- Local `.lrc` loading and persistent lyric cache.
- LRCLIB exact lookup, fallback search, URL encoding, identifying User-Agent, asynchronous libcurl multi requests, HTTPS certificate verification, and HTTP 429 `Retry-After` backoff. Multiple search matches are shown in an on-device picker and the selected result is fetched through `/api/get/{id}`.
- Synced LRC timestamps are used for line changes. Plain lyrics are displayed as plain text and are **not** assigned invented timestamps.
- DSi-mode Wi-Fi initialization requests WPA2-capable DSi mode and uses saved firmware access-point settings. BlocksDS documents that WPA2 requires DSi mode; ordinary DS-compatible Wi-Fi is limited to open/WEP networks. <https://blocksds.skylyrac.net/tutorial/advanced/wifi/>

## Not yet verified / incomplete

This source package is an implementation milestone, **not a claim of a finished, hardware-verified release**.

- This environment does not contain BlocksDS / arm-none-eabi-gcc / ndstool, so an `.nds` binary has not been built here. The included CI workflow is the build route.
- No physical DSi is attached to this environment. MP3 playback quality, audio underruns, Wi-Fi association, TLS handshake, and control timing still require a real DSi test. libmad 0.15.1b is a mature but old decoder; malformed-file fuzzing and long-session playback tests remain required. BlocksDS currently labels its Mbed TLS 3.6.4 package as a prototype with an entropy-generation issue; lyrics requests are read-only and send no credentials, but this TLS stack should not be treated as suitable for security-critical traffic.
- The current Cover Flow artwork is a branded placeholder. ID3 APIC/JPEG/PNG album-art extraction is not implemented yet.
- Search entry uses the touchscreen keyboard; the D-pad left/right keys move the caret. The DSi's lack of a physical text keyboard makes this a deliberate touch-first entry method.
- Playback-position resume, seek controls, playlists, ID3 embedded album-art extraction, a full background theme system, and a visualizer are not implemented yet. The current theme setting changes the accent palette.
- Lyric transitions are lightweight and timestamp-driven; they are not a heavy GPU-style animation.

Do not describe any unverified item above as tested. The next required gate is a successful BlocksDS CI build, followed by emulator smoke tests and physical DSi playback/network tests.

## References

- BlocksDS SDK and libnds: https://blocksds.skylyrac.net/
- DSXP, used only to cross-check that libmad + Maxmod is a viable DS/DSi MP3 architecture; no DSXP application source is copied: https://github.com/Farzin-Abdi/DSXP
- BlocksDS Maxmod streaming example: https://raw.githubusercontent.com/blocksds/sdk/master/examples/maxmod/streaming/source/main.c
- BlocksDS DSi Wi-Fi / HTTPS guide: https://blocksds.skylyrac.net/tutorial/advanced/wifi/
- LRCLIB API docs: https://lrclib.net/docs
- libmad 0.15.1b: https://sourceforge.net/projects/mad/files/libmad/0.15.1b/ (GPL-2.0-or-later)
