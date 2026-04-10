#!/usr/bin/env python
# Copyright 2013 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate bitmaps for firmware screens."""

import argparse
from collections import Counter
from collections import defaultdict
from collections import namedtuple
from concurrent.futures import ProcessPoolExecutor
import copy
import glob
import json
import os
import re
import shutil
import subprocess
import sys

from PIL import Image
import yaml


SCRIPT_BASE = os.path.dirname(os.path.abspath(__file__))

STRINGS_GRD_FILE = 'firmware_strings.grd'
STRINGS_JSON_FILE_TMPL = '%s.json'
FORMAT_FILE = 'format.yaml'
BOARDS_CONFIG_FILE = 'boards.yaml'

OUTPUT_DIR = os.getenv('OUTPUT', os.path.join(SCRIPT_BASE, 'build'))

ONE_LINE_DIR = 'one_line'
SVG_FILES = '*.svg'
PNG_FILES = '*.png'

# String format YAML key names.
KEY_DEFAULT = '_DEFAULT_'
KEY_GLYPH = '_GLYPH_'
KEY_LOCALES = 'locales'
KEY_GENERIC_FILES = 'generic_files'
KEY_LOCALIZED_FILES = 'localized_files'
KEY_SPRITE_FILES = 'sprite_files'
KEY_STYLES = 'styles'
KEY_BGCOLOR = 'bgcolor'
KEY_FGCOLOR = 'fgcolor'
KEY_HEIGHT = 'height'
KEY_MAX_WIDTH = 'max_width'
KEY_MAX_NUM_LINES = 'max_num_lines'
KEY_FONTS = 'fonts'
KEY_RW_ONLY = 'rw_only'

# Board config YAML key names.
KEY_SDCARD = 'sdcard'
KEY_DPI = 'dpi'
KEY_RTL = 'rtl'
KEY_RW_OVERRIDE = 'rw_override'
KEY_SPLIT_RATIO = 'split_ratio'

BMP_HEADER_OFFSET_NUM_LINES = 6

# Regular expressions used to eliminate spurious spaces and newlines in
# translation strings.
NEWLINE_PATTERN = re.compile(r'([^\n])\n([^\n])')
NEWLINE_REPLACEMENT = r'\1 \2'
CRLF_PATTERN = re.compile(r'\r\n')
MULTIBLANK_PATTERN = re.compile(r'   *')
# Symbols use different font than regular text. While symbols used by us,
# doesn't take more vertical space than regular characters, pango inflates
# height of the result bitmap to match requirements of symbols font. Use pango
# markup to ignore height of the font used for symbols. SYMBOL_PATTERN should
# contain all symbols used in firmware_strings.grd.
SYMBOL_PATTERN = re.compile(
    r'([\N{POWER SYMBOL}\N{CLOCKWISE GAPPED CIRCLE ARROW}])'
)
SYMBOL_REPLACEMENT = r'<span line_height="0.1">\1</span>'

LocaleInfo = namedtuple('LocaleInfo', ['code', 'rtl'])


class BuildImageError(Exception):
    """Exception for all errors generated during build image process."""


def get_config_with_defaults(configs, key):
    """Gets config of `key` from `configs`.

    If `key` is not present in `configs`, the default config will be returned.
    Similarly, if some config values are missing for `key`, the default ones
    will be used.
    """
    config = configs[KEY_DEFAULT].copy()
    params = configs.get(key, {})
    if params:
        config.update(params)
    return config


def load_board_config(filename, board):
    """Loads the configuration of `board` from `filename`.

    Args:
        filename: File name of a YAML config file.
        board: Board name.

    Returns:
        A dictionary mapping each board name to its config.
    """
    with open(filename, 'rb') as file:
        raw = yaml.safe_load(file)

    config = copy.deepcopy(raw[KEY_DEFAULT])
    for boards, params in raw.items():
        if boards == KEY_DEFAULT:
            continue
        if board not in boards.split(','):
            continue
        if params:
            config.update(params)
        break
    else:
        raise BuildImageError('Board config not found for ' + board)

    return config


def check_fonts(fonts):
    """Checks if all fonts are available."""
    for locale, font in fonts.items():
        if subprocess.run(['fc-list', '-q', font], check=False).returncode != 0:
            raise BuildImageError(
                f'Font {font!r} not found for locale {locale!r}'
            )


