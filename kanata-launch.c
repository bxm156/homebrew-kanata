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
// Stage 2: verify the kanata binary, then start it as an ordinary child. The
//          child inherits this launcher as its responsible process, so
//          kanata's privacy checks are evaluated against this binary.
//
// Grant this binary once and kanata can be upgraded without re-granting.
//
// Security
// --------
// Because the grant sits on the launcher, whatever the launcher starts gets
// keyboard access, and "a file named kanata" proves nothing: Homebrew's
// directories are writable by the user. The launcher therefore:
//
//   * ignores its arguments; the kanata and config paths are compiled in;
//   * copies the kanata binary into a root-only directory (RUN_DIR) and
//     works on that copy, so it cannot be swapped between check and exec;
//   * requires the copy to carry a valid code signature with a specific
//     identifier, made by a specific certificate (both compiled in). The
//     signing key lives in CI, not on this machine, so only builds from that
//     pipeline pass, and every new version passes without a local step;
//   * starts kanata with a fixed, minimal environment.
//
// The launcher's own code hash is what macOS pins the grant to, so these
// checks cannot be patched out without losing the grant.

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <errno.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// All of these come from the Makefile (-DKANATA_BIN=... etc).
#ifndef KANATA_BIN
#error "KANATA_BIN must be defined at build time"
#endif
#ifndef KANATA_CFG
#error "KANATA_CFG must be defined at build time"
#endif
#ifndef KANATA_SIGN_ID
#error "KANATA_SIGN_ID (required signing identifier) must be defined at build time"
#endif
#ifndef KANATA_CERT_SHA1
#error "KANATA_CERT_SHA1 (required signing certificate hash) must be defined at build time"
#endif
#ifndef RUN_DIR
#error "RUN_DIR must be defined at build time"
#endif

#define RUN_COPY RUN_DIR "/kanata"      // the verified copy that gets executed
#define RUN_TEMP RUN_DIR "/kanata.new"  // written first, then renamed into place

// The code requirement kanata must satisfy, in Apple's requirement language
// (the same text `codesign --verify -R=...` accepts): signed with this
// identifier by exactly this certificate. An ad-hoc signature, or one made
// with any other certificate, does not match.
#define KANATA_REQUIREMENT \
  "identifier \"" KANATA_SIGN_ID "\" and certificate leaf = H\"" KANATA_CERT_SHA1 "\""

// Refuse anything implausibly large rather than reading it into memory.
#define MAX_BIN_SIZE (256u * 1024u * 1024u)

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

// Read the whole kanata binary into memory. Returns 0 on success; the caller
// frees *buf.
static int read_binary(unsigned char **buf, size_t *len) {
  int fd = open(KANATA_BIN, O_RDONLY);  // follows the Homebrew symlink
  if (fd < 0) {
    fprintf(stderr, "kanata-launch: cannot open %s: %s\n", KANATA_BIN, strerror(errno));
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
      (uintmax_t)st.st_size > MAX_BIN_SIZE) {
    fprintf(stderr, "kanata-launch: %s is not a regular file of sane size\n", KANATA_BIN);
    close(fd);
    return -1;
  }

  size_t want = (size_t)st.st_size;
  unsigned char *data = malloc(want);
  if (data == NULL) {
    fprintf(stderr, "kanata-launch: out of memory\n");
    close(fd);
    return -1;
  }

  size_t got = 0;
  while (got < want) {
    ssize_t n = read(fd, data + got, want - got);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;  // error, or the file shrank underneath us
    got += (size_t)n;
  }
  close(fd);

  if (got != want) {
    fprintf(stderr, "kanata-launch: short read of %s\n", KANATA_BIN);
    free(data);
    return -1;
  }

  *buf = data;
  *len = want;
  return 0;
}

