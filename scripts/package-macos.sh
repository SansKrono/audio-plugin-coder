#!/bin/bash
# macOS packaging for APC plugins (no PowerShell needed).
#
#   scripts/package-macos.sh <PluginName> <version> <artefactsDir> [signIdentity]
#
#   artefactsDir  e.g. build/universal/plugins/RitualAnvil/RitualAnvil_artefacts/Release
#   signIdentity  "-" (ad-hoc, default) or a "Developer ID Application: …" identity.
#                 The .pkg is only signed when INSTALLER_IDENTITY is set
#                 ("Developer ID Installer: …"); notarization is not done here.
#
# Output: dist/<Name>-v<version>/  (pkg, dmg, docs) and dist/<Name>-v<version>-macOS.zip
# Docs:   every file in plugins/<Name>/Documentation is copied next to the installer.
set -euo pipefail

NAME="${1:?plugin name}"
VERSION="${2:?version}"
ART="${3:?artefacts dir}"
SIGN_ID="${4:--}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BUNDLE_BASE="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$ART/VST3/$NAME.vst3/Contents/Info.plist")"
OUT="dist/$NAME-v$VERSION"
WORK="build/package-$NAME-$VERSION"
STAGE="$WORK/stage"

echo "== $NAME $VERSION from $ART (sign: $SIGN_ID)"
for f in "$ART/VST3/$NAME.vst3" "$ART/AU/$NAME.component" "$ART/Standalone/$NAME.app"; do
  [ -e "$f" ] || { echo "missing: $f"; exit 1; }
done

rm -rf "$WORK" "$OUT" "dist/$NAME-v$VERSION-macOS.zip"
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app" "$OUT"

ditto "$ART/VST3/$NAME.vst3"      "$STAGE/vst3/$NAME.vst3"
ditto "$ART/AU/$NAME.component"   "$STAGE/au/$NAME.component"
ditto "$ART/Standalone/$NAME.app" "$STAGE/app/$NAME.app"

# ---- sign (ad-hoc unless an identity is given) ----
for b in "$STAGE/vst3/$NAME.vst3" "$STAGE/au/$NAME.component" "$STAGE/app/$NAME.app"; do
  codesign --force --deep --timestamp=none --sign "$SIGN_ID" "$b"
  codesign --verify --deep --strict "$b"
done

# ---- component packages ----
pkgbuild --root "$STAGE/vst3" --install-location "/Library/Audio/Plug-Ins/VST3" \
         --identifier "$BUNDLE_BASE.vst3" --version "$VERSION" "$WORK/$NAME-VST3.pkg"
pkgbuild --root "$STAGE/au" --install-location "/Library/Audio/Plug-Ins/Components" \
         --identifier "$BUNDLE_BASE.au" --version "$VERSION" "$WORK/$NAME-AU.pkg"
pkgbuild --component "$STAGE/app/$NAME.app" --install-location "/Applications" \
         --identifier "$BUNDLE_BASE.app" --version "$VERSION" "$WORK/$NAME-App.pkg"

# ---- product package with a choice per format ----
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$NAME $VERSION</title>
  <license file="LICENSE.txt"/>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="vst3"/>
    <line choice="au"/>
    <line choice="app"/>
  </choices-outline>
  <choice id="vst3" title="VST3 plug-in" description="/Library/Audio/Plug-Ins/VST3"><pkg-ref id="$BUNDLE_BASE.vst3"/></choice>
  <choice id="au" title="Audio Unit plug-in" description="/Library/Audio/Plug-Ins/Components"><pkg-ref id="$BUNDLE_BASE.au"/></choice>
  <choice id="app" title="Standalone app" description="/Applications"><pkg-ref id="$BUNDLE_BASE.app"/></choice>
  <pkg-ref id="$BUNDLE_BASE.vst3">$NAME-VST3.pkg</pkg-ref>
  <pkg-ref id="$BUNDLE_BASE.au">$NAME-AU.pkg</pkg-ref>
  <pkg-ref id="$BUNDLE_BASE.app">$NAME-App.pkg</pkg-ref>
</installer-gui-script>
EOF

PACK="plugins/$NAME/.packaging"   # LICENSE.txt (required, shown by the installer) + extra files for the download
[ -f "$PACK/LICENSE.txt" ] || { echo "missing $PACK/LICENSE.txt"; exit 1; }
mkdir -p "$WORK/resources"
cp "$PACK/LICENSE.txt" "$WORK/resources/LICENSE.txt"

PRODUCT_SIGN=()
if [ -n "${INSTALLER_IDENTITY:-}" ]; then PRODUCT_SIGN=(--sign "$INSTALLER_IDENTITY"); fi
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
             --resources "$WORK/resources" ${PRODUCT_SIGN[@]+"${PRODUCT_SIGN[@]}"} \
             "$OUT/$NAME-$VERSION.pkg"

# ---- docs next to the installer ----
cp -R "$PACK/." "$OUT/"
if [ -d "plugins/$NAME/Documentation" ]; then cp -R "plugins/$NAME/Documentation/." "$OUT/"; fi

# ---- zip (installer + docs, no AppleDouble files) and dmg (same content) ----
(cd dist && ditto -c -k --norsrc --noextattr --noacl --keepParent "$NAME-v$VERSION" "$NAME-v$VERSION-macOS.zip")
DMG_SRC="$WORK/dmg"
mkdir -p "$DMG_SRC"
cp -R "$OUT/." "$DMG_SRC/"
hdiutil create -volname "$NAME $VERSION" -srcfolder "$DMG_SRC" -ov -format UDZO "$OUT/$NAME-$VERSION-macOS.dmg" > /dev/null

echo "== done"
ls -la "$OUT"
