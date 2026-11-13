# kanata-launch

A tiny macOS launcher for [kanata](https://github.com/jtroo/kanata) that holds the Input Monitoring and Accessibility grants itself, so that:

- `brew upgrade kanata` does not require re-granting permissions, and
- kanata can run inside tmux without giving tmux keyboard access.

It is about 100 lines of C with no dependencies.

## Status

Experimental.

- **Verified:** under both launchd and tmux, macOS evaluates kanata's Input Monitoring check against `kanata-launch` (confirmed in the `tccd` log on macOS 27 with kanata 1.12.0).
- **Not yet verified:** kanata actually grabbing the keyboard once `kanata-launch` has been granted the permissions.

## The problem

macOS does not check keyboard permissions against the process that asks for access. It checks that process's *responsible process*, which is normally inherited from whatever started it.

| How kanata is started | Responsible process | Consequence |
| --- | --- | --- |
| launchd → sudo → kanata | kanata itself | The grant is tied to the versioned Homebrew binary, so every upgrade needs a re-grant |
| tmux → sudo → kanata | tmux | Fails unless tmux is granted keyboard access |
| anything → sudo → kanata-launch → kanata | kanata-launch | One grant, on a binary that rarely changes |

## How it works

1. **Stage 1.** `kanata-launch` re-execs itself with the "disclaim responsibility" spawn attribute. That makes it its own responsible process regardless of who started it.
2. **Stage 2.** It starts kanata as a normal child process and stays alive. kanata inherits `kanata-launch` as its responsible process, so the privacy checks land on the launcher.

It also forwards `SIGTERM`, `SIGINT` and `SIGHUP` to kanata and exits with kanata's exit status, so launchd `KeepAlive` and restarts behave as they would with kanata alone.

## Build

Requires the Xcode command line tools.

```sh
make
```

The kanata path and config path are compiled in. Override the defaults if yours differ:

```sh
make KANATA_BIN=/opt/homebrew/bin/kanata \
     KANATA_CFG="$HOME/.config/kanata/kanata.kbd" \
     SIGN_ID=com.example.kanata-launch
```

## Use

1. Run `kanata-launch` as root wherever you currently run kanata, for example from the script your LaunchAgent or sudoers rule points at:

   ```sh
   #!/bin/sh
   exec /path/to/kanata-launch
   ```

2. In System Settings → Privacy & Security, enable `kanata-launch` under both **Input Monitoring** and **Accessibility**. Use the `+` button to add the binary if it is not listed.
3. Restart kanata.

`kanata-launch` takes no arguments; anything passed to it is ignored.

## Checking which process macOS blames

```sh
/usr/bin/log show --info --last 1m --predicate 'process == "tccd"' \
  | grep -E 'Handling access request|responsible=' | grep -i kanata
```

Look for `Sub:{...}` and `responsible_path=`. They should name `kanata-launch`.

## Caveats

- **Private API.** `responsibility_spawnattrs_setdisclaim` is an undocumented libSystem function. It has been stable for years but could change in a macOS update.
- **Rebuilding or moving the binary invalidates the grant.** The grant is tied to the binary's path and code hash (it is ad-hoc signed). Re-grant after either.
- **Runs as root.** The launcher starts whatever is at the compiled-in kanata path. If that path, the config, or the launcher itself is writable by your user, anything running as your user can get root and keyboard access through it. Keep them root-owned if that matters to you.
- **macOS only.** Linux has no equivalent of the responsible-process check.