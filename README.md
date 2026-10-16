# BO3 - Windows Audio Black Screen FIX

**Short name: BO3-WABSF**

**Alpha (v0.0.2a)** · by [BrunoBVM](https://github.com/BrunoBVM) · MIT license

🇧🇷 [Leia em Português (Brasil)](README.pt-BR.md)

A possible workaround for the **Call of Duty: Black Ops III (Steam, PC)** black screen and freeze at startup when Windows has many audio endpoints (for example VoiceMeeter or VB-Cable virtual devices). It limits how many devices the game sees. It does not change any Windows audio setting.

> **Status:** works on the author's machine. Not verified anywhere else. Not affiliated with Activision, Treyarch, Valve or VB-Audio. Use at your own risk, and read [Safety and anti-cheat](#safety-and-anti-cheat) first.

## The problem

With many active audio endpoints the game starts, plays the intro video's audio, shows a black screen, and stops responding once the video ends. Disabling audio devices in Windows is the known workaround, but that is not always possible, for example on a two-PC stream setup where the virtual devices carry the audio routing.

## What this does

A small x64 `dsound.dll` placed next to `BlackOps3.exe`. The game imports DirectSound, so Windows loads this copy first. It:

1. forwards every DirectSound call to the real `dsound.dll` and logs the calls to `dsound_proxy.log`;
2. patches the game's own import slots for three `winmm` functions, `mixerGetNumDevs`, `waveOutGetNumDevs` and `waveInGetNumDevs`, so they never report more than N devices (default 16).

Nothing in Windows is modified, and other programs still see all your devices.

## What was found (single machine, treat as a lead, not a proof)

- A Process Monitor stack showed `BlackOps3.exe` calling `winmmbase!mixerGetNumDevs`, which makes `MMDevAPI` walk every audio endpoint in the registry, including not-present ones.
- A hang dump of the frozen process showed the main thread in a Sleep loop and no thread blocked inside an audio call.
- After the intro video the game re-read one sound bank (`core_patch.all.sabs`) in 256 KB chunks, roughly one chunk every 1.5 seconds, for minutes without finishing.
- With the clamp active the game launched normally. The log showed:
  `mixerGetNumDevs real=28 -> returned 16` and `waveInGetNumDevs real=13 -> returned 13`.
  Only the mixer count was actually reduced, so that call appears to be the one that matters.
- **Unknown:** the exact threshold, and what the game does with the count internally.

## Install

### Option A: download the prebuilt DLL (easiest)

1. Download `dsound.dll` from the [Releases page](https://github.com/BrunoBVM/BO3-WABSF/releases). Releases are marked as pre-release (alpha).
2. Recommended: check the file against the SHA-256 listed in the release notes. For v0.0.2a it is `971001F82716D9739C58EE3FE451CAB3E3565AA04492E6553FDA5871033FAC88`. In PowerShell:
   ```
   Get-FileHash .\dsound.dll -Algorithm SHA256
   ```
3. Copy `dsound.dll` into the Black Ops III folder, next to `BlackOps3.exe`.

The DLL is unsigned and built by the author with MinGW-w64. A DLL in the game folder runs with the game's permissions, and some antivirus programs flag proxy DLLs. If you would rather not trust a prebuilt binary, use Option B.

### Option B: build from source

MinGW-w64 (x64): run `build-mingw.cmd`, or

```
g++ -shared -O2 -static -o dsound.dll src/dsound.cpp src/dsound.def
```

MSVC (x64 Native Tools prompt):

```
cl /LD /O2 /EHsc src\dsound.cpp /link /DEF:src\dsound.def /OUT:dsound.dll
```

The game is 64-bit, so build a 64-bit DLL. Then copy `dsound.dll` next to `BlackOps3.exe`.

#### Installing a compiler (pick one)

**MinGW-w64 (smaller download, recommended)**

1. Open PowerShell and run:
   ```
   winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
   ```
   If winget cannot find that ID, run `winget search winlibs` and use the ID it lists.
2. Close the terminal and open a new one so `g++` is on your PATH.
3. Check that it works and is 64-bit:
   ```
   g++ --version
   g++ -dumpmachine
   ```
   The second command should print `x86_64-w64-mingw32`. If it prints `i686`, you installed a 32-bit toolchain and the DLL will not load in the game.

Alternative: install [MSYS2](https://www.msys2.org/), open the "MSYS2 UCRT64" terminal, run `pacman -S mingw-w64-ucrt-x86_64-gcc`, and add `C:\msys64\ucrt64\bin` to your PATH.

**MSVC (Visual Studio Build Tools, a multi-gigabyte download)**

1. Download "Build Tools for Visual Studio" from the [Visual Studio downloads page](https://visualstudio.microsoft.com/downloads/) (under "Tools for Visual Studio").
2. In the installer, tick the **Desktop development with C++** workload and install it.
3. Open **x64 Native Tools Command Prompt for VS** from the Start menu. A normal terminal will say `'cl' is not recognized`.
4. `cd` into the project folder and run the `cl` command shown above.

### After installing

1. Optional: create `dsound_proxy.cfg` in the game folder containing one number, the maximum device count to report (default 16, `0` reports none).
2. Start the game. Check `dsound_proxy.log` for a line like `clamp cap=16; import slots patched: ...`. If every patched count is 0, the game does not call those functions through its import table on your setup, and this approach will not help.
3. To remove it, delete `dsound.dll`, `dsound_proxy.cfg` and `dsound_proxy.log`.

`scripts/launch-bo3.ps1` installs the proxy only for one session and removes it after the game exits. It looks for `dsound.dll` in the repo root by default, or pass `-ProxyDll <path>` to use the downloaded file. It is untested beyond a basic run, so read it first.

## Safety and anti-cheat

This is not a cheat and does not touch gameplay, but it does place a DLL in the game folder and patches the game's import table in memory. Treat that as a real risk.

- Black Ops III has its own Treyarch anti-cheat (TAC). Treyarch's published [Security & Enforcement Policy](https://support.activision.com/call-of-duty--black-ops-iii/articles/call-of-duty-black-ops-iii-security-enforcement-policy) penalizes unauthorized third-party software used to gain an advantage or manipulate stats or game data. It lists injectors and "any other software used to deliberately modify game data on disk or in memory" as examples. This project does none of the first things, but by the letter of that last wording an in-memory patch could be read as covered, and Treyarch does not publish how TAC detects anything.
- Sources disagree on whether Valve Anti-Cheat also applies to BO3 on Steam. A third-party mirror of the Steam store listing shows "Valve Anti-Cheat enabled", while forum posts describe TAC as the active system. Valve's own [VAC FAQ](https://help.steampowered.com/faqs/view/647C-5CC1-7EA9-3C29) says VAC bans come from connecting to VAC-secured servers while running cheat software.
- **The author's view:** Activision does not seem to care much about this game anymore. The anti-cheat is old, mods and cheaters are everywhere, and many security holes were never patched, so people are probably safe using this. That is an opinion, not a guarantee. The policy above still applies, penalties are final, and nobody outside Treyarch knows what TAC detects.
- This project is intended for offline, solo and LAN play. If you need certainty, ask Activision Support before using it.
- The proxy only adds files to the game folder; it does not modify any existing game file.

## Reporting results

Useful information if you try it: your Windows build, the `real=` lines from `dsound_proxy.log`, the number of active playback and recording devices, and whether the game reached the menu. Different caps (for example 24 or 28) would help find where the threshold is.

## Versions

See [CHANGELOG.md](CHANGELOG.md). Current release: **v0.0.2a (alpha)**.

## License

MIT, copyright (c) 2026 BrunoBVM. See `LICENSE`.

PS. CLAUDE DID ALL THE WORK, I JUST WANTED TO PLAY THE GAME LOL
