# Paths are compiled into the binary on purpose: it runs as root and ignores
# its arguments, so these are the only kanata binary and config it will use.
#
# KANATA_BIN should be the Homebrew symlink, not the versioned Cellar path,
# so the launcher keeps working across `brew upgrade kanata`.
KANATA_BIN ?= /opt/homebrew/bin/kanata
KANATA_CFG ?= $(HOME)/.config/kanata/kanata.kbd

# Code-signing identifier shown in the macOS privacy logs. Override with your
# own, for example: make SIGN_ID=com.example.kanata-launch
SIGN_ID    ?= com.bryanmarty.kanata-launch

CFLAGS ?= -O2 -Wall -Wextra

# Ad-hoc signing (--sign -) needs no certificate. The macOS privacy grant is
# tied to this binary's path and code hash, so rebuilding or moving it means
# granting Input Monitoring and Accessibility again.
kanata-launch: kanata-launch.c
	$(CC) $(CFLAGS) -DKANATA_BIN='"$(KANATA_BIN)"' -DKANATA_CFG='"$(KANATA_CFG)"' -o $@ $<
	codesign --force --sign - --identifier $(SIGN_ID) $@

clean:
	rm -f kanata-launch

.PHONY: clean
