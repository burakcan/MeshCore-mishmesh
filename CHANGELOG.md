# Changelog

Notable changes to the mishmesh on-device UI. Versions are the mishmesh (mm)
version; each firmware also reports the meshcore (mc) base it was built on.

The release workflow reads the section matching the tag (`mishmesh-v<version>`)
into the GitHub Release notes, so keep the newest version at the top and start
its heading with `## v<version>`.

## Unreleased

## v1.3.0

**Now built on meshcore v1.17.1** (was v1.16.0)

- Active repeater/sensor discovery, from the Discover tab in Contacts.
- Repeat alert: a chat's notification tone replays while a message is unread.
- System Info now shows the MCU temperature, on boards that report it.
- Fix: contact rows could show the wrong node, or drop the last few contacts.
- Fix: a bug causing failed retries to never drop and retry indefinitely

## v1.2.0

- Pomodoro timer added to the Clock app.
- "Wake screen on message" configuration (global + per-chat override).
- Battery settings panel with battery display style (gauge, percentage, voltage) and ADC calibration.
- Screen brightness control (thanks @Bjorkan).

## v1.1.0

- Configurable path hash size, in Experimental settings.
- System Info now shows the node's public key.
- Piezo buzzer tone tuning.
- Release firmware filenames now encode both versions:
  `<env>_mm-<mm>_mc-<mc>-<hash>`.
- Fix: empty preference file no longer causes a load error.
- Fix: multibyte path hashes are now stored and displayed correctly.

## v1.0.0

- Initial release
