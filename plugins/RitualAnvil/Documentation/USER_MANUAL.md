# Ritual Anvil — User Manual

**Version 0.3.0 (beta)** · SansKrono

Ritual Anvil is a drum-bus finishing chain in one plugin: cleanup, transient shaping, tone EQ, parallel punch, multiband saturation, stereo width and a true-peak limiter. Turn **Mix Ready** and the whole chain moves together; open the stages to set each one yourself.

---

## Formats

| macOS (universal: Apple Silicon + Intel) |
| :--- |
| VST3 · Audio Unit · Standalone app |

Mono and stereo tracks. Sample rates 44.1–192 kHz.

**Latency:** 157 samples at 48 kHz (151 at 44.1 kHz, 229 at 96 kHz). It is reported to the host, so delay compensation lines it up automatically. The footer of the plugin window shows the current value.

---

## Signal flow

Audio runs left to right, top to bottom, in the order the stage numbers show:

**01 Input → 02 Transient → 03 Tone EQ → 04 Punch → 05 Heat → 06 Width → 07 Ceiling**

---

## Mix Ready (the big dial)

One knob from raw kit to a finished bus. As you turn it up it adds, on top of whatever you have set by hand:

| Mix Ready | What it adds |
| :--- | :--- |
| 0 % | nothing |
| from 0 % | high-pass rises (floor 20 → 50 Hz); Kick +4, Box −3, Snare +3, Presence +2, Air +4 dB at 100 % |
| above 40 % | Punch (parallel compression) blends in, up to 50 % wet |
| above 50 % | transient Attack up to +4 dB; Width up to +20 % |
| above 60 % | Heat drive up to +6 dB |

Your own settings stay live: the macro's contribution is added to them and kept inside each control's range.

**Ember = what the macro is doing.** Each knob shows an ember extension on its ring from your setting to the value actually in use, and an ember `→ value` line under it. The EQ display draws your curve in amber and the result in ember. The background glow brightens as Heat and Mix Ready rise.

---

## Controls

| Stage | Control | Range | Default | What it does |
| :--- | :--- | :--- | :--- | :--- |
| 01 Input | Trim | −12…+12 dB | 0 dB | Level into the chain |
| 01 Input | HPF | 20–100 Hz | 40 Hz | 12 dB/oct high-pass to clear rumble |
| 02 Transient | Attack | −12…+12 dB | 0 dB | Boosts or softens hits (lows and mids; cymbals untouched) |
| 02 Transient | Sustain | −12…+6 dB | 0 dB | Body and ring after the hit |
| 03 Tone EQ | Kick | ±12 dB | 0 dB | 60 Hz |
| 03 Tone EQ | Box | ±12 dB | 0 dB | 200 Hz (usually a cut) |
| 03 Tone EQ | Snare | ±12 dB | 0 dB | 3 kHz |
| 03 Tone EQ | Presence | ±12 dB | 0 dB | 8 kHz |
| 03 Tone EQ | Air | ±12 dB | 0 dB | 12 kHz |
| 04 Punch | Blend | 0–100 % | 0 % | Parallel compression (8:1, fast); 100 % = half compressed, half dry |
| 05 Heat | Drive | 0–20 dB | 0 dB | Three-band saturation: hard clip on lows, soft clip on mids, tube on highs. Level-compensated, oversampled 4× |
| 06 Width | Side | 50–150 % | 100 % | Stereo width above 200 Hz; lows stay as they are |
| 07 Ceiling | dBTP | −1.0…−0.1 dB | −0.3 dB | True-peak limiter ceiling (catches inter-sample peaks) |
| Header | Power | on / off | on | Bypass, with a short click-free crossfade |

**Meters:** IN / OUT peak (dBFS) with the ceiling marked in red, a gain-reduction bar for Punch and one for the limiter, and the limiter's gain reduction in the footer.

**Knob handling:** drag up/down · Shift + drag for fine moves · mouse wheel · double-click to reset.

---

## Quick start

1. Insert Ritual Anvil on your drum bus.
2. Set **Trim** so the IN meter peaks around −12 to −6 dBFS.
3. Turn **Mix Ready** until the kit sounds finished; 50–75 % is a good start.
4. Fine-tune by hand: cut **Box** if it gets boxy, add **Heat** for grit, raise **Punch** for weight.
5. Leave **Ceiling** at −0.3 dB unless the bus feeds a mastering chain that wants more headroom.

---

## Installation (macOS)

Open the `.dmg` and run **RitualAnvil-0.3.0-beta.pkg**. It installs:

- VST3 → `/Library/Audio/Plug-Ins/VST3/RitualAnvil.vst3`
- Audio Unit → `/Library/Audio/Plug-Ins/Components/RitualAnvil.component`
- Standalone → `/Applications/RitualAnvil.app`

**This beta is not notarized by Apple.** See `INSTALL.md` in the download for how to open it the first time.

---

## Beta notes

- Not notarized: macOS asks for confirmation on first install/launch.
- On macOS the window can show white for a moment while it opens.
- The glass interface uses GPU blur. If your DAW feels sluggish with many instances open, please report it.

---

## Credits

Ritual Anvil by SansKrono. Re-implementation of the Ritual Suite prototype.
Built with JUCE. Interface uses GlassKit (MIT). See `THIRD_PARTY_NOTICES.txt`.