def run_pango_view(
    input_file,
    output_file,
    locale,
    font,
    height,
    width_pt,
    dpi,
    bgcolor,
    fgcolor,
    hinting='full',
    markup=False,
):
    """Runs pango-view."""
    command = ['pango-view', '-q']
    if locale:
        command += ['--language', locale]

    if markup:
        command += ['--markup']
    # Font size should be proportional to the height. Here we use 2 as the
    # divisor so that setting dpi to 96 (pango-view's default) in boards.yaml
    # will be roughly equivalent to setting the screen resolution to 1366x768.
    font_size = height / 2
    font_spec = f'{font} {font_size!r}'
    command += ['--font', font_spec]

    if width_pt:
        command.append(f'--width={width_pt:d}')
    if dpi:
        command.append(f'--dpi={dpi:d}')
    command.append('--margin=0')
    command += ['--background', bgcolor]
    command += ['--foreground', fgcolor]
    command += ['--hinting', hinting]

    command += ['--output', output_file]
    command.append(input_file)

    subprocess.check_call(command, stdout=subprocess.PIPE)


def parse_locale_json_file(locale, json_dir):
    """Parses given firmware string json file.

    Args:
        locale: The name of the locale, e.g. "da" or "pt-BR".
        json_dir: Directory containing json output from grit.

    Returns:
        A dictionary for mapping of "name to content" for files to be generated.
    """
    result = {}
    filename = os.path.join(json_dir, STRINGS_JSON_FILE_TMPL % locale)
    with open(filename, encoding='utf-8-sig') as input_file:
        for tag, msgdict in json.load(input_file).items():
            msgtext = msgdict['message']
            msgtext = re.sub(CRLF_PATTERN, '\n', msgtext)
            msgtext = re.sub(NEWLINE_PATTERN, NEWLINE_REPLACEMENT, msgtext)
            msgtext = re.sub(MULTIBLANK_PATTERN, ' ', msgtext)
            msgtext = re.sub(SYMBOL_PATTERN, SYMBOL_REPLACEMENT, msgtext)
            # Strip any trailing whitespace.  A trailing newline appears to make
            # Pango report a larger layout size than what's actually visible.
            msgtext = msgtext.strip()
            result[tag] = msgtext
    return result


