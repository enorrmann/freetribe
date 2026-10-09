# Architecture

The tracker is split into four vertical layers plus the application glue. The
guiding rule is **one-way dependencies**: a layer may use the layers below it,
never the ones above it.

```
        ┌────────────────────────────────────────────┐
        │                tracker.c                    │  application glue
        │  panel callbacks · commands · render loop   │
        └───────┬──────────────┬──────────────┬───────┘
                │              │              │
        ┌───────▼──────┐ ┌─────▼──────┐ ┌─────▼───────┐
        │  editor.c    │ │ engine/    │ │ ui/         │
        │  edit cmd    │ │ sequencer  │ │ gui_task    │
        │              │ │ transport  │ │ theme       │
        │              │ │ note_router│ │ tracker_view│
        └───────┬──────┘ └─────┬──────┘ └─────┬───────┘
                │              │              │
                └──────────────┴──────┬───────┘
                                     │
                          ┌──────────▼──────────┐
                          │       model/        │  pure data
                          │ song · cursor       │
                          │ clipboard · undo    │
                          │ tracker_types       │
                          └─────────────────────┘

                          storage/  (used by tracker.c only)
```

## Responsibilities

| Module | Owns | Must not |
|--------|------|----------|
| `model/` | The song data, the cursor, selections, undo history. | Touch hardware, MIDI or the display. |
| `engine/` | Timing, row decoding, MIDI output. | Mutate song data other than per-channel preview state; draw. |
| `ui/` | Drawing to the dot-matrix. | Mutate the song or the cursor. |
| `editor/` | Edit commands, undo snapshots, insert mode. | Talk to hardware or MIDI. |
| `storage/` | Serialising the song into slots. | Know about rendering or the sequencer. |
| `tracker.c` | Wiring, input translation, the `needs_redraw` flag. | Contain model or drawing logic. |

## Data flow

### Input → model

```
panel callback            editor command            model
─────────────────►  tracker.c  ───────────►  editor.c  ───────────►  song/cursor
                      │                                              │
                      └──────────► needs_redraw = true ──────────────┘
```

Every panel event is translated into an `editor` command or a cursor move, sets
`needs_redraw`, and returns. No drawing happens inside a callback.

### Clock → engine → MIDI → display

```
1 ms tick ─► sequencer_tick ─► transport_tick ─(row due?)─► _process_row
                                                            │
                               ┌────────────────────────────┤
                               ▼                            ▼
                     note_router ─► ft_send_note_on   seq listener
                     note_router ─► ft_send_note_off  (tracker.c)
                                                            │
                                                            ▼
                                                    needs_redraw = true
```

The sequencer emits `t_seq_event`s to a single listener registered by
`tracker.c`. The listener decides whether the visible playhead moved and raises
`needs_redraw`. This keeps the engine free of display knowledge.

### Render

```
app_run ─► gui_task()          (init uGUI on first call)
        └► if needs_redraw:
              tracker_view_render(&view)   (pure function of view state)
```

`t_view_state` is a plain struct filled in `_render_view()`. The view is a pure
function of that struct plus the song, so it can be exercised and changed
independently of the engine.

## Why these boundaries

- **Engine without display.** The row decoder can be unit-tested or driven by a
  MIDI file player later, unchanged.
- **View without state.** Because the view never mutates anything, "what does
  the screen show" is a single function call to reason about.
- **Model without hardware.** `model/` includes only `<stdint.h>` and
  `<string.h>`, so it compiles and can be tested on the host.
- **Storage behind an interface.** `storage_save` / `storage_load` are the only
  entry points, so swapping RAM for flash touches one file.

## Build integration

The Freetribe `cpu/Makefile` discovers every `.c` file under the app directory
and adds every sub-directory to the include path, so the `model/`, `engine/`,
`ui/` and `storage/` folders are picked up automatically. No build changes are
required to add a new module.
