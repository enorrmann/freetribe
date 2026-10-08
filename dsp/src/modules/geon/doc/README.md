# Geonkick DSP Module

The Geonkick percussion synthesizer, ported to the Blackfin DSP of the
Electribe 2 for the Freetribe firmware.

Geonkick is a free software percussion synthesizer by Iurie Nistor,
capable of generating kicks, snares, claps, hi-hats, shakers and unique
effect sounds.  This directory is a port of its DSP[1] onto the small
Blackfin, driven by a Freetribe CPU application
(`cpu/src/apps/geon/`).

[1] <https://github.com/Geonkick-Synthesizer/geonkick>
    Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later.

## Architecture

This module follows the Freetribe **monosynth example**, and that
choice shapes everything else here:

> The CPU owns every envelope, LFO and parameter.  It evaluates them at
> control rate and sends the DSP a value **only when it changes**.  The
> DSP is a pure sample generator.

Upstream Geonkick takes the opposite approach: it keeps the envelopes on
the audio side and renders a whole kick into a buffer whenever a
parameter changes, then swaps that buffer into the audio output.  That
does not fit here, for two reasons:

1. **Memory.**  Upstream keeps three oscillators, three envelope point
   lists and a filter per oscillator, plus a kick buffer, per
   instrument.  On the Blackfin that is about 50 KB of state for 16
   instruments, against 32 KB of L1 data A and 32 KB of L1 data B
   available to the whole kernel.  Moving the envelopes to the CPU and
   rendering sample by sample removes the point lists and the buffer and
   brings the state to about 5 KB.
2. **Latency.**  The Electribe DSP runs one audio frame per interrupt, so
   generating the sample that is needed now is both simpler and lower
   latency than rendering a buffer ahead of time.

What the DSP keeps is only what a sample needs: oscillator phases, filter
state, and the current values of the parameters the CPU has sent.

## Files

| File | Contents |
|------|----------|
| `geon.c` | Freetribe module API, parameter messages, instrument mix and master gain |
| `geon_types.h` | Types and constants shared by the rest of the module |
| `geon_oscillator.c/.h` | Waveforms: sine, square, triangle, sawtooth, white and Brownian noise |
| `geon_filter.c/.h` | Digital state variable filter, low/high/band pass |
| `geon_instrument.c/.h` | One instrument: oscillators, FM, filter, distortion |
| `doc/protocol.md` | The CPU / DSP message format |
| `doc/README.md` | This file |

## The module API

`dsp/src/kernel/module.h` defines the four functions the kernel calls,
plus two it does not:

| Function | Purpose |
|----------|---------|
| `module_init()` | Build the 16 instruments |
| `module_process(fract32 *in, fract32 *out)` | Render one stereo frame |
| `module_set_param(index, value)` | Receive a parameter message |
| `module_get_param_count()` | Number of parameters |
| `module_get_param_name(index, text)` | Name of a parameter, for a host UI |
| `module_get_param(index)` | Read back a parameter |

The module is built by selecting it at build time:

```sh
make APP=geon MODULE=geon
```

`module_process()` sums every active instrument into `mix`, applies
`GEON_MASTER_GAIN`, clamps to `[-1, 1]` and writes the same value to both
channels.  Instruments that reach the end of their length are marked
inactive and skipped until the next note on.

## Synthesis

### Oscillator

An oscillator holds a phase in radians, advanced once per sample:

```
phase += (2 * pi * f) / sample_rate
```

where `f` is the frequency the CPU sent, adjusted for FM:

```
f += f * fm_k * fm_input
```

The first oscillator of an instrument can frequency modulate the second
one, as upstream.  Its output is stored in the second oscillator's
`fm_input` instead of being summed into the instrument, and the second
oscillator applies it on the next sample.

The waveforms are the upstream ones:

| Waveform | Expression, phase in radians |
|----------|------------------------------|
| Sine | `sin(phase)` |
| Square | `-1` below pi, `1` above |
| Triangle | `-1 + 2/pi * phase` below pi, `3 - 2/pi * phase` above |
| Sawtooth | `phase/pi` below pi, `phase/pi - 2` above |
| White noise | random, gated by density |
| Brownian noise | random walk, gated by density |

The noise generators use a xorshift32 PRNG seeded from the oscillator
seed, where upstream uses `rand()`.  The point is reproducibility: a
given seed gives the same noise on every run and on both sides of the
port.

Each oscillator can have its own filter, applied to its output before it
reaches the instrument mix.

