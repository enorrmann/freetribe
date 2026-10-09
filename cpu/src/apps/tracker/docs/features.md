# Feature catalogue

This page lists what the tracker implements today, organised by area, and
calls out what is deliberately left out.

## Sequencer

| Feature | Status | Notes |
|---------|--------|-------|
| Event-based row decoding | Done | Each row yields NOTE_ON / NOTE_OFF / NOTE_CUT / ROW events. |
| Millisecond clock | Done | Fed by `ft_register_tick_callback(1, …)`. |
| BPM 20–400, 4 rows per beat | Done | `transport_set_bpm`. |
| Per-channel note tracking | Done | A new note releases the previous one. |
| Note gates | Done | `gate` byte counts rows; `0xFF` = legato. |
| Play / stop / pause | Done | STOP resets to row 0. |
| Playhead display | Done | The playing row is inverted in the grid. |
| Audition / preview | Done | The entered note sounds immediately. |
| Song mode (order list playback) | Partial | The order list exists and playback wraps it, but the editor exposes only pattern-level editing. |

## Editing

| Feature | Status | Notes |
|---------|--------|-------|
| Note entry from pads | Done | 16 pads = a chromatic run of 16 semitones (one octave and a third); writes only when REC is on. |
| Audition without recording | Done | With REC off a pad press sounds the pitch without editing. |
| Note-off / note-cut entry | Done | Written when a following empty row is needed. |
| Instrument column | Done | `FF` = inherit the channel's last instrument. |
| Velocity / volume column | Done | `FF` = inherit the channel's last volume. |
| Effect command column | Done | Two hex bytes per cell (`fxx`, `fxx_data`); no effects are executed yet. |
| Note editing | Done | The note value is edited as a whole note; the octave comes from the cursor's octave setting. |
| Octave selection | Done | Encoder or shift + pads. |
| Edit step | Done | Encoder, or shift + bar button (1–4). |
| Insert / overwrite mode | Done | INSERT pushes rows down and writes a note-off below. |
| Transpose | Done | MUTE / SHIFT + MUTE transpose the cell under the cursor. |
| Undo | Done | 5 levels, per-pattern snapshots. |
| Copy / cut / paste | Done | Cell-based: WRITE copies the current cell, SHIFT+WRITE pastes at the cursor. The block-selection API (`t_selection`, `editor_copy` / `editor_cut` over a rectangle) is implemented and ready for a future block-selection binding. |
| Row insert / delete | Done | STEP JUMP button. |
| Clear cell / channel | Done | ERASE button. |
| Column fill | Done | `editor_fill_column` (API; not bound to a panel control). |

The editor also exposes `editor_cut` (cut a selected block to the clipboard)
and `cursor_move_page` (page the cursor by one screen); these have no panel
binding yet but are part of the editing API.

## Display

| Feature | Status |
|---------|--------|
| Two-line header (pattern / order / BPM, octave / step / state) | Done |
| Scrolling pattern grid, two channels visible | Done |
| Row-number gutter in hex | Done |
| Focused-field inverse highlight | Done |
| Playing-row inverse highlight | Done |
| Status line (channel, effect, gate) | Done |
| Transient status messages | Done |

See [ui.md](ui.md) for the layout.

## Storage

| Feature | Status | Notes |
|---------|--------|-------|
| In-memory song slots | Done | 4 slots. |
| Save / load | Done | Volatile: slots are lost on power-off. |
| Flash persistence | Not implemented | The storage interface is designed for it. See [storage.md](storage.md). |

## LED feedback

| Feature | Status |
|---------|--------|
| Play / stop / record LEDs | Done |
| Bar LED follows the order position | Done |
| Pad LEDs show active channels | Done |
| Blue bar shows the edit step | Done |

## Known limitations

- **Effects are stored but not executed.** The FX column round-trips through
  the model and is shown in the status line, but no effect command (arp, slide,
  vibrato, …) is applied at playback yet.
- **Song / order editing is not exposed in the UI.** The order list is used by
  playback but cannot be edited from the panel yet.
- **No live recording from pads into a running pattern** (step entry only).
- **Storage is volatile.** See [storage.md](storage.md).

These are the natural next milestones; the module boundaries leave room for
each of them without touching unrelated code.
