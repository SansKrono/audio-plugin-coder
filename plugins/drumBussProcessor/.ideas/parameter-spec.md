# Parameter Specification: drumBussProcessor

| ID | Name | Type | Range | Default | Unit | Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `input_trim` | Input Trim | Float | -24.0 - +24.0 | 0.0 | dB | Pre-processing gain stage to drive the chain or clean up hot signals. |
| `mix_ready` | Mix Ready | Float | 0.0 - 100.0 | 0.0 | % | Macro control for the entire processing chain intensity. |
| `output_trim` | Output Trim | Float | -24.0 - +24.0 | 0.0 | dB | Post-processing gain stage for level matching. |
| `bypass` | Bypass | Bool | Off/On | Off | - | Hard bypass of the entire plugin. |
