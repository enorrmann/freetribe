# Geonkick CPU / DSP Protocol

Reference for the message format between the Geonkick application on the
CPU (`cpu/src/apps/geon/`) and the Geonkick audio module on the DSP
(`dsp/src/modules/geon/`).

The two sides are separate firmwares, compiled by separate toolchains
(ARM and Blackfin), and the only link between them is this protocol, so
it is defined once here.

## Transport

The CPU sends a parameter with:

```c
ft_set_module_param(0, GEON_PARAM_BASE + param, message);
```

which the DSP receives in `module_set_param()` as:

```c
void module_set_param(uint16_t param_index, int32_t value);
```

- `module_id` is `0`, the DSP only runs one module.
- `param_index` is `GEON_PARAM_BASE + param`, where `param` is an
  `e_param` value.  Parameters below `GEON_PARAM_BASE` (`0x80`) are not
  handled by this module and are ignored.
- `message` is a 32 bit value carrying the instrument index, a target
  and the value itself.

`PARAM_COUNT` is the only parameter that is a count rather than a
command, and it must be identical on both sides.

## Message layout

```
 31      28 27      24 23                    0
+----------+----------+----------------------+
| instr    | target   | value                |
+----------+----------+----------------------+
```

| Field  | Bits  | Meaning                                    |
|--------|-------|--------------------------------------------|
| instr  | 31-28 | Instrument index, 0...15                   |
| target | 27-24 | Selector, meaning depends on the parameter |
| value  | 23-0  | Payload, encoding depends on the parameter |

The CPU builds it with `GEON_MESSAGE()`, the DSP splits it with
`_index()`, `_target()` and friends.

## Value encodings

Three encodings are used in the 24 bit value field.

### Fraction, 0...1

Used for amplitudes, noise density and output volume.

```
value = round(x * 16777215)      x is the parameter, 0...1
x     = value / 16777216
```

The CPU encoder is `_fix16()`, the DSP decoder is `_fix16()`.  The
decoder divides by 2^24 because the field is 24 bits wide, not 2^16.
A mismatch here is silent and severe: encoding 1.0 as 16.16 and decoding
as 24 bit gives 0.0078, roughly 128 times too quiet.

### Milli units

Used for values that cover a wide range: frequencies, filter Q and
distortion drive.  The parameter is scaled by 1000, so a frequency is
carried in millihertz.

```
value = round(x * 1000)          x is the parameter
x     = value / 1000
```

The CPU encoder is `_milli()`.  It clamps to 16000, because
`16000 * 1000` is the largest value that still fits in 24 bits, and a
silent wrap would be worse than a clamp.  Frequencies above 16 kHz are
not useful for percussion.

Q is carried this way too, and is not a fraction, it ranges from 0.5 to
10.

### Enumerator

Used for waveform and filter type.  The value is placed in bits 23-16,
which the DSP reads with `_byte()`.

```
value = enum << 16
enum  = (value >> 16) & 0xFF
```

Only the low byte of the field is read, so an enumerator must be less
than 256.

## Flags and selectors in the target field

The target field is 4 bits, so a parameter that needs both a selector
and a flag packs them together.  This is the convention used throughout:

| Bit | Meaning                                  |
|-----|------------------------------------------|
| 0   | Oscillator index, 0 or 1                 |
| 3   | Flag, for the parameters that carry one  |

A parameter that carries a flag sends `osc | (flag << 3)`.  The DSP
reads the oscillator with `target & 0x01` and the flag with
`target & 0x08`.

A parameter whose value is an enumerator that also needs a selector
(such as the oscillator filter type) sends the selector in the target
and the enumerator in the value, so the two never collide.

## Messages without an instrument index

`PARAM_ALL_NOTES_OFF` is global.  Its value field is ignored and the
instrument index field is not read, the DSP stops every instrument.

`PARAM_NOTE_ON` triggers the instrument named in the index field.  The
rest of its message is unused.

## Parameter list

Indices must match between `e_param` in `dsp/src/modules/geon/geon.c`
and `e_param` in `cpu/src/apps/geon/geon_interface.h`.  Both enums are
in the same order; `PARAM_COUNT` closes each one.

