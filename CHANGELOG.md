# Changelog

## 0.0.2a (alpha)
- Adds the `winmm` device-count clamp: `mixerGetNumDevs`, `waveOutGetNumDevs` and `waveInGetNumDevs` never report more than N devices. N comes from `dsound_proxy.cfg` (default 16).
- Keeps the DirectSound pass-through and the `dsound_proxy.log` log.
- Verified on one machine only: the mixer count was reported as 28 and clamped to 16, and the game reached the menu.

## 0.0.1a (internal, not released)
- Logging-only DirectSound proxy used to diagnose the problem. It showed the game never enumerates DirectSound devices and creates only the default one.
