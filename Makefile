# Paths are compiled into the binary on purpose: it runs as root and ignores
# its arguments, so these are the only locations it will ever use.
#
# KANATA_BIN should be the Homebrew symlink, not the versioned Cellar path,
# so the launcher keeps working across `brew upgrade kanata`.
KANATA_BIN ?= /opt/homebrew/bin/kanata
KANATA_CFG ?= $(HOME)/.config/kanata/kanata.kbd

# Root-only directory where the launcher keeps the verified copy it executes.
RUN_DIR    ?= /var/db/kanata-launch

# What a kanata binary must be signed with for the launcher to run it. These
# must match what .github/workflows/build-kanata.yml signs with: the same
# identifier, and the certificate committed at SIGNING_CERT (public half only;
# the private key exists only as a GitHub Actions secret).
KANATA_SIGN_ID ?= com.bryanmarty.kanata
SIGNING_CERT   ?= signing/kanata-signing-cert.pem
KANATA_CERT_SHA1 := $(shell /usr/bin/openssl x509 -in $(SIGNING_CERT) -noout -fingerprint -sha1 2>/dev/null | sed 's/.*=//; s/://g')

# Code-signing identifier of the launcher itself, shown in the macOS privacy
# logs. Override with your own: make SIGN_ID=com.example.kanata-launch
SIGN_ID    ?= com.bryanmarty.kanata-launch

CFLAGS  ?= -O2 -Wall -Wextra
LDLIBS  ?= -framework Security -framework CoreFoundation

DEFINES = -DKANATA_BIN='"$(KANATA_BIN)"' -DKANATA_CFG='"$(KANATA_CFG)"' \
          -DRUN_DIR='"$(RUN_DIR)"' \
          -DKANATA_SIGN_ID='"$(KANATA_SIGN_ID)"' \
          -DKANATA_CERT_SHA1='"$(KANATA_CERT_SHA1)"'

# The launcher is ad-hoc signed (--sign -), which needs no certificate.
# `--options runtime` turns on the hardened runtime so DYLD_* environment
# variables cannot inject code into it. The macOS privacy grant is tied to
# this binary's path and code hash, so rebuilding or moving it means granting
# Input Monitoring and Accessibility again.
kanata-launch: kanata-launch.c $(SIGNING_CERT)
	@test -n "$(KANATA_CERT_SHA1)" || { echo "cannot read certificate $(SIGNING_CERT)"; exit 1; }
	$(CC) $(CFLAGS) $(DEFINES) -o $@ kanata-launch.c $(LDLIBS)
	codesign --force --sign - --options runtime --identifier $(SIGN_ID) $@

# Would the launcher accept the kanata that is installed right now? Runs the
# same check with codesign, without needing root.
check:
	codesign --verify --strict \
	  -R='identifier "$(KANATA_SIGN_ID)" and certificate leaf = H"$(KANATA_CERT_SHA1)"' \
	  "$$(realpath $(KANATA_BIN))" && echo "OK: $(KANATA_BIN) is signed by the expected certificate"

clean:
	rm -f kanata-launch

.PHONY: check clean