| Param | Target | Value | Meaning |
|-------|--------|-------|---------|
| `PARAM_INSTRUMENT_LENGTH` | unused | milli units, seconds | Instrument length, 0.001...2 s |
| `PARAM_INSTRUMENT_AMPLITUDE` | unused | fraction | Instrument level |
| `PARAM_INSTRUMENT_FILTER_ENABLED` | bit 0 flag | unused | Global filter on/off |
| `PARAM_INSTRUMENT_FILTER_TYPE` | enumerator | unused | Global filter type |
| `PARAM_INSTRUMENT_FILTER_CUTOFF` | unused | milli units, Hz | Global filter cutoff |
| `PARAM_INSTRUMENT_FILTER_Q` | unused | milli units | Global filter Q |
| `PARAM_DISTORTION_ENABLED` | bit 0 flag | unused | Distortion on/off |
| `PARAM_DISTORTION_DRIVE` | unused | milli units | Distortion drive |
| `PARAM_DISTORTION_VOLUME` | unused | fraction | Distortion output volume |
| `PARAM_OSC_ENABLED` | osc, bit 3 flag | unused | Oscillator on/off |
| `PARAM_OSC_FUNC` | osc | enumerator | Waveform |
| `PARAM_OSC_FREQUENCY` | osc | milli units, Hz | Frequency |
| `PARAM_OSC_AMPLITUDE` | osc | fraction | Amplitude |
| `PARAM_OSC_NOISE_DENSITY` | osc | fraction | Noise density |
| `PARAM_OSC_FM` | osc, bit 3 flag | unused | Oscillator 0 modulates 1 |
| `PARAM_OSC_FILTER_ENABLED` | osc, bit 3 flag | unused | Oscillator filter on/off |
| `PARAM_OSC_FILTER_TYPE` | osc | enumerator | Oscillator filter type |
| `PARAM_OSC_FILTER_CUTOFF` | osc | milli units, Hz | Oscillator filter cutoff |
| `PARAM_OSC_FILTER_Q` | osc | milli units | Oscillator filter Q |
| `PARAM_NOTE_ON` | unused | unused | Trigger the instrument |
| `PARAM_ALL_NOTES_OFF` | unused | unused | Stop all instruments |

## Enumerations

These are mirrored in `geon_interface.h` on the CPU and `geon_types.h`
on the DSP.

| Waveform | Value | Filter type | Value |
|----------|-------|-------------|-------|
| `GEON_OSC_FUNC_SINE` | 0 | `GEON_FILTER_LOW_PASS` | 0 |
| `GEON_OSC_FUNC_SQUARE` | 1 | `GEON_FILTER_HIGH_PASS` | 1 |
| `GEON_OSC_FUNC_TRIANGLE` | 2 | `GEON_FILTER_BAND_PASS` | 2 |
| `GEON_OSC_FUNC_SAWTOOTH` | 3 | | |
| `GEON_OSC_FUNC_NOISE_WHITE` | 4 | | |
| `GEON_OSC_FUNC_NOISE_BROWNIAN` | 5 | | |

`GEON_OSC_FUNC_COUNT` and `GEON_FILTER_TYPE_COUNT` close each
enumeration and are used to wrap around when an encoder steps past the
end.

## Invariants

- The instrument index must be less than `GEON_INSTRUMENTS` (16).  The
  DSP drops any message that is not, so a bad index cannot corrupt
  another instrument's state.
- The oscillator index must be less than `GEON_OSCILLATORS` (2).  The
  DSP returns a null pointer from `_get_oscillator()` and drops the
  message.
- The DSP never allocates.  Every value it receives is stored in a plain
  `float` or `int` member of its module state, so a malformed message
  can only produce a wrong value, never a fault.
- The DSP applies the values it is given.  It does not evaluate
  envelopes; the CPU sends the envelope result each control tick, see
  `cpu/src/apps/geon/doc/README.md`.

## Checking the two sides agree

The enums are the easiest thing to get out of step, because nothing in
the build compares them.  A quick check, from the repository root:

```sh
sed -n '/^typedef enum {/,/^} e_param;/p' dsp/src/modules/geon/geon.c \
    | grep -oP 'PARAM_\w+' > /tmp/dsp_params.txt

sed -n '/^typedef enum {/,/^} e_param;/p' \
    cpu/src/apps/geon/geon_interface.h \
    | grep -oP 'PARAM_\w+' > /tmp/cpu_params.txt

diff /tmp/dsp_params.txt /tmp/cpu_params.txt
```

An empty diff means the names, and therefore the indices, match.
