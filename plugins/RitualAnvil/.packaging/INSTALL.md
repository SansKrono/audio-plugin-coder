# RitualAnvil 0.3.0 (beta) — Installation (macOS)

Universal build: Apple Silicon and Intel Macs.

## Install

1. Open `RitualAnvil-0.3.0-beta-macOS.dmg`.
2. Run `RitualAnvil-0.3.0-beta.pkg`. Under **Customize** you can untick any of:
   - VST3 → `/Library/Audio/Plug-Ins/VST3`
   - Audio Unit → `/Library/Audio/Plug-Ins/Components`
   - Standalone → `/Applications`
3. Rescan plug-ins in your DAW (Logic: restart it; it validates the AU on first launch).

## First launch of an unnotarized beta

This beta is **not notarized by Apple**, so macOS may refuse to open the installer.

- **Installer:** right-click (Control-click) the `.pkg` → **Open** → **Open**.
  On recent macOS versions: **System Settings → Privacy & Security** → scroll down → **Open Anyway**.
- **If a DAW reports the plug-in as damaged or blocks it**, clear the quarantine flag in Terminal:

```
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/RitualAnvil.vst3 /Library/Audio/Plug-Ins/Components/RitualAnvil.component /Applications/RitualAnvil.app
```

## Uninstall

Delete the three items listed above.

## More

- `USER_MANUAL.md` — controls, Mix Ready, quick start
- `LICENSE.txt` — license terms
- `THIRD_PARTY_NOTICES.txt` — open-source components
