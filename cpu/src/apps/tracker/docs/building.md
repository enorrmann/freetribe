# Building and running

## Prerequisites

The Freetribe Docker image provides the `arm-none-eabi` toolchain.

```
docker-compose up -d          # start the build container
```

## Build the tracker

```
docker-compose exec freetribe make APP=tracker -C cpu
```

The `APP` variable selects the application directory under `cpu/src/apps/`. The
output is:

```
cpu/build/cpu.bin     # flashable image
cpu/build/cpu.elf     # ELF with debug symbols
```

### Clean build

```
docker-compose exec freetribe make clean -C cpu
docker-compose exec freetribe make APP=tracker -C cpu
```

## How the app is discovered

`cpu/Makefile` collects every `.c` / `.cpp` / `.S` file under
`cpu/src/apps/$(APP)` and adds every sub-directory of the app to the include
path. New modules added under `model/`, `engine/`, `ui/` or `storage/` are
compiled automatically — no Makefile change is needed.

## Compiler flags that matter

| Flag | Effect |
|------|--------|
| `-Wall` | The app builds with **no warnings**. |
| `-fanalyzer` | Static analysis of the app code. |
| `-fstack-usage -Wstack-usage=128` | Warns on stack use above 128 bytes per frame. |
| `-Og -g3` | Debug-friendly optimisation with full symbols. |

The kernel and third-party libraries emit a handful of pre-existing warnings
(`svc_dsp.c`, `svc_midi.c`, `startup.c`, `csl_cppi41dma.c`, `ugui.c`, LEAF). The
tracker application itself contributes none.

## Flashing

The `.bin` is flashed with the project's normal Electribe flashing flow. This
application uses only the documented Freetribe API (`freetribe.h`), so it needs
no special device support.

## Testing on hardware

1. Flash `cpu/build/cpu.bin`.
2. On boot the screen shows the header, an empty grid and `TRACKER READY`.
3. Press pads to enter notes; press PLAY to hear them sequenced over MIDI.
4. `MENU` saves to slot 0, `SHIFT + MENU` loads it back.

## Layout of the build

```
cpu/
├── Makefile            selects APP, discovers sources
├── build/              object files and output binaries
└── src/apps/tracker/   this application
```
