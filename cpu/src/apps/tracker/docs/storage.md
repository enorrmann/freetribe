# Storage

Save and load are handled by `storage/`. The current implementation keeps slots
in RAM; the interface is built so a flash (or SD card) backend can replace the
implementation without changing any caller.

## Interface

```c
void             storage_init(void);
e_storage_status storage_save(uint8_t slot, const t_song *song);
e_storage_status storage_load(uint8_t slot, t_song *song);
bool             storage_slot_valid(uint8_t slot);
const char      *storage_status_text(e_storage_status status);
```

`e_storage_status` is one of `STORAGE_OK`, `STORAGE_EMPTY` or `STORAGE_ERROR`.
The application shows the result in the status line (`SAVE SLOT 0 OK`).

## Current implementation

`storage.c` holds a static array:

```c
#define STORAGE_SLOTS 4

typedef struct {
    uint32_t magic;   // STORAGE_MAGIC = 0x54524b30 ("TRK0")
    t_song song;
} t_storage_slot;

static t_storage_slot g_slots[STORAGE_SLOTS];
```

- `storage_save` copies the song and stamps the magic number.
- `storage_load` checks the magic; a slot without it returns `STORAGE_EMPTY`.
- Slots are **volatile**: they are lost when power is removed. This is expected
  and stated in [features.md](features.md).

A `t_song` is roughly 192 KiB, so four slots occupy about 768 KiB of the 64 MiB
external RAM.

## Callers

`tracker.c` is the only caller. `MENU` saves to the current slot, `SHIFT + MENU`
loads it. On a successful load the cursor is clamped, the transport BPM is
refreshed, and the undo history is cleared (`undo_clear`), because the previous
history no longer describes the loaded song.

## Migration to flash

The Freetribe kernel exposes `flash_read`, `flash_write`, `flash_erase` and
`flash_verify` (`src/kernel/device/dev_flash.h`). To move to flash persistence:

1. **Choose a reserved region.** There is no documented user flash region today,
   so a base address and size must be decided before writing anything — writing
   the wrong sector can corrupt firmware. This is the one blocking decision.
2. **Serialise the song.** `t_song` is already flat (arrays of byte-sized
   structs with no pointers), so it can be written as one blob after a header:

   ```
   [ magic | version | length | song bytes ]
   ```

   A CRC over the song bytes lets `storage_load` reject a torn write.
3. **Implement the backend** inside `storage.c` only:

   ```c
   flash_erase(slot_base, FLASH_SECTOR);
   flash_write(slot_base, buffer, length);
   ```

   `flash_write` in the device driver already handles unaligned starts by doing
   a read-erase-write of the surrounding sector, but a slot should still be
   sector-aligned to avoid needless erases.
4. **Keep the UI identical.** Because `storage_save` / `storage_load` keep their
   signatures and status codes, `tracker.c` does not change.

Until step 1 is resolved, the RAM backend is the safe, correct behaviour.
