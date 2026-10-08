# Geonkick CPU Application

The Geonkick percussion synthesizer for the Electribe 2, running on the
Freetribe firmware.

This application owns the whole instrument model: parameters, envelopes
and the panel interface.  It drives the audio module on the Blackfin
(`dsp/src/modules/geon/`) over the control link, sending it values only
when they change.

Geonkick is a free software percussion synthesizer by Iurie Nistor.  See
`dsp/src/modules/geon/doc/README.md` for what was ported and how.

## Architecture

This follows the Freetribe **monosynth example**
(`cpu/src/apps/monosynth/`), and that choice shapes everything else here:

> The CPU owns every envelope and parameter.  A 1 kHz tick evaluates
> them, and a value is sent to the DSP **only when it changes**.

Upstream Geonkick evaluates its envelopes on the audio side, inside the
DSP, because a desktop has the memory and the floating point headroom
for it.  The Blackfin does not, so the work moves here.  Two things
follow from that:

- The DSP holds no envelope point lists, which is what lets 16
  instruments fit in L1 (about 5 KB instead of 50 KB).
- The link carries control rate traffic, not audio rate.  A steady
  instrument sends nothing at all; only a moving envelope or a knob
  turn puts a message on the wire.

## Files

| File | Contents |
|------|----------|
| `geon.c` | The application: panel callbacks, default kit, patch display |
| `geon_control.c/.h` | Per-instrument state and the control rate evaluation |
| `geon_interface.c/.h` | Encoders for the messages the DSP understands |
| `geon_envelope.c/.h` | Envelope points and interpolation |
| `gui_task.c/.h` | Display task, copied from the monosynth example |
| `doc/README.md` | This file |

## The control tick

`app_init()` registers `_tick_callback()` for every kernel systick
(`ft_register_tick_callback(0, ...)`), and the callback calls
`geon_control_process()`.

That function walks all 16 instruments.  For each one that is sounding it
advances its time, computes the envelope position `time / length`, and
sends the value of each envelope when it has changed:

```c
control->time += 1.0f / CONTROL_RATE;
env_x = control->time / control->length;

value = geon_envelope_get_value(&control->env_amplitude, env_x);
if (_changed(value, control->last_amplitude)) {
    control->last_amplitude = value;
    geon_set_amplitude(instrument, value);
}
```

This is the `_cv_update()` pattern from the monosynth, with one change:
the threshold is **relative** rather than absolute.

```c
static bool _changed(float now, float last) {
    float scale = fmaxf(fabsf(now), 1.0f);
    return fabsf(now - last) > (CV_EPSILON * scale);
}
```

An absolute threshold does not work here because the values on the link
span several orders of magnitude: an amplitude is a fraction of 1, a
frequency is thousands of hertz.  A threshold tight enough for an
amplitude would send a frequency message on every single tick.  The last
value of each control starts at `-1`, so the first comparison always
reports a change and the initial value is always sent.

An instrument that reaches the end of its length is marked inactive and
stops being evaluated, which is what stops a quiet instrument from
costing anything.

## Envelopes

`geon_envelope.c` is the Geonkick envelope, ported to run on the CPU.  An
envelope is a short sorted list of points, up to
`GEON_MAX_ENVELOPE_POINTS`, and `geon_envelope_get_value()` interpolates
linearly between them.

Upstream also supports quadratic Bezier segments, flagged per point.
This port keeps the linear interpolation and the `apply_type` field,
which is what percussion envelopes need; the Bezier code was dropped with
the rest of the point editing, since the panel cannot draw or drag a
curve.

The value is looked up at `time / length`, the same x coordinate Geonkick
uses, so the curves behave the same as upstream at any instrument length.

Each instrument has an amplitude envelope.  Each oscillator has a
frequency envelope and a noise density envelope.  These are the three the
port keeps; upstream has more, for filter cutoff, resonance and pitch
shift, and they would be added in the same shape.

## Panel mapping

The Electribe 2S panel is mapped to the parameters Geonkick exposes.  The
index constants are named for the monosynth example, which is what the
`svc_panel` service reports.

### Trigger pads

| Control | Action |
|---------|--------|
| Pads 1-16 | Select and play the instrument, velocity scales its level |
| Pad release | Note off |

Selecting a pad also makes it the instrument the knobs and encoders edit,
and lights its LED.

### Encoders

| Control | Action |
|---------|--------|
| `ENCODER_OSC` (0x01) | Waveform of the current oscillator |
| `ENCODER_CUTOFF` (0x02) | Filter type, and switches the filter on |
| `ENCODER_MOD` (0x03) | What the `Mod Depth` knob edits |

### Knobs

