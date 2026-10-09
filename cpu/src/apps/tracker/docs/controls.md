# Controls

This is the reference for the panel controls in the tracker. `SHIFT` is the
`SHIFT` button; hold it while pressing another control for the shifted
behaviour.

## Transport

| Control | Action |
|---------|--------|
| PLAY / PAUSE | Start playback; press again to pause/resume. |
| STOP | Stop and return to row 0. |
| RECORD | Toggle record mode (shown in the header and on the REC LED). With REC off, pads audition but do not write notes. |

## Navigation

| Control | Action |
|---------|--------|
| BACK | Move the cursor up one row. |
| FORWARD | Move the cursor down one row. |
| LEFT | Move the cursor to the previous column. |
| RIGHT | Move the cursor to the next column. |
| SHIFT + LEFT | Previous channel. |
| SHIFT + RIGHT | Next channel. |

The grid scrolls automatically to keep the cursor visible, in both rows and
channels.

## Note entry

The 16 trigger pads enter notes as a chromatic run of 16 semitones starting at
the root of the current octave (pad 0 = root, pad 12 = root an octave up).

| Control | Action |
|---------|--------|
| Pad 0–15 | Enter a note in the current cell (when REC is on); the cursor advances by the edit step. |
| SHIFT + lower pads (0–7) | Octave down. |
| SHIFT + upper pads (8–15) | Octave up. |

With REC off, a pad press auditions the pitch at the current octave without
changing the pattern.

When INSERT mode is on, entering a note pushes the rows below down and places a
note-off on the following row.

## Editing

| Control | Action |
|---------|--------|
| ERASE | Clear the current cell (note, instrument, volume, gate, effect). |
| SHIFT + ERASE | Clear the whole channel in the current pattern. |
| MUTE | Transpose the cell under the cursor down one semitone. |
| SHIFT + MUTE | Transpose the cell under the cursor up an octave. |
| CHORD | Write a note-off in the current cell. |
| SHIFT + CHORD | Write a note-cut in the current cell. |
| WRITE | Copy the current cell to the clipboard. |
| SHIFT + WRITE | Paste the clipboard at the cursor. |
| STEP JUMP | Insert a blank row at the cursor. |
| SHIFT + STEP JUMP | Delete the row at the cursor. |
| TAP | Toggle INSERT / OVERWRITE mode. |
| SHIFT + TAP | Undo the last edit. |

## Pattern and song

| Control | Action |
|---------|--------|
| PATTERN SET | Next pattern. |
| SHIFT + PATTERN SET | Previous pattern. |
| BAR 1–4 | Select pattern 0–3 directly. |
| SHIFT + BAR 1–4 | Set the edit step to 1–4. |

## Encoders

| Encoder | Action |
|---------|--------|
| MAIN / OSC | Adjust the value under the cursor. |
| SHIFT + MAIN / OSC | Move by rows. |
| CUTOFF | Change the octave. |
| MOD | Change the edit step. |
| IFX | Change the BPM (±5 per click). |

## Storage

| Control | Action |
|---------|--------|
| MENU | Save the song to the current slot. |
| SHIFT + MENU | Load the song from the current slot. |
| EXIT | Shut down the application. |

The slot number is fixed at 0 in the current build; the storage API already
accepts a slot index so a slot selector can be added without touching storage.
