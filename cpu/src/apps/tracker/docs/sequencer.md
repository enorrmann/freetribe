# Sequencer

The engine layer (`engine/`) turns time into events and events into MIDI. It is
split into three small modules so each has a single reason to change.

```
tick (ms) ──► transport ──(row due?)──► sequencer ──► note_router ──► MIDI
                                          │
                                          └──► t_seq_event ─► listener (app)
```

## Transport (`transport.c`)

The transport owns *time only*.

- **State**: `STOPPED`, `PLAYING`, `PAUSED`.
- **Position**: `order_index` (position in the song order) and `row` (row in
  the current pattern).
- **Tempo**: `tick_period` milliseconds per row, computed as

  ```
  tick_period = 60000 / (bpm * ROWS_PER_BEAT)
  ```

  with `ROWS_PER_BEAT = 4` (sixteenth-note grid).

- `transport_tick(ms)` accumulates milliseconds and returns `true` when a row is
  due. It subtracts `tick_period` rather than resetting to zero, so the
  remainder carries over and the tempo does not drift.
- `transport_next_row()` advances the row and wraps to the next order entry, and
  to the start of the song when the order list ends.

The transport does not know anything about notes. It reports "there is a row".

## Sequencer (`sequencer.c`)

The sequencer decodes the current row.

```c
typedef struct {
    e_seq_event_type type;   // ROW / NOTE_ON / NOTE_OFF / NOTE_CUT / STOP
    uint8_t order_index;
    uint8_t pattern;
    uint8_t row;
    uint8_t channel;
    uint8_t value;
} t_seq_event;
```

On each due row it:

1. caches the playing order, pattern and row (for the playhead),
2. emits a `ROW` event,
3. for every channel, reads the cell and:
   - `NOTE_OFF`  → release the channel's note, emit `NOTE_OFF`,
   - `NOTE_CUT`  → cut the channel's note, emit `NOTE_CUT`,
   - pitch       → inherit missing instrument/volume from the channel state,
                   start the note, emit `NOTE_ON`,
   - empty       → nothing.

Events are handed to a single listener registered with
`sequencer_set_listener()`. The application uses this to raise a redraw when the
playhead is visible.

`sequencer_preview()` auditions a single cell for edit-time feedback without
starting the transport.

## Note router (`note_router.c`)

The router is the only code that talks MIDI.

```c
typedef struct {
    uint8_t note, velocity, gate_remaining;
    bool active;
} t_router_channel;   // one per channel
```

- `router_note_on()` releases any note already sounding on the channel, then
  sends `ft_send_note_on`. A gate of `0xFF` means legato (sustain until
  replaced); any other value is a row countdown.
- `router_advance_row()` counts gates down by one row and releases the notes
  that reach zero.
- `router_note_off()` / `router_note_cut()` send `ft_send_note_off` (velocity
  0x40 for a release, 0 for a cut).
- `router_all_notes_off()` is called on stop to avoid stuck notes.

Channel mapping: tracker channel *n* maps to MIDI channel *n* + `MIDI_CHANNEL_BASE`
(currently 0, i.e. channels 1–16).

## Timing walkthrough

```
bpm = 120, ROWS_PER_BEAT = 4
tick_period = 60000 / (120 * 4) = 125 ms per row
                ↓ every 125 ms
SEQ_EVENT_ROW(row N) → note events for row N → router sends MIDI
                ↓
transport_next_row() advances to N+1
```

## Adding an effect

Effects are decoded in `_process_channel()` in `sequencer.c`. To implement one:

1. read `cell->fxx` / `cell->fxx_data`,
2. add a case that emits the appropriate events or calls into the router,
3. keep any effect state in `t_channel_state` or a small effect table.

No other module needs to change: the cell already stores and displays the FX
bytes, and the router already exposes note on/off/cut.
