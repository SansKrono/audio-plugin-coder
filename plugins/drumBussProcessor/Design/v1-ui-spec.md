# UI Specification v1 (Based on NeoGrid Minimal)

## Design Source
- **Library Design:** NeoGrid Minimal
- **ID:** neogrid-minimal
- **Style:** Geometric, grid-based, futuristic minimalism

## Layout
- **Window:** 940x600 px
- **Structure:** Three-column layout (Input - Macro - Output)
- **Grid:** 4fr (Input) - 8fr (Macro) - 4fr (Output)

## Colors
- **Primary:** #00FFFF (Cyan) - Used for Value Arcs and Active elements
- **Accent:** #FF0099 (Magenta) - Used for Highlights/Warnings
- **Background:** #111115 (Dark Grey)
- **Surface:** #1E1E24 (Lighter Grey)

## Controls
| Parameter | Type | Position | Range | Default | Label |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `input_trim` | Knob (Medium) | Left Panel | -24 to +24 dB | 0.0 | INPUT |
| `mix_ready` | Knob (Large) | Center Panel | 0% to 100% | 0.0 | MIX READY |
| `output_trim` | Fader (Vertical) | Right Panel | -24 to +24 dB | 0.0 | OUTPUT |
| `bypass` | Toggle Button | Header | Off/On | Off | BYPASS |

## Visual Style
- **Knobs:** Vector SVG, flat track, cyan value arc, "futuristic" screw heads optional.
- **Faders:** Integrated into track, illuminated fill.
- **Meters:** LED segment style (Green -> Yellow -> Red).