| Control | Action | With Shift |
|---------|--------|------------|
| `KNOB_LEVEL` (0x00) | Instrument level | |
| `KNOB_PITCH` (0x02) | Oscillator frequency | |
| `KNOB_RES` (0x03) | Filter resonance | |
| `KNOB_EG` (0x04) | Filter cutoff | Amplitude envelope shape |
| `KNOB_ATTACK` (0x06) | Instrument length | Amplitude attack |
| `KNOB_DECAY` (0x08) | Pitch sweep amount | |
| `KNOB_MOD_DEPTH` (0x05) | Selected by `ENCODER_MOD` | |
| `KNOB_MOD_SPEED` (0x0a) | Pitch envelope depth | |

`KNOB_EG` and `KNOB_ATTACK` are the two knobs with a Shift function,
matching how the monosynth example splits attack and filter controls
across the same panel.

`ENCODER_MOD` selects what `KNOB_MOD_DEPTH` edits, from `e_mod_type`:

| Value | Edits |
|-------|-------|
| `MOD_NOISE_DENSITY` | Noise density of the oscillator |
| `MOD_FM` | Whether the oscillator modulates the next one |
| `MOD_OSC_LEVEL` | Level of the oscillator |

### Buttons

| Control | Action |
|---------|--------|
| `BUTTON_LPF` (0x12) | Low pass filter |
| `BUTTON_HPF` (0x14) | High pass filter |
| `BUTTON_BPF` (0x16) | Band pass filter |
| `BUTTON_AMP_EG` (0x20) | Toggle the oscillator; with Shift, select the oscillator |
| `BUTTON_SHIFT` (0x0a) | Second function of the knobs |
| `BUTTON_MENU` (0x09) | Print the current patch to the console |
| `BUTTON_PLAY` (0x13) | All notes off |

The three filter buttons enable the filter and select its type at the
same time, and light the matching LED.  `BUTTON_AMP_EG` follows the
monosynth, where the same button toggles a section and, with Shift,
changes which section the knobs edit; here that is the oscillator.

### LEDs

| LED | Meaning |
|-----|---------|
| `LED_OSC_ENC` | Oscillator 0 selected, or its enabled state |
| `LED_MAIN_ENC` | Oscillator 1 selected |
| `LED_LPF`, `LED_HPF`, `LED_BPF` | Active filter type |
| `LED_AMP_EG` | Current oscillator enabled |
| `LED_PAD_n_RED` | The pad being edited |

## Transformations between panel and protocol

The panel gives 7 bit knob values and single step encoder deltas; the DSP
wants frequencies, fractions and enumerators.  `geon.c` converts:

- `_knob_to_float()` maps 0...127 to 0...1.
- `_knob_to_frequency()` maps 0...127 logarithmically onto a range, which
  is what makes a frequency and a cutoff knob feel right.  Frequencies
  run `FREQ_MIN` 20 Hz to `FREQ_MAX` 8000 Hz, cutoffs `CUTOFF_MIN` 20 Hz
  to `CUTOFF_MAX` 16000 Hz.
- Encoders wrap around their enumeration by adding or subtracting one and
  testing against the matching `*_COUNT`.

The encoders arrive as `u8` where `1` means one step up and anything else
one step down, which `_encoder_callback()` turns into a signed delta.

## Default kit

`_patch_init()` builds a starting patch for each pad, and `app_init()`
sends each one to the DSP with `geon_control_send_all()` before any note
can be played.  That ordering matters: the incremental updates during
playback assume the DSP already knows the current values.

Each instrument gets a frequency spread over the pads:

```c
base = 60.0f * powf(2.0f, (float)instrument / 4.0f);   // 60 Hz upward
```

and a pitch sweep to a lower value, which is what shapes a kick.  The
rest is the default from `_init_instrument()`: length 300 ms, a sine
oscillator, level 0.9, filter off, distortion off.

Once the kit exists the application only edits the parameters the panel
can reach; there is no patch save or load, because the SD card driver is
not part of this branch.

## Memory

The 16 control instruments live in one `static t_geon_control
g_control[GEON_INSTRUMENTS]` in `.bss`:

| Type | Size |
|------|------|
| `t_geon_osc_control` | 276 B |
| `t_geon_control` | 672 B |
| all 16 instruments | 10752 B |

The CPU runs from DDR (`EXT_RAM`, 64 MB per `cpu/cpu.lds`), so this is a
small part of what is available, unlike the DSP side where the same
structure would not fit.

## Build

From the repository root, using the project's container:

```sh
docker-compose exec freetribe make APP=geon MODULE=geon
```

This builds the DSP module, converts it to an LDR and embeds it in the
CPU image as `bfin_ldr.h`, then builds the CPU binary.  Both `APP` and
`MODULE` must be passed, they default to `demo` and `default`.

## See also

- `dsp/src/modules/geon/doc/README.md` — the audio module this drives.
- `dsp/src/modules/geon/doc/protocol.md` — the message format between
  them.
- `cpu/src/apps/monosynth/` — the example this architecture follows.
