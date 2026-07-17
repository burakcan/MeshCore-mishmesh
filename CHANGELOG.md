# Changelog

Notable changes to the mishmesh on-device UI. Versions are the mishmesh (mm)
version; each firmware also reports the meshcore (mc) base it was built on.

The release workflow reads the section matching the tag (`mishmesh-v<version>`)
into the GitHub Release notes, so keep the newest version at the top and start
its heading with `## v<version>`.

## Unreleased

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
