# Signed kanata tap

Homebrew tap that ships kanata as a signed `Kanata.app`, built from upstream source in CI, so macOS privacy permissions survive upgrades. See [README.md](README.md).

## Files

| Path | Purpose |
| --- | --- |
| `.github/workflows/build-kanata.yml` | Daily (and manual) workflow: if upstream has a newer kanata than the cask, build it from the upstream tag, assemble and sign `Kanata.app`, publish a release, update the cask |
| `app/Info.plist` | Bundle metadata copied into `Kanata.app`. Holds the bundle identifier |
| `Casks/kanata.rb` | Installs the release to `/Applications/Kanata.app`. The workflow rewrites its `version` and `sha256` lines |

## Rules

- **Keep it this small.** The owner wants a plain tap built from existing tools. Do not add launchers, generator scripts or extra verification steps.
- **Do not change the bundle identifier** (`com.bryanmarty.kanata` in `app/Info.plist`), the app name, or the signing certificate. macOS matches the privacy grant against the identifier and signer; changing either forces the owner to re-grant by hand.
- **The workflow runs on a daily schedule and by hand, nothing else.** Never add `pull_request` or `push` triggers (they would run untrusted changes next to the signing secrets), and never echo the secrets. The owner chose the schedule knowing it signs whatever upstream releases without review.
- Keep `version` and `sha256` each on their own line in the cask, indented two spaces: the workflow updates them with `sed`. The `url` is derived from `version`.
- The workflow commits to `main`, so pull before making local changes.

## Not yet verified

- Signing with a Developer ID, notarization and stapling. Only a self-signed, non-notarized test build has been run.
- That a grant on `Kanata.app` survives an upgrade (tracked in issue #1).
- A scheduled run that actually finds a newer version and builds it.

Verified on the owner's Mac (2026-11-13): macOS attributes kanata, started as root from launchd, to the `Kanata.app` bundle, and a self-signed build runs once its quarantine flag is cleared.