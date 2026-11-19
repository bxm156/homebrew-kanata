# Signed kanata tap

A Homebrew tap that ships [kanata](https://github.com/jtroo/kanata) as a signed `Kanata.app` for Apple silicon Macs. A GitHub Actions workflow builds kanata from the upstream source, wraps it in an app bundle, signs it, and publishes it here; the cask installs it to `/Applications`.

## Why

kanata needs Input Monitoring and Accessibility. Homebrew's own kanata is an ad-hoc signed binary in a versioned directory, so macOS treats every new version as a different program and the permissions have to be granted again after each upgrade.

Here kanata is an app with a fixed bundle identifier (`com.bryanmarty.kanata`), signed by the same signer every time, at a path that does not change (`/Applications/Kanata.app`). macOS should then treat each new version as the same app, so the permissions are granted once.

Status: the workflow and cask are in place. That the permissions survive an upgrade has not been verified yet.

## Install

```sh
brew tap bxm156/kanata
brew install --cask bxm156/kanata/kanata
```

kanata is a command-line program inside the bundle. Run it as:

```sh
/Applications/Kanata.app/Contents/MacOS/kanata -c ~/.config/kanata/kanata.kbd
```

Grant **Kanata** Input Monitoring and Accessibility in System Settings → Privacy & Security.

## Restart kanata when a keyboard is plugged in

kanata does not grab a keyboard that connects after it has started, so a replugged keyboard types unmapped until kanata is restarted. The bundle includes a helper, `kanata-restart-on-attach`, that restarts a launchd service; launchd runs it whenever a USB keyboard appears.

If kanata runs as a LaunchAgent labelled `com.kanata`, save this as `~/Library/LaunchAgents/com.kanata.restart-on-attach.plist`, replacing `501` with your user ID (`id -u`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.kanata.restart-on-attach</string>
  <key>ProgramArguments</key>
  <array>
    <string>/Applications/Kanata.app/Contents/MacOS/kanata-restart-on-attach</string>
    <string>gui/501/com.kanata</string>
  </array>
  <key>LaunchEvents</key>
  <dict>
    <key>com.apple.iokit.matching</key>
    <dict>
      <key>usb-keyboard-attached</key>
      <dict>
        <key>IOProviderClass</key>
        <string>IOHIDDevice</string>
        <key>Transport</key>
        <string>USB</string>
        <key>PrimaryUsagePage</key>
        <integer>1</integer>
        <key>PrimaryUsage</key>
        <integer>6</integer>
        <key>IOMatchLaunchStream</key>
        <true/>
      </dict>
    </dict>
  </dict>
</dict>
</plist>
```

Load it with `launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.kanata.restart-on-attach.plist`.

It restarts kanata about two seconds after any USB keyboard appears, and once when it is loaded for the keyboards already attached. Keep `Transport` set to `USB`: kanata's own virtual keyboard has no transport, so restarting kanata does not trigger another restart. If kanata runs as a LaunchDaemon, install the job in `/Library/LaunchDaemons` and pass `system/<label>` instead.

## New kanata versions

The workflow runs once a day. It uses `brew livecheck` and the cask's `livecheck` block to compare the latest upstream kanata release (pre-releases are ignored) with the version in the cask and, if upstream is newer, builds, signs and publishes it. Scheduled builds are signed but not notarized.

On your Mac, pick up a new build with `brew update && brew upgrade --cask kanata`, then restart kanata.

To build by hand, for the latest release or a specific version:

```sh
gh workflow run build-kanata.yml -R bxm156/homebrew-kanata
gh workflow run build-kanata.yml -R bxm156/homebrew-kanata -f version=1.13.0
```

GitHub pauses scheduled workflows in a repository with no activity for 60 days; re-enable it from the Actions tab if that happens.

## Signing

The workflow signs with [indygreg/apple-code-sign-action](https://github.com/marketplace/actions/apple-code-signing) and, by default, notarizes and staples the app so Gatekeeper accepts it. It reads these repository secrets:

| Secret | What it is |
| --- | --- |
| `SIGNING_P12_BASE64` | Developer ID Application certificate with its private key, exported as `.p12` and base64-encoded |
| `SIGNING_P12_PASSWORD` | Password of that `.p12` |
| `ASC_KEY_ID` | App Store Connect API key ID |
| `ASC_ISSUER_ID` | App Store Connect API issuer ID |
| `ASC_PRIVATE_KEY` | Contents of the `AuthKey_<id>.p8` file for that key |

The three `ASC_*` secrets are only needed for notarization. Run the workflow with `notarize=false` to sign without it.

To check an installed copy: `codesign -dvv /Applications/Kanata.app`.

kanata is licensed LGPL-3.0-only; each release is built from the matching upstream tag, and the licence is included in the bundle.