class Kanata < Formula
  desc "Keyboard remapper, built from upstream source and signed"
  homepage "https://github.com/jtroo/kanata"
  # url, version and sha256 are updated by .github/workflows/build-kanata.yml.
  url "https://github.com/bxm156/kanata-launch/releases/download/kanata-v1.12.0/kanata-1.12.0-arm64-apple-darwin.tar.gz"
  version "1.12.0"
  sha256 "fc7ade029f78374027020249b4c08f39afc8289800324a472dc250fd7d66eb9b"
  license "LGPL-3.0-only"
  revision 1

  depends_on arch: :arm64
  depends_on :macos

  def install
    # Prebuilt and already signed. Do not re-sign it here: the macOS privacy
    # grant follows the signature made by the workflow.
    bin.install "kanata"
  end

  post_install_steps do
    # macOS ties a privacy grant to the real path of a binary, and Homebrew's
    # real path changes with every version (Cellar/kanata/<version>/...).
    # Symlinks such as bin/kanata do not help, because macOS resolves them.
    # So keep a copy at a path that never changes and run kanata from there.
    # It is named kanata-signed so it is not mistaken for an upstream build.
    #
    # Homebrew does not track this copy and formulae have no uninstall hook,
    # so `brew uninstall` leaves it behind; delete it by hand.
    #
    # Remove the old copy first so the new one is a fresh file: overwriting a
    # signed binary in place can make macOS reject its signature.
    remove "bin/kanata-signed", base: :homebrew_prefix
    copy "bin/kanata", "bin/kanata-signed", target_base: :homebrew_prefix
  end

  def caveats
    <<~EOS
      Run kanata from this path, which stays the same across upgrades:
        #{HOMEBREW_PREFIX}/bin/kanata-signed

      Grant that file Input Monitoring and Accessibility once, in
      System Settings -> Privacy & Security.
    EOS
  end

  test do
    system bin/"kanata", "--version"
  end
end
