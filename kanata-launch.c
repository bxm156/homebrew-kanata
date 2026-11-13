// kanata-launch: a small, stable launcher that owns the macOS privacy grants
// for kanata (https://github.com/jtroo/kanata).
//
// Background
// ----------
// macOS does not check Input Monitoring / Accessibility against the process
// that asks for keyboard access. It checks the "responsible process", which
// is normally inherited from whatever started the process. Two consequences:
//
//   * Run kanata from launchd and kanata is responsible for itself, so the
//     grant sits on the kanata binary. Homebrew installs each version at a
//     new path with a new code hash, so every upgrade needs a re-grant.
//   * Run kanata inside tmux and the tmux server becomes responsible, so the
//     check fails unless tmux itself is granted keyboard access.
//
// What this does
// --------------
// Stage 1: re-exec this same binary with the "disclaim responsibility" spawn
//          attribute, which makes it its own responsible process no matter
//          who started it (launchd, tmux, a terminal).
// Stage 2: start kanata as an ordinary child. The child inherits this
//          launcher as its responsible process, so kanata's privacy checks
//          are evaluated against this binary.
//
// Grant this binary once and kanata can be upgraded freely.
//
// Security
// --------
// This runs as root (kanata needs root on macOS) and holds the keyboard
// grants, so it must not be a general-purpose "run anything" tool: the kanata
// path and config path are fixed at build time and command-line arguments are
// ignored.

#include <errno.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Both paths come from the Makefile (-DKANATA_BIN=... -DKANATA_CFG=...).
#ifndef KANATA_BIN
#error "KANATA_BIN must be defined at build time"
#endif
#ifndef KANATA_CFG
#error "KANATA_CFG must be defined at build time"
#endif

// Set by stage 1 so the re-exec'd copy knows to skip straight to stage 2.
// Setting it by hand only skips the disclaim step; it grants nothing.
#define STAGE_ENV "KANATA_LAUNCH_STAGE2"

extern char **environ;

// Private libSystem function with no public header, so it is declared here.
// With disclaim=1 the spawned process becomes its own responsible process
// instead of inheriting the parent's. Being private, it could change in a
// future macOS release.
extern int responsibility_spawnattrs_setdisclaim(posix_spawnattr_t *attrs, int disclaim);

// PID of the kanata child; -1 until it has been started.
static pid_t child = -1;

// Pass termination signals on to kanata so `launchctl kickstart -k`, a tmux
// session being killed, or Ctrl-C stop kanata instead of orphaning it.
// kill() is async-signal-safe, so calling it from a handler is fine.
static void forward_signal(int sig) {
  if (child > 0) kill(child, sig);
}

// Stage 1: replace this process with a copy of itself that is its own
// responsible process. Only returns on failure.
static int reexec_disclaimed(void) {
  // Resolve our own path; argv[0] is not trustworthy for this.
  char self[4096];
  uint32_t size = sizeof(self);
  if (_NSGetExecutablePath(self, &size) != 0) {
    fprintf(stderr, "kanata-launch: executable path too long\n");
    return 1;
  }

  posix_spawnattr_t attr;
  int rc = posix_spawnattr_init(&attr);
  // SETEXEC turns posix_spawn into an exec: same PID, no extra process, so
  // launchd and sudo keep supervising the process they started.
  if (rc == 0) rc = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC);
  if (rc == 0) rc = responsibility_spawnattrs_setdisclaim(&attr, 1);
  if (rc != 0) {
    fprintf(stderr, "kanata-launch: spawn attr setup failed: %s\n", strerror(rc));
    return 1;
  }

  setenv(STAGE_ENV, "1", 1);
  char *argv[] = {self, NULL};
  rc = posix_spawn(NULL, self, NULL, &attr, argv, environ);

  // With SETEXEC a successful spawn never returns, so reaching here is an error.
  fprintf(stderr, "kanata-launch: re-exec failed: %s\n", strerror(rc));
  return 1;
}

int main(void) {
  if (getenv(STAGE_ENV) == NULL) return reexec_disclaimed();

  // Stage 2. Drop the marker so kanata does not inherit it.
  unsetenv(STAGE_ENV);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = forward_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);  // launchd stop / kickstart -k
  sigaction(SIGINT, &sa, NULL);   // Ctrl-C in a terminal or tmux pane
  sigaction(SIGHUP, &sa, NULL);   // terminal or tmux session closed

  // Start kanata as a child rather than exec'ing it: this process has to stay
  // alive as the responsible process. If it exec'd kanata, kanata would become
  // responsible for itself again and the grant would be back on kanata.
  char *argv[] = {KANATA_BIN, "-c", KANATA_CFG, NULL};
  int rc = posix_spawn(&child, KANATA_BIN, NULL, NULL, argv, environ);
  if (rc != 0) {
    fprintf(stderr, "kanata-launch: cannot start %s: %s\n", KANATA_BIN, strerror(rc));
    return 1;
  }

  // Wait for kanata. A forwarded signal interrupts waitpid with EINTR, so
  // retry until the child has actually exited.
  int status;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("kanata-launch: waitpid");
      return 1;
    }
  }

  // Report kanata's result as our own so launchd (KeepAlive) sees the real
  // exit status. 128+signal is the usual shell convention for a signal death.
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return 1;
}
