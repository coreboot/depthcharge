# bmpblk: Bitmap sources for firmware UI

**Note**: Because the bitmaps are stored in RO firmware, back-porting any
new bitmaps to already shipped devices is not possible.


## Building bitmaps

To build images for board $BOARD with default locales, do:

```
(chroot) cd ~/trunk/src/platform/bmpblk
(chroot) BOARD="$BOARD" make
```

To override the locale list defined in `boards.yaml` (for instance, to build
with only English locale to speed up testing flow), pass `LOCALES=<locale-list>`
as an environment variable. For example,

```
(chroot) LOCALES="en ja es" BOARD="$BOARD" make
```

The default output folder is `./build/$BOARD`. To override output folder,
specify `OUTPUT=<path_to_output>` as an environment variable.


## Adding a new target board

Add an entry for the new board in `boards.yaml`. See the description at the
top of `boards.yaml`. For example, add the following for board `link`:

```
link:
  screen: [1920, 1080]
  dpi: 112
  # List of locales to include.
  locales: [en, es-419, pt-BR, fr, es, it, de, nl, da, 'no', sv, ko, he]
  # Right-to-left locales.
  rtl: [he]
```

**Note**: The locale `no` will be interpreted as boolean False in YAML, so we
need to quote it as `'no'`.

If your configuration is exactly the same as existing ones, add your new board
into the existing entry. For example:

```
asurada,link:
  screen:   [1920, 1080]
  dpi: 112  # DO NOT COPY-PASTE -- follow instructions at top of file.
```

## Bitmaps in firmware image

After emerging `chromeos-bmpblk`, bitmaps will be stored in the following files:

1. `vbgfx.bin`: archive of generic (locale-independent) bitmaps
2. `locale_${LOCALE}.bin`: archive of bitmaps for locale `${LOCALE}`
3. `font.bin`: archive of glyph bitmaps

These archive files for Chromium OS firmware will be created using the `archive`
command from coreboot utils (`src/third_party/coreboot/util/archive`). These
files will end up being stored in the FMAP region COREBOOT in the image.

To show these files in an image $IMAGE, run:

```
cbfstool $IMAGE print -r COREBOOT
```

To extract an archive $NAME from an image as $FILE, run:

```
cbfstool $IMAGE extract -r COREBOOT -n $NAME -f $FILE
```

Also see the
[firmware UI troubleshooting doc](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/firmware_ui.md)
for bitmap-related issues.
