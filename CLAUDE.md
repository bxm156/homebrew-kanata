# kanata-launch

macOS launcher that makes itself the "responsible process" for kanata, so the Input Monitoring and Accessibility grants sit on this binary instead of on kanata or tmux. It only starts a kanata binary signed by this repository's CI. See [README.md](README.md) for the full explanation.

## Direction change (in progress)

The launcher is being retired in favour of relying on kanata's own signature: CI signs every build with the same self-signed certificate and identifier, and kanata runs from a fixed path (`$(brew --prefix)/var/kanata/kanata-signed`, copied there by the formula's `post_install`). See the README section of the same name.

- macOS matches the privacy grant against the identifier (`SIGN_IDENTIFIER` in the workflow, `com.bryanmarty.kanata`), the signing certificate (`signing/kanata-signing-cert.pem`), and the binary's path. Changing any of the three forces a re-grant.
- The owner considered a Developer ID certificate and chose the self-signed one.
- A rebuild of the same kanata version uses the workflow's `revision` input, which produces tag `kanata-v<version>_<n>` and a `revision` line in the formula.
- Do not remove the launcher, `signing/`, or the launcher sections of the docs until the owner has confirmed that the grant survives an upgrade. Until then the launcher sections below describe the previous design.

## Files

| Path | Purpose |
| --- | --- |
| `kanata-launch.c` | The launcher: stage 1 re-execs with disclaim; stage 2 copies kanata to a root-only directory, verifies its signature, spawns it and waits |
| `Makefile` | Builds and ad-hoc signs the launcher (hardened runtime). `make check` tests the installed kanata's signature |
| `signing/kanata-signing-cert.pem` | Public half of the kanata signing certificate. Its SHA-1 is compiled into the launcher |
| `.github/workflows/build-kanata.yml` | Manual workflow: build kanata from an upstream tag, sign it, publish a release, regenerate the formula |
| `scripts/update-formula.sh` | Writes `Formula/kanata.rb`; the formula template lives here |
| `Formula/kanata.rb` | Generated. Installs the signed release. This repo doubles as a Homebrew tap |
| `kanata-launch` | Build output, git-ignored |

## How the pieces must agree

The launcher accepts kanata only if it satisfies:

```text
identifier "<KANATA_SIGN_ID>" and certificate leaf = H"<sha1 of signing/kanata-signing-cert.pem>"
```

- `KANATA_SIGN_ID` in the `Makefile` must equal `SIGN_IDENTIFIER` in the workflow.
- The certificate in `signing/` must be the public half of the key stored in the `SIGNING_P12_BASE64` secret. The workflow selects the signing identity by the committed certificate's hash, so a mismatch fails the workflow at the signing step.
- The private key exists only in GitHub Actions secrets (`SIGNING_P12_BASE64`, `SIGNING_P12_PASSWORD`). It was deleted locally after upload and cannot be read back. Losing or rotating it means a new certificate, a launcher rebuild, and a re-grant.

## Rules

- **Do not rebuild the launcher casually.** The macOS privacy grant is tied to the binary's path and code hash. `make` after any source or certificate change produces a new hash, and the user then has to re-grant Input Monitoring and Accessibility by hand in System Settings. Ask before rebuilding a binary that is in use.
- **Do not move the built binary** for the same reason.
- **Keep it single-purpose.** It runs as root and holds keyboard grants. Do not add argument pass-through, environment-driven paths, or any way to launch something other than the compiled-in kanata binary and config.
- **Keep the child model.** Stage 2 must spawn kanata as a child and stay alive. Exec'ing kanata would make kanata its own responsible process again and defeat the purpose.
- **Verify the private copy, not the original path.** The copy-then-verify-then-exec order is what prevents a swap after the check.
- **Never add `pull_request` or `push` triggers to the signing workflow**, and never echo the secrets. It is manual-trigger only so that secrets stay away from untrusted code and a person chooses what gets signed.
- **Do not re-sign kanata in the formula.** The launcher only accepts the CI signature; an ad-hoc re-sign would make it refuse to start.
- **Never overwrite a published release.** The formula pins its SHA-256.
- `responsibility_spawnattrs_setdisclaim` is a private API with no header; the `extern` declaration in the source is intentional.
- Edit the formula template in `scripts/update-formula.sh`, not `Formula/kanata.rb`.

## Check the source without rebuilding

```sh
cc -Wall -Wextra -fsyntax-only \
  -DKANATA_BIN='"/opt/homebrew/bin/kanata"' -DKANATA_CFG='"/tmp/kanata.kbd"' \
  -DRUN_DIR='"/var/db/kanata-launch"' \
  -DKANATA_SIGN_ID='"com.example.kanata"' -DKANATA_CERT_SHA1='"00"' \
  kanata-launch.c
```

## Testing

There are no unit tests.

- **Signature check, no root needed:** run `./kanata-launch` as a normal user. It verifies the installed kanata and stops, printing either a refusal or "has a trusted signature, but the launcher must run as root". `make check` does the same with `codesign`.
- **Responsible process:** run the launcher as root in place of kanata (only one kanata can hold the keyboard, so stop the live one first), then read the privacy log:

  ```sh
  /usr/bin/log show --info --last 1m --predicate 'process == "tccd"' \
    | grep -E 'Handling access request|responsible=' | grep -i kanata
  ```

  `Sub:{...}` and `responsible_path=` should name `kanata-launch`, under both launchd and tmux.

Use `/usr/bin/log` explicitly; in zsh, `log` can resolve to something else.

Agents cannot enter a sudo password, so anything needing root must go through an existing passwordless rule or be run by the user.

## Releasing a new kanata

```sh
gh workflow run build-kanata.yml -f version=1.13.0
gh run watch
```

The workflow commits the regenerated formula to `main`, so pull before making further local changes.

## Verified so far

- Attribution lands on `kanata-launch` under launchd and under tmux (macOS 27, kanata 1.12.0).
- The launcher refuses an ad-hoc signed kanata.
- Not yet verified: the privacy check returning allowed, and kanata grabbing the keyboard, once the launcher has been granted.