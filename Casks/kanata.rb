cask "kanata" do
  # version and sha256 are updated by .github/workflows/build-kanata.yml.
  version "1.12.0"
  sha256 "0bb0c10b9d9fcfe3ed96424fd487ed9a5f7631d05982aebea1f22e3718f23fd9"

  url "https://github.com/bxm156/homebrew-kanata/releases/download/kanata-v#{version}/Kanata-#{version}.zip"
  name "Kanata"
  desc "Keyboard remapper, built from upstream source and signed"
  homepage "https://github.com/jtroo/kanata"

  # Tells `brew livecheck` where new versions come from: upstream's latest
  # release (pre-releases are skipped), not this tap's own releases. The build
  # workflow uses it to decide when to build.
  livecheck do
    url "https://github.com/jtroo/kanata"
    strategy :github_latest
  end

  depends_on arch: :arm64

  # Installed to /Applications/Kanata.app: a fixed path, removed on uninstall.
  app "Kanata.app"

  caveats <<~EOS
    kanata is a command-line program inside the app bundle. Run it as:
      #{appdir}/Kanata.app/Contents/MacOS/kanata

    Grant Kanata Input Monitoring and Accessibility once, in
    System Settings -> Privacy & Security.
  EOS
end
