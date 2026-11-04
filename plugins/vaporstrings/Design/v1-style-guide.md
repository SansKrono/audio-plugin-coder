# Style Guide v1 — VaporStrings ("Neon Night")

## Color Palette
| Token | Hex | Use |
|---|---|---|
| `--surface` | `#0d0221` | Letterbox, deepest background |
| `--plate` | `#1a0b2e` | Main plate. Add a subtle vertical gradient down to `#0d0221` |
| `--inset` | `#12052b` | Canvas wells, screens, flip caps |
| `--rule` | `#5b2a86` | Section borders, hairlines, knob tracks |
| `--ink` | `#f6e9ff` | Primary text, values |
| `--ink-dim` | `#a58bc7` | Labels, inactive text |
| `--accent` | `#ff71ce` | Hot pink: value arcs, active tab, VAPOR glow |
| `--accent-2` | `#01cdfe` | Cyan: grid lines, secondary arcs, meters (low) |
| `--accent-3` | `#05ffa1` | Mint: power LEDs on, tracking confidence OK |
| `--accent-4` | `#b967ff` | Violet: idle borders, grip, hover |
| `--sun` | `#fffb96` | Sun top stop, preset name highlight |
| `--warn` | `#ff4f6d` | Clip, low-confidence/chord warning |

**Gradients**
- **Neon arc:** `conic-gradient(#ff71ce, #b967ff, #01cdfe)`.
- **Sun:** a linear gradient `#fffb96 → #ff9a5a → #ff71ce`, with 5 horizontal cut-out stripes that get thicker toward the bottom.
- **Chrome title:** linear `#ffffff → #b9f2ff 45% → #ff71ce 55% → #ffd5f2`, plus a 1 px cyan text-shadow offset for a glitch double.

## Typography
| Role | Font stack | Size (in rem cells) | Style |
|---|---|---|---|
| Title | `--font-display`: Eurostile/Microgramma → Segoe UI Black → Arial Black | 1.6rem | Uppercase, letter-spacing .12em, chrome gradient fill |
| Subtitle (katakana) | `--font-ui` (Segoe UI → Meiryo for JP glyphs) | 0.7rem | `--accent-2`, letter-spacing .3em |
| Section titles | `--font-ui` | 0.6rem | Uppercase, `--ink-dim`, JP glyphs in `--accent` |
| Control labels | `--font-ui` | 0.55rem | Uppercase, letter-spacing .08em, `--ink-dim` |
| Values / screens | `--font-mono`: Cascadia Mono → Consolas | 0.7rem | `--ink`, tabular numbers |
| Tracking note | `--font-display` | 2.2rem | `--ink` with pink glow |

If a display font is bundled at impl, embed it as base64 `@font-face` inside the single HTML file, because external requests are not allowed.

## Spacing
- **Unit:** 1 grid cell = 1 rem (16 px at 1×). Everything snaps to 0.25 cells.
- **Gutters:** sections are 0.5 cells apart. The section title strip is 1.5 cells, and content starts 1.5 cells below the section top.
- **Knobs:** standard knob cells are 4.25–5 cells wide by 4.5 tall (dial plus label). The hero knob is 9×9.
- **Flips:** 3.5 tall (cap 2 plus label). The full-width priority flip is 2 tall with the label inline.

## Control Styles

### Knob (standard)
- **Dial:** a dark disc (`--inset`) with a 1 px `--rule` ring.
- **Track:** a 270° track ring in `--rule` at 25 % alpha.
- **Value arc:** in `--accent`, with `box-shadow`/`filter: drop-shadow` glow at 0 → 6 px as the value increases.
- **Pointer:** a thin `--ink` line.
- **Bipolar knobs** (`flt_env_amt`, `osc2_detune`, `oct_shift`, `slow_pitch`, `lfoN_amt`): the arc starts at 12 o'clock. Positive values draw in pink and negative in cyan.
- **Stepped knobs** (`unison`): tick dots for each step.
- **Hover:** the ring turns `--accent-4`.
- **Drag:** the value appears in a mono readout under the label, replacing the label while dragging. Double-click resets to default.

### VAPOR hero knob
- A chrome bezel (radial gradient silver → violet) around a deep-purple face.
- The full neon conic arc fills with the value.
- The face glows outward with `--accent` at an intensity proportional to the value.
- At values above 0.7, a subtle scanline shimmer animates on the face. It must respect `prefers-reduced-motion`.

### Flip (choice)
- A rounded cap (`--r-control`) in `--inset`, with the value in mono `--ink`.
- Clicking cycles to the next option; shift-click goes to the previous one.
- The border is `--rule`, and turns `--accent-2` on hover.

### Power switch (psw)
- A pill showing an LED and the label. The LED is off at `#2a1840` and on at `--accent-3` with glow.
- When `haze_freeze` is on, the LED is `--accent-2` and pulses slowly.

### Tab chips
- **Idle:** transparent with `--ink-dim` text and a 1 px `--rule` bottom border.
- **Active:** `--accent` text, a 2 px pink underline with glow, and a slightly lifted plate.
- A tiny status LED sits on the left of each chip, mirroring its effect's `*_on` param.

### Screen (preset name)
- `--inset` background, mono `--sun` text, and a faint scanline overlay.

## Canvases
| Canvas | Description |
|---|---|
| `horizon` | Night-sky gradient over a striped sun, with a cyan perspective grid scrolling toward the viewer. The output waveform is drawn as a glowing pink mountain ridge on the horizon line. Animated at 30 fps; pauses when hidden. |
| `tracking` | Large note name (e.g. `A2`) with a cents needle (±50). The confidence bar is mint above 0.6 and amber/warn below. Shows `CHORD? · MONO SYNTH` in `--warn` when confidence is low while the gate is open, and `— —` when silent. |
| `env-curve` | Draws the active envelope shape (an ADSR polyline, a follower trace, or a swell ramp) in pink, with the live `m_env` position as a cyan dot. |
| `fx-scope` | Per-tab visualisation of the active effect, in the `--inset` well with a cyan 10 % grid. |
| `io-meters` | Two horizontal bars labelled IN and OUT. The gradient runs cyan → pink → warn from −60 to 0 dBFS, with a 1.5 s peak-hold tick. |

## Global Treatments
- **Scanlines:** a plate-wide overlay of `repeating-linear-gradient(transparent 0 2px, rgba(0,0,0,.12) 2px 3px)`. It is decorative only and set to `pointer-events:none`.
- **Section frames:**
  - 1 px `--rule` border, radius `--r-panel`, background `rgba(26,11,46,.65)`.
  - A top hairline in a pink → cyan gradient.
- **The grip** uses the violet `--grip-ink` diagonal hatch in the bottom-right corner.

## Example UI States
- **Idle, no input:** the tracking display shows `— —`. The horizon ridge is flat, the meters are dark and the sun is static.
- **Playing a single note:** the note shows in large glowing text with a mint confidence bar. The env-curve dot rides the curve and the horizon ridge pulses with the output.
- **Playing a chord:** the confidence bar drops and the display shows `CHORD? · MONO SYNTH` in `--warn`. Sources A and B keep sounding.
- **Haze freeze on:** the FREEZE LED pulses cyan, the fx-scope tail turns into a steady glowing plateau, and the HAZE tab LED is cyan instead of mint.
- **VAPOR at 100 %:** the hero glow is at maximum, the horizon grid slows down, and the sun shifts a few degrees toward violet as a subtle global "vapor" tint.
- **Clipping:** the OUT meter's end segment flashes `--warn` for 300 ms.
