# Changelog

Notable changes to the mishmesh on-device UI. Versions are the mishmesh (mm)
version; each firmware also reports the meshcore (mc) base it was built on.

The release workflow reads the section matching the tag (`mishmesh-v<version>`)
into the GitHub Release notes, so keep the newest version at the top and start
its heading with `## v<version>`.

## Unreleased

## v1.5.0

- E-ink support - the Wio Tracker L1 E-Ink is now a build target.
- Portrait orientation and a Large interface size on the e-ink panel.
- New Display settings group; screen sleep and brightness moved there from Home.
- A single Back at the home screen sleeps the display; three in quick succession still lock it.
- Sleep screen on e-ink: leave a clock or the logo on the panel while it sleeps.
- Return to home when the screen has been asleep a while, configurable.
- Fix: onboarding not triggering on a new device.

## v1.4.1

- Fix: messages could go missing from a chat in some cases.
- Affected devices repair themselves on the next boot after update.

## v1.4.0

- Cyrillic rendering and keyboard support for Ukrainian, Russian, Belarussian, Bulgarian, Serbian and Macedonian languages. Thanks to @nxstd for the base of this work.
- Option to open chats from the oldest unread message

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
