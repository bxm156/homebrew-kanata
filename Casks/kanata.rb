cask "kanata" do
  # version and sha256 are updated by .github/workflows/build-kanata.yml.
  version "0.0.0"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"

  url "https://github.com/bxm156/kanata-launch/releases/download/kanata-v#{version}/Kanata-#{version}.zip"
  name "Kanata"
  desc "Keyboard remapper, built from upstream source and signed"
  homepage "https://github.com/jtroo/kanata"

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