// Write the bytes to RUN_COPY inside a directory only root can touch. The
// signature check and the exec both use this copy, so nothing a normal user
// does to the original path after this point matters. Returns 0 on success.
static int install_private_copy(const unsigned char *buf, size_t len) {
  if (mkdir(RUN_DIR, 0700) != 0 && errno != EEXIST) {
    fprintf(stderr, "kanata-launch: cannot create %s: %s\n", RUN_DIR, strerror(errno));
    return -1;
  }

  // lstat, not stat: a symlink here must be rejected, not followed.
  struct stat st;
  if (lstat(RUN_DIR, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != 0 ||
      (st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
    fprintf(stderr, "kanata-launch: %s must be a directory owned by root and "
                    "not writable by group or others\n", RUN_DIR);
    return -1;
  }

  // Always write a brand-new file and rename it into place. Overwriting an
  // existing signed binary in place can make macOS reject its signature.
  unlink(RUN_TEMP);
  int fd = open(RUN_TEMP, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0700);
  if (fd < 0) {
    fprintf(stderr, "kanata-launch: cannot create %s: %s\n", RUN_TEMP, strerror(errno));
    return -1;
  }

  size_t done = 0;
  while (done < len) {
    ssize_t n = write(fd, buf + done, len - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) {
      fprintf(stderr, "kanata-launch: write to %s failed: %s\n", RUN_TEMP, strerror(errno));
      close(fd);
      unlink(RUN_TEMP);
      return -1;
    }
    done += (size_t)n;
  }

  if (close(fd) != 0 || rename(RUN_TEMP, RUN_COPY) != 0) {
    fprintf(stderr, "kanata-launch: cannot install %s: %s\n", RUN_COPY, strerror(errno));
    unlink(RUN_TEMP);
    return -1;
  }
  return 0;
}

// Check that the file at `path` has an intact code signature that satisfies
// KANATA_REQUIREMENT. Returns 1 if it does, 0 otherwise (with the reason on
// stderr). "Intact" matters: the check re-hashes the file, so a binary that
// was modified after signing fails even though it still carries a signature.
static int signature_is_trusted(const char *path) {
  int trusted = 0;
  CFURLRef url = NULL;
  CFStringRef text = NULL;
  SecStaticCodeRef code = NULL;
  SecRequirementRef requirement = NULL;

  url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
                                                (CFIndex)strlen(path), false);
  text = CFStringCreateWithCString(NULL, KANATA_REQUIREMENT, kCFStringEncodingUTF8);

  OSStatus status = errSecParam;
  if (url != NULL && text != NULL) {
    status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code);
  }
  if (status == errSecSuccess) {
    status = SecRequirementCreateWithString(text, kSecCSDefaultFlags, &requirement);
  }
  if (status == errSecSuccess) {
    status = SecStaticCodeCheckValidity(
        code, kSecCSStrictValidate | kSecCSCheckAllArchitectures, requirement);
  }

  if (status == errSecSuccess) {
    trusted = 1;
  } else {
    char reason[256] = "unknown error";
    CFStringRef message = SecCopyErrorMessageString(status, NULL);
    if (message != NULL) {
      CFStringGetCString(message, reason, sizeof(reason), kCFStringEncodingUTF8);
      CFRelease(message);
    }
    fprintf(stderr, "kanata-launch: signature check failed: %s (%d)\n", reason, (int)status);
  }

  if (requirement != NULL) CFRelease(requirement);
  if (code != NULL) CFRelease(code);
  if (text != NULL) CFRelease(text);
  if (url != NULL) CFRelease(url);
  return trusted;
}

int main(void) {
  if (getenv(STAGE_ENV) == NULL) return reexec_disclaimed();

  // ---- Stage 2 ----

  int is_root = (geteuid() == 0);

  // As root: take a private copy first and verify that. As a normal user the
  // copy is not possible, so verify the original in place and stop; that
  // makes an unprivileged run a safe "would this be accepted?" check.
  const char *target = KANATA_BIN;
  if (is_root) {
    unsigned char *bin = NULL;
    size_t bin_len = 0;
    if (read_binary(&bin, &bin_len) != 0) return 1;
    int rc = install_private_copy(bin, bin_len);
    free(bin);
    if (rc != 0) return 1;
    target = RUN_COPY;
  }

  if (!signature_is_trusted(target)) {
    if (is_root) unlink(RUN_COPY);  // never leave an unverified binary behind
    fprintf(stderr,
            "kanata-launch: refusing to start kanata\n"
            "  binary:   %s\n"
            "  required: %s\n",
            KANATA_BIN, KANATA_REQUIREMENT);
    return 1;
  }

  if (!is_root) {
    fprintf(stderr, "kanata-launch: %s has a trusted signature, but the launcher "
                    "must run as root to start it\n", KANATA_BIN);
    return 1;
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = forward_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);  // launchd stop / kickstart -k
  sigaction(SIGINT, &sa, NULL);   // Ctrl-C in a terminal or tmux pane
  sigaction(SIGHUP, &sa, NULL);   // terminal or tmux session closed

  // Fixed, minimal environment: nothing from the caller (DYLD_* variables in
  // particular) reaches kanata.
  char *envp[] = {"PATH=/usr/bin:/bin:/usr/sbin:/sbin", NULL};

  // Start kanata as a child rather than exec'ing it: this process has to stay
  // alive as the responsible process. If it exec'd kanata, kanata would become
  // responsible for itself again and the grant would be back on kanata.
  char *argv[] = {RUN_COPY, "-c", KANATA_CFG, NULL};
  int rc = posix_spawn(&child, RUN_COPY, NULL, NULL, argv, envp);
  if (rc != 0) {
    fprintf(stderr, "kanata-launch: cannot start %s: %s\n", RUN_COPY, strerror(rc));
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
