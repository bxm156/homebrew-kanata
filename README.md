# Signed kanata tap

A Homebrew tap that ships [kanata](https://github.com/jtroo/kanata) as a signed `Kanata.app` for Apple silicon Macs. A GitHub Actions workflow builds kanata from the upstream source, wraps it in an app bundle, signs it, and publishes it here; the cask installs it to `/Applications`.

## Why

kanata needs Input Monitoring and Accessibility. Homebrew's own kanata is an ad-hoc signed binary in a versioned directory, so macOS treats every new version as a different program and the permissions have to be granted again after each upgrade.

Here kanata is an app with a fixed bundle identifier (`com.bryanmarty.kanata`), signed by the same signer every time, at a path that does not change (`/Applications/Kanata.app`). macOS should then treat each new version as the same app, so the permissions are granted once.

Status: the workflow and cask are in place. That the permissions survive an upgrade has not been verified yet.

## Install

```sh
brew tap bxm156/kanata-launch https://github.com/bxm156/kanata-launch
brew install --cask bxm156/kanata-launch/kanata
```

kanata is a command-line program inside the bundle. Run it as:

```sh
/Applications/Kanata.app/Contents/MacOS/kanata -c ~/.config/kanata/kanata.kbd
```

Grant **Kanata** Input Monitoring and Accessibility in System Settings → Privacy & Security.

## Releasing a new kanata version

```sh
gh workflow run build-kanata.yml -R bxm156/kanata-launch -f version=1.13.0
```

When it finishes, `brew update && brew upgrade --cask kanata`, then restart kanata.

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