class Converter:
    """Converter for converting sprites, texts, and glyphs to bitmaps.

    Attributes:
        SCALE_BASE (int): The base for bitmap scales, same as UI_SCALE in
            depthcharge. For example, if SCALE_BASE is 1000, then height = 200
            means 20% of the screen height. Also see the 'styles' section in
            format.yaml.
        SPRITE_ASSUMED_RESOLUTION (int): Screen resolution to decide the size of
            sprite images.
        GLYPH_ASSUMED_RESOLUTION (int): Screen resolution to decide the size of
            glyph images.
        SPRITE_MAX_COLORS (int): Maximum colors to use for converting image
            sprites to bitmaps.
        GLYPH_MAX_COLORS (int): Maximum colors to use for glyph bitmaps.
    """

    SCALE_BASE = 1000

    # Assumed screen resolutions for sprite and glyph images, which should be
    # good enough to render the images clearly on the screen, while not taking
    # too much storage space.  We don't need the screen resolution for text
    # images here, because the image size only depends on the DPI.
    SPRITE_ASSUMED_RESOLUTION = 2160
    GLYPH_ASSUMED_RESOLUTION = 2160

    # Max colors
    SPRITE_MAX_COLORS = 128
    GLYPH_MAX_COLORS = 7

    def __init__(self, board, formats, board_config, output):
        """Inits converter.

        Args:
            board: Board name.
            formats: A dictionary of string formats.
            board_config: A dictionary of board configurations.
            output: Output directory.
        """
        self.board = board
        self.formats = formats
        self.config = board_config
        self.set_dirs(output)
        self.set_rename_map()
        self.set_locales()
        self.text_max_colors = self.get_text_colors(self.config[KEY_DPI])

    def set_dirs(self, output):
        """Sets board output directory and stage directory.

        Args:
            output: Output directory.
        """
        self.strings_dir = os.path.join(SCRIPT_BASE, 'strings')
        self.sprite_dir = os.path.join(SCRIPT_BASE, 'sprite')
        self.locale_dir = os.path.join(self.strings_dir, 'locale')
        self.output_dir = os.path.join(output, self.board)
        self.output_ro_dir = os.path.join(self.output_dir, 'locale', 'ro')
        self.output_rw_dir = os.path.join(self.output_dir, 'locale', 'rw')
        self.stage_dir = os.path.join(output, '.stage')
        self.stage_grit_dir = os.path.join(self.stage_dir, 'grit')
        self.stage_locale_dir = os.path.join(self.stage_dir, 'locale')
        self.stage_glyph_dir = os.path.join(self.stage_dir, 'glyph')
        self.stage_sprite_dir = os.path.join(self.stage_dir, 'sprite')

    def set_rename_map(self):
        """Initializes a dict `self.rename_map` for image renaming.

        For each items in the dict, image `key` will be renamed to `value`.
        """
        is_detachable = os.getenv('DETACHABLE') == '1'
        physical_presence = os.getenv('PHYSICAL_PRESENCE')
        rename_map = {}

        # Navigation instructions
        if is_detachable:
            rename_map.update(
                {
                    'nav-button_power': 'nav-key_enter',
                    'nav-button_volume_up': 'nav-key_up',
                    'nav-button_volume_down': 'nav-key_down',
                    'navigate0_tablet': 'navigate0',
                    'navigate1_tablet': 'navigate1',
                }
            )
        else:
            rename_map.update(
                {
                    'nav-button_power': None,
                    'nav-button_volume_up': None,
                    'nav-button_volume_down': None,
                    'navigate0_tablet': None,
                    'navigate1_tablet': None,
                }
            )

        # Physical presence confirmation
        if physical_presence != 'recovery':
            rename_map['rec_to_dev_desc1_phyrec'] = None
            if physical_presence != 'keyboard':
                raise BuildImageError(
                    f'Invalid physical presence setting {physical_presence} '
                    f'for board {self.board}'
                )

        # Broken screen
        if physical_presence == 'recovery':
            rename_map['broken_desc2_phyrec'] = 'broken_desc2'
            rename_map['broken_desc2_detach'] = None
        elif is_detachable:
            rename_map['broken_desc2_phyrec'] = None
            rename_map['broken_desc2_detach'] = 'broken_desc2'
        else:
            rename_map['broken_desc2_phyrec'] = None
            rename_map['broken_desc2_detach'] = None

        # Error when untrusted key is used to unlock bootloader
        if is_detachable:
            rename_map['error_untrusted_confirm_detach'] = (
                'error_untrusted_confirm'
            )
        else:
            rename_map['error_untrusted_confirm_detach'] = None

        # SD card
        if not self.config[KEY_SDCARD]:
            rename_map.update(
                {
                    'btn_rec_by_disk_no_sd': 'btn_rec_by_disk',
                    'rec_disk_step1_desc1_no_sd': 'rec_disk_step1_desc1',
                    'error_internet_recovery_no_sd': 'error_internet_recovery',
                }
            )
        else:
            rename_map.update(
                {
                    'btn_rec_by_disk_no_sd': None,
                    'rec_disk_step1_desc1_no_sd': None,
                    'error_internet_recovery_no_sd': None,
                }
            )

        # Check for duplicate new names
        new_names = list(
            new_name for new_name in rename_map.values() if new_name
        )
        if len(set(new_names)) != len(new_names):
            raise BuildImageError('Duplicate values found in rename_map')

        # Map new_name to None to skip image generation for it
        for new_name in new_names:
            if new_name not in rename_map:
                rename_map[new_name] = None

        # Print mapping
        print('Rename map:')
        for name, new_name in sorted(rename_map.items()):
            print(f'  {name} => {new_name}')

        self.rename_map = rename_map

    def set_locales(self):
        """Sets a list of locales for which localized images are converted."""
        # LOCALES environment variable can override boards.yaml
        env_locales = os.getenv('LOCALES')
        rtl_locales = set(self.config[KEY_RTL])
        if env_locales:
            locales = env_locales.split()
        else:
            locales = self.config[KEY_LOCALES]
            # Check rtl_locales are contained in locales.
            unknown_rtl_locales = rtl_locales - set(locales)
            if unknown_rtl_locales:
                raise BuildImageError(
                    f'Unknown locales {list(unknown_rtl_locales)} in {KEY_RTL}'
                )
        self.locales = [
            LocaleInfo(code, code in rtl_locales) for code in locales
        ]

    @classmethod
    def get_text_colors(cls, dpi):
        """Derives maximum text colors from `dpi`."""
        if dpi < 64:
            return 2
        if dpi < 72:
            return 3
        if dpi < 80:
            return 4
        if dpi < 96:
            return 5
        if dpi < 112:
            return 6
        return 7

    @classmethod
    def _to_px(cls, length, screen_resolution, num_lines=1):
        """Converts the relative coordinate to absolute one in pixels."""
        return int(length * screen_resolution / cls.SCALE_BASE) * num_lines

    @classmethod
    def _get_png_height(cls, png_file):
        # With small DPI, pango-view may generate an empty file
        if os.path.getsize(png_file) == 0:
            return 0
        with Image.open(png_file) as image:
            return image.size[1]

    def get_num_lines(self, file, one_line_dir):
        """Gets the number of lines of text in `file`."""
        name, _ = os.path.splitext(os.path.basename(file))
        png_name = name + '.png'
        multi_line_file = os.path.join(os.path.dirname(file), png_name)
        one_line_file = os.path.join(one_line_dir, png_name)
        # The number of lines is determined by comparing the height of
        # `multi_line_file` with `one_line_file`, where the latter is generated
        # without the '--width' option passed to pango-view.
        height = self._get_png_height(multi_line_file)
        line_height = self._get_png_height(one_line_file)
        return int(round(height / line_height))

    def convert_svg_to_png(
        self, svg_file, png_file, height, resolution, bgcolor, num_lines=1
    ):
        """Converts SVG to PNG file."""
        # If the width/height of the SVG file is specified in points, the
        # rsvg-convert command with default 90DPI will potentially cause the
        # pixels at the right/bottom border of the output image to be
        # transparent (or filled with the specified background color).  This
        # seems like an rsvg-convert issue regarding image scaling.  Therefore,
        # use 72DPI here to avoid the scaling.
        command = [
            'rsvg-convert',
            '--background-color',
            f"'{bgcolor}'",
            '--dpi-x',
            '72',
            '--dpi-y',
            '72',
            '-o',
            png_file,
        ]
        height_px = self._to_px(height, resolution, num_lines)
        if height_px <= 0:
            raise BuildImageError(
                f'Height of {os.path.basename(svg_file)!r} '
                f'<= 0 ({height_px:d}px)'
            )
        command.extend(['--height', f'{height_px:d}'])
        command.append(svg_file)
        subprocess.check_call(' '.join(command), shell=True)

    def convert_png_to_bmp(self, png_file, bmp_file, max_colors, num_lines=1):
        """Converts PNG to BMP file."""
        image = Image.open(png_file)

        # Process alpha channel and transparency.
        if image.mode == 'RGBA':
            raise BuildImageError('PNG with RGBA mode is not supported')
        if image.mode == 'P' and 'transparency' in image.info:
            raise BuildImageError('PNG with RGBA palette is not supported')
        if image.mode != 'RGB':
            image = image.convert('RGB')

        # Export and downsample color space.
        image.convert(
            'P', dither=None, colors=max_colors, palette=Image.ADAPTIVE
        ).save(bmp_file)

        with open(bmp_file, 'rb+') as f:
            f.seek(BMP_HEADER_OFFSET_NUM_LINES)
            f.write(bytearray([num_lines]))

    @classmethod
    def _bisect_width(cls, initial_width_pt, max_width, get_width):
        """Bisects to find the width that produces image width `max_width`.

        Args:
            initial_width_pt: Initial width_pt to try with in binary search.
            max_width: Maximum (target) relative width to search for.
            get_width: A function converting width_pt to relative width. The
                function is called once before returning.

        Returns:
            The best integer width_pt.
        """
        min_width_pt = 1
        width_pt = initial_width_pt
        width = get_width(width_pt)
        while width < max_width:
            min_width_pt = width_pt
            width_pt *= 2
            width = get_width(width_pt)
        if width == max_width:
            return width_pt

        max_width_pt = width_pt
        # Find maximum width_pt with get_width(width_pt) <= max_width
        while min_width_pt < max_width_pt:
            width_pt = (min_width_pt + max_width_pt + 1) // 2
            width = get_width(width_pt)
            if width > max_width:
                max_width_pt = width_pt - 1
            else:
                min_width_pt = width_pt
        get_width(max_width_pt)
        return max_width_pt

    def convert_text_to_image(
        self,
        locale,
        input_file,
        output_file,
        font,
        stage_dir,
        max_colors,
        height=None,
        max_width=None,
        initial_width_pt=None,
        dpi=None,
        screen_resolution=None,
        bgcolor='#000000',
        fgcolor='#ffffff',
        use_svg=False,
        max_num_lines=None,
        pango_markup=False,
    ):
        """Converts text file `input_file` into image file.

        Because pango-view does not support assigning output format options for
        bitmap, we must create images in SVG/PNG format and then post-process
        them (e.g. convert into BMP by ImageMagick).

        Args:
            locale: Locale (language) to select implicit rendering options. None
                for locale-independent strings.
            input_file: Path of input text file.
            output_file: Path of output image file.
            font: Font name.
            stage_dir: Directory to store intermediate file(s).
            max_colors: Maximum colors to convert to bitmap.
            height: Image height relative to the screen resolution.
            max_width: Maximum image width relative to the screen resolution.
            initial_width_pt: Initial width_pt to try with in binary search.
            dpi: DPI value passed to pango-view.
            screen_resolution: Screen resolution for converting SVG to PNG.
            bgcolor: Background color (#rrggbb).
            fgcolor: Foreground color (#rrggbb).
            use_svg: If set to True, generate SVG file. Otherwise, generate PNG
                file.
            max_num_lines: Maximum number of text lines that image can have.

        Returns:
            The width in points passed to pango-view, or `None` when not
            applicable.
        """
        one_line_dir = os.path.join(stage_dir, ONE_LINE_DIR)
        os.makedirs(one_line_dir, exist_ok=True)

        name, _ = os.path.splitext(os.path.basename(input_file))
        svg_file = os.path.join(stage_dir, name + '.svg')
        png_file = os.path.join(stage_dir, name + '.png')
        png_file_one_line = os.path.join(one_line_dir, name + '.png')

        if use_svg:
            run_pango_view(
                input_file,
                svg_file,
                locale,
                font,
                height,
                0,
                dpi,
                bgcolor,
                fgcolor,
                hinting='none',
                markup=pango_markup,
            )
            self.convert_svg_to_png(
                svg_file, png_file, height, screen_resolution, bgcolor
            )
            self.convert_png_to_bmp(png_file, output_file, max_colors)
            return None, None

        if not dpi:
            raise BuildImageError('DPI must be specified with use_svg=False')

        run_pango_view(
            input_file,
            png_file_one_line,
            locale,
            font,
            height,
            0,
            dpi,
            bgcolor,
            fgcolor,
            markup=pango_markup,
        )

        def get_width(width_pt):
            """Gets the worst-case relative width."""
            run_pango_view(
                input_file,
                png_file,
                locale,
                font,
                height,
                width_pt,
                dpi,
                bgcolor,
                fgcolor,
                markup=pango_markup,
            )
            num_lines = self.get_num_lines(png_file, one_line_dir)
            with Image.open(png_file) as image:
                png_width, png_height = image.size
            # To ensure the rendered image doesn't exceed the maximum width
            # in runtime, we need to calculate the worst-case width, considering
            # the rounding errors for integer division. In runtime, the rendered
            # width and the maximum width (both in pixels) are calculated with:
            #
            #  height_px = floor(height * num_lines * R / SCALE_BASE)
            #  width_px = floor(height_px * W / H)
            #  max_width_px = floor(max_width * R / SCALE_BASE)
            #
            # where `height` and `max_width` are relative lengths (as in the
            # code), R is the screen resolution in pixels, and W and H are width
            # and height of the image in pixels.
            #
            # If the following holds
            #
            #  height * num_lines * W / H <= max_width
            #
            # we can prove that `width_px <= max_width_px`. Therefore we use the
            # formula to calculate the worst-case width.
            return height * num_lines * png_width / png_height

        if max_width:
            # NOTE: With the same DPI, the height of multi-line PNG is not
            # necessarily a multiple of the height of one-line PNG. Therefore,
            # even with the binary search, the height of the resulting
            # multi-line PNG might be less than "one_line_height * num_lines".
            if not initial_width_pt:
                # max_width is not in points, but this should be good enough
                # as an initial value.
                initial_width_pt = max_width
            width_pt = self._bisect_width(
                initial_width_pt, max_width, get_width
            )
            num_lines = self.get_num_lines(png_file, one_line_dir)
        else:
            width_pt = None
            png_file = png_file_one_line
            num_lines = 1
        if max_num_lines and num_lines > max_num_lines:
            raise BuildImageError(
                f'Sprite image {input_file!r} uses more than {max_num_lines}'
                f' lines ({num_lines} > {max_num_lines})'
            )

        self.convert_png_to_bmp(
            png_file, output_file, max_colors, num_lines=num_lines
        )
        return width_pt

    def convert_sprite_images(self):
        """Converts sprite images."""
        names = self.formats[KEY_SPRITE_FILES]
        styles = self.formats[KEY_STYLES]
        # Check redundant images
        for filename in glob.glob(os.path.join(self.sprite_dir, SVG_FILES)):
            name, _ = os.path.splitext(os.path.basename(filename))
            if name not in names:
                raise BuildImageError(
                    f'Sprite image {filename!r} not specified in {FORMAT_FILE}'
                )
        # Convert images
        os.makedirs(self.stage_sprite_dir, exist_ok=True)
        for name, category in names.items():
            new_name = self.rename_map.get(name, name)
            if not new_name:
                continue
            style = get_config_with_defaults(styles, category)
            svg_file = os.path.join(self.sprite_dir, name + '.svg')
            png_file = os.path.join(self.stage_sprite_dir, name + '.png')
            bmp_file = os.path.join(self.output_dir, new_name + '.bmp')
            height = style[KEY_HEIGHT]
            bgcolor = style[KEY_BGCOLOR]
            self.convert_svg_to_png(
                svg_file,
                png_file,
                height,
                self.SPRITE_ASSUMED_RESOLUTION,
                bgcolor,
            )
            self.convert_png_to_bmp(png_file, bmp_file, self.SPRITE_MAX_COLORS)

    def build_generic_strings(self):
        """Builds images of generic (locale-independent) strings."""
        dpi = self.config[KEY_DPI]

        names = self.formats[KEY_GENERIC_FILES]
        styles = self.formats[KEY_STYLES]
        fonts = self.formats[KEY_FONTS]
        default_font = fonts[KEY_DEFAULT]

        for txt_file in glob.glob(os.path.join(self.strings_dir, '*.txt')):
            name, _ = os.path.splitext(os.path.basename(txt_file))
            new_name = self.rename_map.get(name, name)
            if not new_name:
                continue
            bmp_file = os.path.join(self.output_dir, new_name + '.bmp')
            category = names[name]
            style = get_config_with_defaults(styles, category)
            if style[KEY_MAX_WIDTH]:
                # Setting max_width causes left/right alignment of the text.
                # However, generic strings are locale independent, and hence
                # shouldn't have text alignment within the bitmap.
                raise BuildImageError(
                    f'{name}: {KEY_MAX_WIDTH!r} should be '
                    'null for generic strings'
                )
            self.convert_text_to_image(
                None,
                txt_file,
                bmp_file,
                default_font,
                self.stage_dir,
                self.text_max_colors,
                height=style[KEY_HEIGHT],
                max_width=None,
                initial_width_pt=None,
                dpi=dpi,
                bgcolor=style[KEY_BGCOLOR],
                fgcolor=style[KEY_FGCOLOR],
            )

    def build_locale(self, locale, names, generic):
        """Builds images of strings for `locale`."""
        dpi = self.config[KEY_DPI]
        styles = self.formats[KEY_STYLES]
        fonts = self.formats[KEY_FONTS]
        font = fonts.get(locale, fonts[KEY_DEFAULT])
        inputs = parse_locale_json_file(locale, self.stage_grit_dir)

        # Walk locale dir to add pre-generated texts such as language names.
        for txt_file in glob.glob(
            os.path.join(self.locale_dir, locale, '*.txt')
        ):
            name, _ = os.path.splitext(os.path.basename(txt_file))
            with open(txt_file, 'r', encoding='utf-8-sig') as f:
                inputs[name] = f.read().strip()

        # Add generic strings to allow to use them as placeholders
        inputs.update(generic)

        # Replace placeholders (like '{msg[name]}') with actual values
        for name, msg in inputs.items():
            inputs[name] = msg.format(msg=inputs)

        stage_dir = os.path.join(self.stage_locale_dir, locale)
        os.makedirs(stage_dir, exist_ok=True)
        output_dir = os.path.join(self.output_ro_dir, locale)
        os.makedirs(output_dir, exist_ok=True)

        width_pt_counters = defaultdict(Counter)
        width_pt_counter = None
        for name, category in sorted(names.items()):
            if name not in inputs:
                raise BuildImageError(
                    f'Locale {locale!r}: ' f'missing translation: {name!r}'
                )

            new_name = self.rename_map.get(name, name)
            if not new_name:
                continue
            output_file = os.path.join(output_dir, new_name + '.bmp')

            # Write to text file
            text_file = os.path.join(stage_dir, name + '.txt')
            with open(text_file, 'w', encoding='utf-8-sig') as f:
                f.write(inputs[name] + '\n')

            # Convert text to image
            style = get_config_with_defaults(styles, category)
            height = style[KEY_HEIGHT]
            max_width = style[KEY_MAX_WIDTH]
            max_num_lines = style[KEY_MAX_NUM_LINES]
            width_pt_counter = (
                width_pt_counters[(height, max_width)] if max_width else None
            )
            if width_pt_counter:
                # Similarly, find the most frequently used `width_pt`. In case
                # of a tie, pick the largest width.
                best_width_pt = max(
                    width_pt_counter, key=lambda w: (width_pt_counter[w], w)
                )
            else:
                best_width_pt = None
            width_pt = self.convert_text_to_image(
                locale,
                text_file,
                output_file,
                font,
                stage_dir,
                self.text_max_colors,
                height=height,
                max_width=max_width,
                initial_width_pt=best_width_pt,
                dpi=dpi,
                bgcolor=style[KEY_BGCOLOR],
                fgcolor=style[KEY_FGCOLOR],
                max_num_lines=max_num_lines,
                pango_markup=True,
            )
            if width_pt:
                width_pt_counter[width_pt] += 1

    def build_localized_strings(self):
        """Builds images of localized strings."""
        # Sources are one .grd file with identifiers chosen by engineers and
        # corresponding English texts, as well as a set of .xtb files (one for
        # each language other than US English) with a mapping from hash to
        # translation. Because the keys in the .xtb files are a hash of the
        # English source text, rather than our identifiers, such as
        # "btn_cancel", we use the "grit" command line tool to process the .grd
        # and .xtb files, producing a set of .json files mapping our identifier
        # to the translated string, one for every language including US English.

        # This invokes the grit build command to generate JSON files from the
        # XTB files containing translations.  The results are placed in
        # `self.stage_grit_dir` as specified in firmware_strings.grd, i.e. one
        # JSON file per locale.
        os.makedirs(self.stage_grit_dir, exist_ok=True)
        subprocess.check_call(
            [
                'grit',
                '-i',
                os.path.join(self.locale_dir, STRINGS_GRD_FILE),
                'build',
                '-o',
                self.stage_grit_dir,
            ]
        )

        names = self.formats[KEY_LOCALIZED_FILES]

        # Walk strings dir to add pre-generated generic texts
        generic = {}
        for txt_file in glob.glob(os.path.join(self.strings_dir, '*.txt')):
            name, _ = os.path.splitext(os.path.basename(txt_file))
            with open(txt_file, 'r', encoding='utf-8-sig') as f:
                generic[name] = f.read().strip()

        with ProcessPoolExecutor() as executor:
            futures = []
            for locale_info in self.locales:
                locale = locale_info.code
                print(locale, end=' ', flush=True)
                futures.append(
                    executor.submit(self.build_locale, locale, names, generic)
                )

            print()

            try:
                for future in futures:
                    future.result()
            except KeyboardInterrupt:
                executor.shutdown(wait=False)
                sys.exit('Aborted by user')

    def move_language_images(self):
        """Renames language bitmaps and move to self.output_dir.

        The directory self.output_dir contains locale-independent images, and is
        used for creating vbgfx.bin by archive_images.py.
        """
        for locale_info in self.locales:
            locale = locale_info.code
            ro_locale_dir = os.path.join(self.output_ro_dir, locale)
            old_file = os.path.join(ro_locale_dir, 'language.bmp')
            new_file = os.path.join(self.output_dir, f'language_{locale}.bmp')
            if os.path.exists(new_file):
                raise BuildImageError(f'File already exists: {new_file}')
            shutil.move(old_file, new_file)

    def build_glyphs(self):
        """Builds glyphs of ascii characters."""
        os.makedirs(self.stage_glyph_dir, exist_ok=True)
        output_dir = os.path.join(self.output_dir, 'glyph')
        os.makedirs(output_dir)
        styles = self.formats[KEY_STYLES]
        style = get_config_with_defaults(styles, KEY_GLYPH)
        height = style[KEY_HEIGHT]
        font = self.formats[KEY_FONTS][KEY_GLYPH]
        with ProcessPoolExecutor() as executor:
            futures = []
            for c in range(ord(' '), ord('~') + 1):
                name = f'idx{c:03d}_{c:02x}'
                txt_file = os.path.join(self.stage_glyph_dir, name + '.txt')
                with open(txt_file, 'w', encoding='ascii') as f:
                    f.write(chr(c))
                    f.write('\n')
                output_file = os.path.join(output_dir, name + '.bmp')
                futures.append(
                    executor.submit(
                        self.convert_text_to_image,
                        None,
                        txt_file,
                        output_file,
                        font,
                        self.stage_glyph_dir,
                        self.GLYPH_MAX_COLORS,
                        height=height,
                        screen_resolution=self.GLYPH_ASSUMED_RESOLUTION,
                        use_svg=True,
                    )
                )
            for future in futures:
                future.result()

    def copy_images_to_rw(self):
        """Copies localized images specified in boards.yaml for RW override."""
        split_ratio = self.config[KEY_SPLIT_RATIO]
        if not self.config[KEY_RW_OVERRIDE] and split_ratio == 0:
            print('  No localized images are specified for RW, skipping')
            return

        # Check if the split ratio between RO and RW is supported.
        if split_ratio not in (0, 100):
            raise BuildImageError(
                f'Unsupported split_ratio value {split_ratio}!'
                ' Choose either 0 (no split) or 100 (move RW_ONLY assets)'
            )

        for locale_info in self.locales:
            locale = locale_info.code
            ro_locale_dir = os.path.join(self.output_ro_dir, locale)
            rw_locale_dir = os.path.join(self.output_rw_dir, locale)
            os.makedirs(rw_locale_dir)

            # Overlapping assets in RW_OVERRIDE & RW_ONLY is not expected.
            # Hence move any RW_ONLY asset before copying RW_OVERRIDE assets.
            # This will help to catch any overlapping scenario during build.
            rw_only_names = self.formats[KEY_RW_ONLY] if split_ratio > 0 else []
            for name in rw_only_names:
                ro_src = os.path.join(ro_locale_dir, name + '.bmp')
                rw_dst = os.path.join(rw_locale_dir, name + '.bmp')
                shutil.move(ro_src, rw_dst)

            for name in self.config[KEY_RW_OVERRIDE]:
                ro_src = os.path.join(ro_locale_dir, name + '.bmp')
                rw_dst = os.path.join(rw_locale_dir, name + '.bmp')
                shutil.copyfile(ro_src, rw_dst)

    def create_locale_list(self):
        """Creates locale list as a CSV file.

        Each line in the file is of format "code,rtl", where
        - "code": language code of the locale
        - "rtl": "1" for right-to-left language, "0" otherwise
        """
        with open(
            os.path.join(self.output_dir, 'locales'), 'w', encoding='utf-8'
        ) as f:
            for locale_info in self.locales:
                f.write(f'{locale_info.code},{locale_info.rtl:d}\n')

    def build(self):
        """Builds all images required by a board."""
        # Clean up output/stage directories
        for path in (self.output_dir, self.stage_dir):
            if os.path.exists(path):
                shutil.rmtree(path)
        os.makedirs(self.output_dir)
        os.makedirs(self.stage_dir)

        print('Converting sprite images...')
        self.convert_sprite_images()

        print('Building generic strings...')
        self.build_generic_strings()

        print('Building localized strings...')
        self.build_localized_strings()

        print('Moving language images to locale-independent directory...')
        self.move_language_images()

        print('Creating locale list file...')
        self.create_locale_list()

        print('Building glyphs...')
        self.build_glyphs()

        print('Copying specified images to RW packing directory...')
        self.copy_images_to_rw()


def main():
    """Builds bitmaps for firmware screens."""
    parser = argparse.ArgumentParser()
    parser.add_argument('board', help='Target board')
    args = parser.parse_args()
    board = args.board

    with open(FORMAT_FILE, encoding='utf-8') as f:
        formats = yaml.safe_load(f)
    board_config = load_board_config(BOARDS_CONFIG_FILE, board)

    print('Building for ' + board)
    check_fonts(formats[KEY_FONTS])
    print('Output dir: ' + OUTPUT_DIR)
    converter = Converter(board, formats, board_config, OUTPUT_DIR)
    converter.build()


if __name__ == '__main__':
    main()