### Filter

A digital state variable filter, the same one Geonkick uses for
oscillators and for the instrument output:

```
F = 2 * sin(pi * cutoff / sample_rate)
Q = 10 / factor          clamped to 10

h = in - l_prev - Q * b_prev
b = F * h + b_prev
l = F * b + l_prev
```

`out` is `h`, `b` or `l` for high, band or low pass.  The CPU sends the
cutoff and the user facing Q each tick, so the two other filter outputs
are free.

The filter is unstable for extreme cutoff and Q combinations, which
upstream notes and treats as a feature.  The instrument output is
clamped, and `geon_filter_val()` replaces a NaN with silence, so an
unstable setting cannot leave the module producing NaNs.

### Instrument

The instrument renders one sample:

```
mix = 0
for each enabled oscillator:
    if oscillator 0 is an FM source:
        osc[1].fm_input = osc[0].value()      # not summed
    else:
        mix += osc.value()                    # already scaled by its amplitude
    osc.increment_phase()

mix *= amplitude                              # CPU amplitude envelope value
if filter_enabled:   mix = filter(mix)
if distortion:       mix = tanh(drive * mix) * volume
clamp(mix, -1, 1)
```

This mirrors `gkick_synth_get_value()` upstream, minus the envelope
lookups, which now happen on the CPU.

`amplitude`, `filter_cutoff`, `filter_q` and `distortion_drive` are plain
members that the CPU updates over the control link; the DSP reads them
as it renders.

`GEON_DISTORTION_SOFT_CLIPPING_TANH` is the upstream default and the only
curve used here.  Upstream offers eight; the others were dropped to keep
the module small, and they are listed here so a future patch can add
them back in one place.

## State and memory

The whole module state is one `static t_module g_module`, holding 16
instruments.  It is zero-initialised and lives in `.bss`, in the L1 data
A bank:

| Type | Size |
|------|------|
| `t_geon_filter` | 52 B |
| `t_geon_oscillator` | 104 B |
| `t_geon_instrument` | 304 B |
| all 16 instruments | 4864 B |

The link reports `.bss` at about 7.6 KB in total, well inside the 32 KB
bank.  **No linker script change was needed**, and none should be made:
`dsp/dsp.lds` is shared with every other app in the repository.

The module never allocates.  There is no `malloc`, no pool, and no
buffer: the sample a parameter affects is the sample it is applied to.

## Bounds and safety

The DSP trusts the CPU, but not blindly:

- An instrument index at or above `GEON_INSTRUMENTS` is dropped before
  any state is touched.
- An oscillator index at or above `GEON_OSCILLATORS` makes
  `_get_oscillator()` return `NULL`, and the caller checks it.
- Every incoming value is clamped to a sane range, so a malformed
  message yields a wrong sound, never a fault or a NaN.
- `module_set_param()` ignores any `param_index` below
  `GEON_PARAM_BASE`, so other modules and kernel messages pass through
  untouched.

## Porting notes

Kept from upstream, because they are what make a Geonkick instrument
sound like itself:

- The phase accumulation and the FM relation between the two
  oscillators.
- The waveform expressions, including the pi breakpoints.
- The white noise density gate and the Brownian random walk.
- The state variable filter, its `2 * sin(...)` tuning and the `10/Q`
  factor convention.
- `tanh` soft clipping as the default distortion.

Changed for this port:

- **Envelopes moved to the CPU.**  The DSP receives values, not curves.
- **Sample by sample rendering.**  No kick buffer, no worker thread, no
  locking; the module runs entirely inside the audio callback.
- **Deterministic PRNG** instead of `rand()` for the noise generators.
- **Two oscillators, one layer, per instrument**, and 16 instruments, to
  fit the 16 trigger pads and the available memory.  Upstream has three
  oscillators and three layers.
- **No sample playback.**  `GEON_OSC_FUNC_SAMPLE` and the wav/ogg/flac
  loading depend on file I/O and a resampler, neither of which exists
  here.  Noise waveforms cover the same ground for percussion.

Dropped, with no replacement:

- The kit and preset JSON handling, the LV2/VST3 and JACK plumbing, the
  worker thread and its ring buffers, and the UI.  All of that is host
  side and lives on the CPU instead.

## See also

- `doc/protocol.md` — the message format, parameter table and
  invariants.
- `cpu/src/apps/geon/doc/README.md` — the CPU side that drives this
  module.
- `dsp/src/modules/monosynth/` — the example this architecture follows.
