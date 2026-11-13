# Signed kanata tap

Homebrew tap that ships kanata built from upstream source and signed in CI, so macOS privacy permissions survive upgrades. See [README.md](README.md).

## Files

| Path | Purpose |
| --- | --- |
| `.github/workflows/build-kanata.yml` | Manual workflow: build kanata from an upstream tag, sign it, publish a release, update the formula |
| `Formula/kanata.rb` | Installs the signed release and copies it to `$(brew --prefix)/bin/kanata-signed`. The workflow rewrites its `url`, `version` and `sha256` lines |
| `signing/kanata-signing-cert.pem` | Public half of the self-signed signing certificate |

## Rules

- **Keep it this small.** The owner wants a plain tap built from existing tools. Do not add launchers, generator scripts or extra verification steps.
- **Do not change the signing identifier** (`com.bryanmarty.kanata` in the workflow), the certificate, or the installed path (`bin/kanata-signed`). macOS matches the privacy grant against all three; changing any of them forces the owner to re-grant by hand.
- **Do not re-sign kanata in the formula.** An ad-hoc re-sign would break the grant.
- **Keep the workflow manual-trigger only** and never echo the secrets. The private key exists only in the Actions secrets and cannot be read back; replacing it means a new certificate and a re-grant.
- Keep `url`, `version` and `sha256` each on their own line in the formula, indented two spaces: the workflow updates them with `sed`.
- The workflow commits to `main`, so pull before making local changes.

## Not yet verified

That a grant on `bin/kanata-signed` survives an upgrade. Test by granting it, running the workflow again, upgrading, and checking that kanata still grabs the keyboard.