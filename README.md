# Signed kanata tap

A Homebrew tap for [kanata](https://github.com/jtroo/kanata) on Apple silicon Macs. A GitHub Actions workflow builds kanata from the upstream source, signs it, and publishes it here; the formula downloads that signed binary.

## Why

kanata needs Input Monitoring and Accessibility. Homebrew's own kanata is ad-hoc signed, so macOS treats every new version as a different program and the permissions have to be granted again after each upgrade.

Every build from this tap is signed with the same certificate and the same identifier (`com.bryanmarty.kanata`), and is installed at a path that does not change. macOS should then treat each new version as the same program, so the permissions are granted once.

Status: the build, signing and formula are in place. That the permissions survive an upgrade has not been verified yet.

## Install

```sh
brew tap bxm156/kanata-launch https://github.com/bxm156/kanata-launch
brew uninstall kanata    # if the Homebrew core version is installed
brew install bxm156/kanata-launch/kanata
```

Run kanata from `$(brew --prefix)/bin/kanata-signed`, and grant that file Input Monitoring and Accessibility in System Settings → Privacy & Security.

`kanata-signed` is a real file copied there on install and upgrade. The usual `bin/kanata` is a symlink into a versioned directory, which macOS resolves, so a grant on it would not survive an upgrade. Homebrew does not track `kanata-signed`; delete it by hand after `brew uninstall kanata`.

## Releasing a new kanata version

```sh
gh workflow run build-kanata.yml -R bxm156/kanata-launch -f version=1.13.0
```

When it finishes, `brew update && brew upgrade kanata`, then restart kanata.

## Signing

- Signed with [indygreg/apple-code-sign-action](https://github.com/marketplace/actions/apple-code-signing).
- The certificate is self-signed. Its public half is in `signing/kanata-signing-cert.pem`; the private key exists only in this repository's Actions secrets (`SIGNING_P12_BASE64`, `SIGNING_P12_PASSWORD`).
- To check a binary: `codesign -dvv "$(brew --prefix)/bin/kanata-signed"` should show `Identifier=com.bryanmarty.kanata` and `Authority=kanata-launch kanata signing`.

kanata is licensed LGPL-3.0-only; each release is built from the matching upstream tag.