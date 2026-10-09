# Tracker

A pattern-based, event-driven step sequencer for the Electribe 2 / 2S, built
as a Freetribe application. It brings a classic tracker workflow — a scrolling
grid of patterns, hex columns and a moving playhead — to the 128×64 dot-matrix
display, driving external MIDI gear one note event at a time.

```
docker-compose exec freetribe make APP=tracker -C cpu
```

The build produces `cpu/build/cpu.bin`.

---

## What it does

- **Event-based sequencer.** A row is decoded into a stream of note / note-off
  / note-cut / effect events. Events are delivered to a listener and routed to
  MIDI. The engine never touches the display and the display never touches the
  song data.
- **16 channels** of 64-row patterns, with **32 patterns** and a **128-entry
  song order list** (the order list is prepared but the current UI plays the
  pattern under the cursor in a loop).
- **MIDI only output.** Notes are sent with `ft_send_note_on` / `ft_send_note_off`
  on the matching MIDI channel (tracker channel *n* → MIDI channel *n+1*), so the
  tracker can drive the internal synth or any external device.
- **Editing**: note entry from the pads, velocity / instrument columns, an
  effect command column, insert and overwrite modes, edit-step, octave,
  transpose, and undo.
- **Modern workflow**: copy / cut / paste of blocks, row insert / delete,
  column fill, save / load to slots, transient status messages.

See [features.md](features.md) for the complete feature list and the
[control reference](controls.md) for the button map.

---

## Documentation index

| Document | Contents |
|----------|----------|
| [features.md](features.md) | Feature catalogue and current limitations |
| [controls.md](controls.md) | Button / pad / encoder reference |
| [ui.md](ui.md) | Display layout and what each field shows |
| [architecture.md](architecture.md) | Module layering and data flow |
| [data-model.md](data-model.md) | Song / pattern / cell structures |
| [sequencer.md](sequencer.md) | Timing, playback and note routing |
| [storage.md](storage.md) | Save / load slots and the flash migration path |
| [building.md](building.md) | Building and running |

---

## Source layout

```
cpu/src/apps/tracker/
├── tracker.c            Application entry point and input handling
├── editor.c/.h          Editing commands (mutate the model)
├── led_view.c/.h        Panel LED feedback
├── panel_buttons.h      Panel control indices
├── model/               Pure data: no hardware, no GUI
│   ├── tracker_types.*  Cell / pattern value types and note formatting
│   ├── song.*           Patterns, order list, global settings
│   ├── cursor.*         Edit position, viewport scroll
│   ├── clipboard.*      Block selection, copy / cut / paste
│   └── undo.*           Multi-level undo (pattern snapshots)
├── engine/              Sequencer: no display, no editing
│   ├── transport.*      Play / stop / pause, BPM to tick timing
│   ├── sequencer.*      Row decoding into events
│   └── note_router.*    Events to MIDI, per-channel note state
├── ui/                  Presentation: no song mutation
│   ├── gui_task.*       uGUI wrapper and drawing primitives
│   ├── theme.h          Layout constants
│   └── tracker_view.*   Full-screen renderer
└── storage/             Save / load
    └── storage.*        Volatile slots, flash-ready interface
```

The dependency rule is one-way:

```
model  ←  editor  ←  tracker (app)
model  ←  engine  ←  tracker
model  ←  ui      ←  tracker
storage ←  tracker
ui     ←  tracker  (drives gui_task)
```
