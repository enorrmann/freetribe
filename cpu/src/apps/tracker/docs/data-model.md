# Data model

All data types live in `model/`. They depend only on `<stdint.h>` and
`<string.h>`, so the model can be compiled and tested on the host.

## Cell

A cell is the smallest editable unit. It is byte-sized so a pattern is a flat,
trivially copyable array.

```c
typedef struct {
    uint8_t note;       // MIDI note, or NOTE_NONE / NOTE_OFF / NOTE_CUT
    uint8_t velocity;   // 0x00..0x7f, 0xff = inherit channel volume
    uint8_t gate;       // 0x01..0xfe rows, 0x00 and 0xff = legato
    uint8_t instrument; // 0x00..0x7f, 0xff = inherit channel instrument
    uint8_t fxx;        // effect command (0x00 = none)
    uint8_t fxx_data;   // effect parameter
} t_cell;
```

### Reserved note values

| Value | Format | Meaning |
|-------|--------|---------|
| `0x00` | `---` | Empty cell — nothing happens on this row. |
| `0x01`–`0x7f` | `C-4` … | Audible note. |
| `0xfe` | `OFF` | Note off: release the channel's current note. |
| `0xff` | `CUT` | Note cut: hard stop. |

`note_is_pitch()` distinguishes audible notes from the three control values.

### Field width

MIDI notes are 0–127, but the special values sit above them, so a note byte is
fully used. Velocity and instrument use `0xFF` as the "inherit" sentinel; the
value is written on first edit and inherited from the channel state afterwards.

## Pattern

```c
typedef struct {
    t_cell cell[TRACKER_CHANNELS][TRACKER_ROWS]; // [16][64]
} t_pattern;
```

A pattern is 16 × 64 cells = 6 KiB. There are 32 patterns (192 KiB), which is a
small fraction of the 64 MiB external RAM.

## Song

```c
typedef struct {
    t_pattern pattern[TRACKER_PATTERNS]; // 32 patterns
    uint8_t order[TRACKER_SONG_LENGTH];  // 128-entry play order
    uint8_t order_length;
    uint16_t bpm;
    uint8_t rows;   // active pattern length (1..64)
    uint8_t step;   // default edit step
    t_channel_state channel[TRACKER_CHANNELS];
} t_song;
```

`t_channel_state` remembers, per channel, the last note, instrument, volume and
gate. Empty fields inherit these values, which is what makes tracker-style
"column inheritance" feel natural.

### Accessors

All access goes through accessors that take the modulo of their indices, so
callers can wrap freely without bounds bugs:

```c
t_cell *song_cell(t_song *song, uint8_t pattern, uint8_t channel, uint8_t row);
void    song_set_cell(t_song *song, uint8_t pattern, uint8_t channel,
                      uint8_t row, const t_cell *cell);
```

### Order list

`order[]` is the sequence of patterns to play. Playback walks the pattern
indices in order and wraps to the start. `song_order_insert()` and
`song_order_delete()` maintain the list; the UI does not expose them yet.

## Cursor

```c
typedef struct {
    uint8_t pattern;      // pattern being edited
    uint8_t row;          // row 0..(rows-1)
    uint8_t channel;      // 0..15
    uint8_t column;       // COL_NOTE / COL_INS / COL_VOL / COL_FX
    uint8_t step;         // edit step
    uint8_t octave;       // note-entry octave
    uint8_t chan_scroll;  // leftmost visible channel
    uint8_t row_scroll;   // topmost visible row
    bool insert;          // insert mode
} t_cursor;
```

The cursor is the single source of truth for "where the user is". It stores the
scroll offsets so the viewport is stable across redraws without the view having
to remember anything.

## Clipboard and selection

```c
typedef struct {           // rectangular block selection
    uint8_t pattern, chan_start, chan_end, row_start, row_end;
    bool active;
} t_selection;

typedef struct {           // clipboard payload
    t_cell cell[TRACKER_CHANNELS][TRACKER_ROWS];
    uint8_t chans, rows;
    bool has_data;
} t_clipboard;
```

Copy captures the normalised rectangle into a compact buffer; paste writes it
back at the cursor, wrapping modulo the pattern size.

## Undo

```c
typedef struct {
    t_undo_entry entry[UNDO_LEVELS]; // 5 snapshots
    uint8_t head, count;
} t_undo_stack;
```

Undo snapshots the whole pattern being edited before each destructive edit.
A pattern is 6 KiB, so five levels cost 30 KiB — trivial, and it makes the
restore logic a single array copy instead of an inverse-operation puzzle.

## Sizes and limits

| Constant | Value | Where |
|----------|-------|-------|
| `TRACKER_CHANNELS` | 16 | `tracker_types.h` |
| `TRACKER_ROWS` | 64 | `tracker_types.h` |
| `TRACKER_PATTERNS` | 32 | `tracker_types.h` |
| `TRACKER_SONG_LENGTH` | 128 | `tracker_types.h` |
| `UNDO_LEVELS` | 5 | `undo.h` |
| `STORAGE_SLOTS` | 4 | `storage.h` |
