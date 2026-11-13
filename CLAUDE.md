# kanata-launch

macOS launcher that makes itself the "responsible process" for kanata, so the Input Monitoring and Accessibility grants sit on this binary instead of on kanata or tmux. See [README.md](README.md) for the full explanation.

## Files

| Path | Purpose |
| --- | --- |
| `kanata-launch.c` | The whole program: stage 1 re-execs with disclaim, stage 2 spawns kanata and waits |
| `Makefile` | Builds and ad-hoc signs the binary; kanata path, config path and signing identifier are make variables |
| `kanata-launch` | Build output, git-ignored |

## Rules

- **Do not rebuild casually.** The macOS privacy grant is tied to the binary's path and code hash. `make` after any source change produces a new hash, and the user then has to re-grant Input Monitoring and Accessibility by hand in System Settings. Ask before rebuilding a binary that is in use.
- **Do not move the built binary** for the same reason.
- **Keep it single-purpose.** It runs as root and holds keyboard grants. Do not add argument pass-through, environment-driven paths, or any way to launch something other than the compiled-in kanata binary and config.
- **Keep the child model.** Stage 2 must spawn kanata as a child and stay alive. Exec'ing kanata would make kanata its own responsible process again and defeat the purpose.
- `responsibility_spawnattrs_setdisclaim` is a private API with no header; the `extern` declaration in the source is intentional.

## Check the source without rebuilding

```sh
cc -Wall -Wextra -fsyntax-only \
  -DKANATA_BIN='"/opt/homebrew/bin/kanata"' -DKANATA_CFG='"/tmp/kanata.kbd"' \
  kanata-launch.c
```

## Testing

There are no unit tests; behaviour is verified against the macOS privacy log.

1. Run the launcher as root in place of kanata (only one kanata can hold the keyboard, so stop the live one first).
2. Read the log:

   ```sh
   /usr/bin/log show --info --last 1m --predicate 'process == "tccd"' \
     | grep -E 'Handling access request|responsible=' | grep -i kanata
   ```

3. `Sub:{...}` and `responsible_path=` should name `kanata-launch`, under both launchd and tmux.

Use `/usr/bin/log` explicitly; in zsh, `log` can resolve to something else.

Agents cannot enter a sudo password, so anything needing root must go through an existing passwordless rule or be run by the user.

## Verified so far

- Attribution lands on `kanata-launch` under launchd and under tmux (macOS 27, kanata 1.12.0).
- Not yet verified: the check returning allowed, and kanata grabbing the keyboard, once the launcher has been granted.