# Display layout

The panel is 128×64 monochrome pixels. The tracker renders with uGUI's
`FONT_6X8`, giving 21 characters per line. All positions are defined in
[`ui/theme.h`](../ui/theme.h); nothing else hard-codes pixel coordinates.

```
 x=0                                                          x=127
  ┌───────────────────────────────────────────────────────────────┐  y=0
  │ PTN:00 SLT:00 BPM:120                                         │  header 1
  ├───────────────────────────────────────────────────────────────┤  y=8
  │ OCT:4 STP:1 >PLAY REC                                         │  header 2
  ├───────────────────────────────────────────────────────────────┤  y=16
  │ 00 C-4 01 7F   |  C-5 .. ..                                    │  row 0  (cursor)
  │ 01 --- .. ..   |  --- .. ..                                    │  row 1
  │ 02 E-4 02 5A   |  OFF .. ..                                    │  row 2  (playing)
  │ 03 --- .. ..   |  --- .. ..                                    │  row 3
  ├───────────────────────────────────────────────────────────────┤  y=50
  │ CH:00 FX:00/00 GT:FF                                          │  status
  └───────────────────────────────────────────────────────────────┘  y=63
```

## Header (lines 1 and 2)

| Field | Meaning |
|-------|---------|
| `PTN:` | Current pattern index (hex). |
| `SLT:` | Current song slot (hex). |
| `BPM:` | Tempo in beats per minute. |
| `OCT:` | Current note-entry octave. |
| `STP:` | Edit step — rows advanced after each entry. |
| `>PLAY` / ` STOP` | Transport state. |
| `REC` / `OVR` / `INS` | Record mode and insert/overwrite mode. |

## Grid

Five columns of information per channel, laid out as three fields:

| Field | Width | Meaning |
|-------|-------|---------|
| Note | 3 chars | `C-4`, `---` (empty), `OFF`, `CUT`. |
| Instrument | 2 chars | Hex `00`–`7F`, or `..` to inherit. |
| Volume | 2 chars | Hex `00`–`7F`, or `..` to inherit. |

- The left **gutter** shows the row number in hex.
- **Two channels** are visible at once and the view scrolls sideways as the
  cursor moves across the 16 channels.
- The **focused field** is drawn in inverse video (white on black).
- The **playing row** of the current pattern is drawn as a full inverted row.

## Status line

| Field | Meaning |
|-------|---------|
| `CH:` | Focused channel. |
| `FX:` | Effect command and parameter bytes of the cell under the cursor. |
| `GT:` | Gate value of the cell under the cursor. |

When the application posts a status message (for example `SAVED`, `UNDO`,
`ROW INSERTED`), the status line shows the message for about 1.2 seconds and
then reverts to the cell details.

## Colour model

The framebuffer uses one bit per pixel. `gui_task` inverts the colour value
before writing, so `gui_set_inverted(true)` together with a filled rectangle
produces the standard "selected" look. The whole screen is redrawn only when
state changes (`needs_redraw`), which keeps the display update inside the
one-page-per-task budget of the display service.
