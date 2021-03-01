#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to generate bitmaps for firmware screens."""

import argparse
from collections import defaultdict, namedtuple
import copy
import fractions
import glob
import json
import multiprocessing
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
from xml.etree import ElementTree

import yaml
from PIL import Image

SCRIPT_BASE = os.path.dirname(os.path.abspath(__file__))

STRINGS_GRD_FILE = 'firmware_strings.grd'
STRINGS_JSON_FILE_TMPL = '{}.json'
FORMAT_FILE = 'format.yaml'
BOARDS_CONFIG_FILE = 'boards.yaml'

OUTPUT_DIR = os.getenv('OUTPUT', os.path.join(SCRIPT_BASE, 'build'))

ONE_LINE_DIR = 'one_line'
SVG_FILES = '*.svg'
PNG_FILES = '*.png'

DIAGNOSTIC_UI = os.getenv('DIAGNOSTIC_UI') == '1'

# String format YAML key names.
KEY_DEFAULT = '_DEFAULT_'
KEY_LOCALES = 'locales'
KEY_GENERIC_FILES = 'generic_files'
KEY_LOCALIZED_FILES = 'localized_files'
KEY_DIAGNOSTIC_FILES = 'diagnostic_files'
KEY_SPRITE_FILES = 'sprite_files'
KEY_STYLES = 'styles'
KEY_BGCOLOR = 'bgcolor'
KEY_FGCOLOR = 'fgcolor'
KEY_HEIGHT = 'height'
KEY_MAX_WIDTH = 'max_width'
KEY_FONTS = 'fonts'

# Board config YAML key names.
SCREEN_KEY = 'screen'
PANEL_KEY = 'panel'
SDCARD_KEY = 'sdcard'
BAD_USB3_KEY = 'bad_usb3'
DPI_KEY = 'dpi'
LOCALES_KEY = 'locales'
RTL_KEY = 'rtl'
RW_OVERRIDE_KEY = 'rw_override'

BMP_HEADER_OFFSET_NUM_LINES = 6

# Regular expressions used to eliminate spurious spaces and newlines in
# translation strings.
NEWLINE_PATTERN = re.compile(r'([^\n])\n([^\n])')
NEWLINE_REPLACEMENT = r'\1 \2'
CRLF_PATTERN = re.compile(r'\r\n')
MULTIBLANK_PATTERN = re.compile(r'   *')

# The base for bitmap scales, same as UI_SCALE in depthcharge. For example, if
# `SCALE_BASE` is 1000, then height = 200 means 20% of the screen height. Also
# see the 'styles' section in format.yaml.
SCALE_BASE = 1000
DEFAULT_GLYPH_HEIGHT = 20

GLYPH_FONT = 'Cousine'

LocaleInfo = namedtuple('LocaleInfo', ['code', 'rtl'])


class DataError(Exception):
  pass


class BuildImageError(Exception):
  """The exception class for all errors generated during build image process."""


def get_config_with_defaults(configs, key):
  """Gets config of `key` from `configs`.

  If `key` is not present in `configs`, the default config will be returned.
  Similarly, if some config values are missing for `key`, the default ones will
  be used.
  """
  config = configs[KEY_DEFAULT].copy()
  config.update(configs.get(key, {}))
  return config


def load_boards_config(filename):
  """Loads the configuration of all boards from `filename`.

  Args:
    filename: File name of a YAML config file.

  Returns:
    A dictionary mapping each board name to its config.
  """
  with open(filename, 'rb') as file:
    raw = yaml.load(file)

  configs = {}
  default = raw[KEY_DEFAULT]
  if not default:
    raise BuildImageError('Default configuration is not found')
  for boards, params in raw.items():
    if boards == KEY_DEFAULT:
      continue
    config = copy.deepcopy(default)
    if params:
      config.update(params)
    for board in boards.replace(',', ' ').split():
      configs[board] = config

  return configs


def check_fonts(fonts):
    """Check if all fonts are available."""
    for locale, font in fonts.items():
      if subprocess.run(['fc-list', '-q', font]).returncode != 0:
        raise BuildImageError('Font %r not found for locale %r'
                              % (font, locale))


def run_pango_view(input_file, output_file, locale, font, height, max_width,
                   dpi, bgcolor, fgcolor, hinting='full'):
  """Run pango-view."""
  command = ['pango-view', '-q']
  if locale:
    command += ['--language', locale]

  # Font size should be proportional to the height. Here we use 2 as the
  # divisor so that setting dpi to 96 (pango-view's default) in boards.yaml
  # will be roughly equivalent to setting the screen resolution to 1366x768.
  font_size = height / 2
  font_spec = '%s %r' % (font, font_size)
  command += ['--font', font_spec]

  if max_width:
    # When converting text to PNG by pango-view, the ratio of image height to
    # the font size is usually no more than 1.1875 (with Roboto). Therefore,
    # set the `max_width_pt` as follows to prevent UI drawing from exceeding
    # the canvas boundary in depthcharge runtime. The divisor 2 is the same in
    # the calculation of `font_size` above.
    max_width_pt = int(max_width / 2 * 1.1875)
    command.append('--width=%d' % max_width_pt)
  if dpi:
    command.append('--dpi=%d' % dpi)
  command.append('--margin=0')
  command += ['--background', bgcolor]
  command += ['--foreground', fgcolor]
  command += ['--hinting',  hinting]

  command += ['--output', output_file]
  command.append(input_file)

  subprocess.check_call(command, stdout=subprocess.PIPE)


def convert_text_to_image(locale, input_file, font, output_dir, height=None,
                          max_width=None, dpi=None, bgcolor='#000000',
                          fgcolor='#ffffff', use_svg=False):
  """Converts text file `input_file` into image file(s).

  Because pango-view does not support assigning output format options for
  bitmap, we must create images in SVG/PNG format and then post-process them
  (e.g. convert into BMP by ImageMagick).

  Args:
    locale: Locale (language) to select implicit rendering options. None for
      locale-independent strings.
    input_file: Path of input text file.
    font: Font name.
    height: Image height relative to the screen resolution.
    max_width: Maximum image width relative to the screen resolution.
    output_dir: Directory to generate image files.
    bgcolor: Background color (#rrggbb).
    fgcolor: Foreground color (#rrggbb).
    use_svg: If set to True, generate SVG file. Otherwise, generate PNG file.
  """
  os.makedirs(os.path.join(output_dir, ONE_LINE_DIR), exist_ok=True)
  name, _ = os.path.splitext(os.path.basename(input_file))
  svg_file = os.path.join(output_dir, name + '.svg')
  png_file = os.path.join(output_dir, name + '.png')
  png_file_one_line = os.path.join(output_dir, ONE_LINE_DIR, name + '.png')

  if use_svg:
    run_pango_view(input_file, svg_file, locale, font, height, 0, dpi,
                   bgcolor, fgcolor, hinting='none')
  else:
    run_pango_view(input_file, png_file, locale, font, height, max_width, dpi,
                   bgcolor, fgcolor)
    if locale:
      run_pango_view(input_file, png_file_one_line, locale, font, height, 0,
                     dpi, bgcolor, fgcolor)


def parse_locale_json_file(locale, json_dir):
  """Parses given firmware string json file.

  Args:
    locale: The name of the locale, e.g. "da" or "pt-BR".
    json_dir: Directory containing json output from grit.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  result = {}
  filename = os.path.join(json_dir, STRINGS_JSON_FILE_TMPL.format(locale))
  with open(filename, encoding='utf-8-sig') as input_file:
    for tag, msgdict in json.load(input_file).items():
      msgtext = msgdict['message']
      msgtext = re.sub(CRLF_PATTERN, '\n', msgtext)
      msgtext = re.sub(NEWLINE_PATTERN, NEWLINE_REPLACEMENT, msgtext)
      msgtext = re.sub(MULTIBLANK_PATTERN, ' ', msgtext)
      # Strip any trailing whitespace.  A trailing newline appears to make
      # Pango report a larger layout size than what's actually visible.
      msgtext = msgtext.strip()
      result[tag] = msgtext
  return result


class Converter(object):
  """Converter from assets, texts, URLs, and fonts to bitmap images.

  Attributes:
    ASSET_DIR (str): Directory of image assets.
    DEFAULT_OUTPUT_EXT (str): Default output file extension.
    ASSET_MAX_COLORS (int): Maximum colors to use for converting image assets
      to bitmaps.
    DEFAULT_BACKGROUND (tuple): Default background color.
    BACKGROUND_COLORS (dict): Background color of each image. Key is the image
      name and value is a tuple of RGB values.
  """

  ASSET_DIR = 'assets'
  DEFAULT_OUTPUT_EXT = '.bmp'

  # background colors
  DEFAULT_BACKGROUND = (0x20, 0x21, 0x24)
  LANG_HEADER_BACKGROUND = (0x16, 0x17, 0x19)
  LINK_SELECTED_BACKGROUND = (0x2a, 0x2f, 0x39)
  ASSET_MAX_COLORS = 128

  BACKGROUND_COLORS = {
      'ic_dropdown': LANG_HEADER_BACKGROUND,
      'ic_dropleft_focus': LINK_SELECTED_BACKGROUND,
      'ic_dropright_focus': LINK_SELECTED_BACKGROUND,
      'ic_globe': LANG_HEADER_BACKGROUND,
      'ic_search_focus': LINK_SELECTED_BACKGROUND,
      'ic_settings_focus': LINK_SELECTED_BACKGROUND,
      'ic_power_focus': LINK_SELECTED_BACKGROUND,
  }

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
    self.set_screen()
    self.set_rename_map()
    self.set_locales()
    self.text_max_colors = self.get_text_colors(self.config[DPI_KEY])
    self.dpi_warning_printed = False

  def set_dirs(self, output):
    """Sets board output directory and stage directory.

    Args:
      output: Output directory.
    """
    self.strings_dir = os.path.join(SCRIPT_BASE, 'strings')
    self.locale_dir = os.path.join(self.strings_dir, 'locale')
    self.output_dir = os.path.join(output, self.board)
    self.output_ro_dir = os.path.join(self.output_dir, 'locale', 'ro')
    self.output_rw_dir = os.path.join(self.output_dir, 'locale', 'rw')
    self.stage_dir = os.path.join(output, '.stage')
    self.stage_locale_dir = os.path.join(self.stage_dir, 'locale')
    self.stage_font_dir = os.path.join(self.stage_dir, 'font')
    self.temp_dir = os.path.join(self.stage_dir, 'tmp')

  def set_screen(self):
    """Sets screen width and height."""
    self.screen_width, self.screen_height = self.config[SCREEN_KEY]

    self.panel_stretch = fractions.Fraction(1)
    if self.config[PANEL_KEY]:
      # Calculate `panel_stretch`. It's used to shrink images horizontally so
      # that the resulting images will look proportional to the original image
      # on the stretched display. If the display is not stretched, meaning the
      # aspect ratio is same as the screen where images were rendered, no
      # shrinking is performed.
      panel_width, panel_height = self.config[PANEL_KEY]
      self.panel_stretch = fractions.Fraction(self.screen_width * panel_height,
                                              self.screen_height * panel_width)

    if self.panel_stretch > 1:
      raise BuildImageError('Panel aspect ratio (%f) is smaller than screen '
                            'aspect ratio (%f). It indicates screen will be '
                            'shrunk horizontally. It is currently unsupported.'
                            % (panel_width / panel_height,
                               self.screen_width / self.screen_height))

    # Set up square drawing area
    self.canvas_px = min(self.screen_width, self.screen_height)

  def set_rename_map(self):
    """Initializes a dict `self.rename_map` for image renaming.

    For each items in the dict, image `key` will be renamed to `value`.
    """
    is_detachable = os.getenv('DETACHABLE') == '1'
    physical_presence = os.getenv('PHYSICAL_PRESENCE')
    rename_map = {}

    # Navigation instructions
    if is_detachable:
      rename_map.update({
          'nav-button_power': 'nav-key_enter',
          'nav-button_volume_up': 'nav-key_up',
          'nav-button_volume_down': 'nav-key_down',
          'navigate0_tablet': 'navigate0',
          'navigate1_tablet': 'navigate1',
      })
    else:
      rename_map.update({
          'nav-button_power': None,
          'nav-button_volume_up': None,
          'nav-button_volume_down': None,
          'navigate0_tablet': None,
          'navigate1_tablet': None,
      })

    # Physical presence confirmation
    if physical_presence == 'recovery':
      rename_map['rec_to_dev_desc1_phyrec'] = 'rec_to_dev_desc1'
      rename_map['rec_to_dev_desc1_power'] = None
    elif physical_presence == 'power':
      rename_map['rec_to_dev_desc1_phyrec'] = None
      rename_map['rec_to_dev_desc1_power'] = 'rec_to_dev_desc1'
    else:
      rename_map['rec_to_dev_desc1_phyrec'] = None
      rename_map['rec_to_dev_desc1_power'] = None
      if physical_presence != 'keyboard':
        raise BuildImageError('Invalid physical presence setting %s for board '
                              '%s' % (physical_presence, self.board))

    # Broken screen
    if physical_presence == 'recovery':
      rename_map['broken_desc_phyrec'] = 'broken_desc'
      rename_map['broken_desc_detach'] = None
    elif is_detachable:
      rename_map['broken_desc_phyrec'] = None
      rename_map['broken_desc_detach'] = 'broken_desc'
    else:
      rename_map['broken_desc_phyrec'] = None
      rename_map['broken_desc_detach'] = None

    # SD card
    if not self.config[SDCARD_KEY]:
      rename_map.update({
          'rec_sel_desc1_no_sd': 'rec_sel_desc1',
          'rec_sel_desc1_no_phone_no_sd': 'rec_sel_desc1_no_phone',
          'rec_disk_step1_desc0_no_sd': 'rec_disk_step1_desc0',
      })
    else:
      rename_map.update({
          'rec_sel_desc1_no_sd': None,
          'rec_sel_desc1_no_phone_no_sd': None,
          'rec_disk_step1_desc0_no_sd': None,
      })

    # Check for duplicate new names
    new_names = list(new_name for new_name in rename_map.values() if new_name)
    if len(set(new_names)) != len(new_names):
      raise BuildImageError('Duplicate values found in rename_map')

    # Map new_name to None to skip image generation for it
    for new_name in new_names:
      if new_name not in rename_map:
        rename_map[new_name] = None

    # Print mapping
    print('Rename map:')
    for name, new_name in sorted(rename_map.items()):
      print('  %s => %s' % (name, new_name))

    self.rename_map = rename_map

  def set_locales(self):
    """Sets a list of locales for which localized images are converted."""
    # LOCALES environment variable can overwrite boards.yaml
    env_locales = os.getenv('LOCALES')
    rtl_locales = set(self.config[RTL_KEY])
    if env_locales:
      locales = env_locales.split()
    else:
      locales = self.config[LOCALES_KEY]
      # Check rtl_locales are contained in locales.
      unknown_rtl_locales = rtl_locales - set(locales)
      if unknown_rtl_locales:
        raise BuildImageError('Unknown locales %s in %s' %
                              (list(unknown_rtl_locales), RTL_KEY))
    self.locales = [LocaleInfo(code, code in rtl_locales)
                    for code in locales]

  @classmethod
  def get_text_colors(cls, dpi):
    """Derive maximum text colors from `dpi`."""
    if dpi < 64:
      return 2
    elif dpi < 72:
      return 3
    elif dpi < 80:
      return 4
    elif dpi < 96:
      return 5
    elif dpi < 112:
      return 6
    else:
      return 7

  def _to_px(self, length, num_lines=1):
    """Converts the relative coordinate to absolute one in pixels."""
    return int(self.canvas_px * length / SCALE_BASE) * num_lines

  def _get_png_height(self, png_file):
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

  def convert_svg_to_png(self, svg_file, png_file, height, num_lines,
                         background):
    """Converts .svg file to .png file."""
    background_hex = ''.join(format(x, '02x') for x in background)
    # If the width/height of the SVG file is specified in points, the
    # rsvg-convert command with default 90DPI will potentially cause the pixels
    # at the right/bottom border of the output image to be transparent (or
    # filled with the specified background color).  This seems like an
    # rsvg-convert issue regarding image scaling.  Therefore, use 72DPI here
    # to avoid the scaling.
    command = ['rsvg-convert',
               '--background-color', "'#%s'" % background_hex,
               '--dpi-x', '72',
               '--dpi-y', '72',
               '-o', png_file]
    height_px = self._to_px(height, num_lines)
    if height_px <= 0:
      raise BuildImageError('Height of %r <= 0 (%dpx)' %
                            (os.path.basename(svg_file), height_px))
    command.extend(['--height', '%d' % height_px])
    command.append(svg_file)
    subprocess.check_call(' '.join(command), shell=True)

  def convert_to_bitmap(self, input_file, height, num_lines, background, output,
                        max_colors):
    """Converts an image file `input_file` to a BMP file `output`."""
    image = Image.open(input_file)

    # Process alpha channel and transparency.
    if image.mode == 'RGBA':
      target = Image.new('RGB', image.size, background)
      image.load()  # required for image.split()
      mask = image.split()[-1]
      target.paste(image, mask=mask)
    elif (image.mode == 'P') and ('transparency' in image.info):
      exit('Sorry, PNG with RGBA palette is not supported.')
    elif image.mode != 'RGB':
      target = image.convert('RGB')
    else:
      target = image

    width_px, height_px = image.size
    max_height_px = self._to_px(height, num_lines)
    # If the image size is larger than what will be displayed at runtime,
    # downscale it.
    if height_px > max_height_px:
      if not self.dpi_warning_printed:
        print('Reducing effective DPI to %d, limited by screen resolution' %
              (self.config[DPI_KEY] * max_height_px // height_px))
        self.dpi_warning_printed = True
      height_px = max_height_px
      width_px = height_px * image.size[0] // image.size[1]
    # Stretch image horizontally for stretched display.
    if self.panel_stretch != 1:
      width_px = int(width_px * self.panel_stretch)
    new_size = width_px, height_px
    if new_size != image.size:
      target = target.resize(new_size, Image.BICUBIC)

    # Export and downsample color space.
    target.convert('P', dither=None, colors=max_colors, palette=Image.ADAPTIVE
                   ).save(output)

    with open(output, 'rb+') as f:
      f.seek(BMP_HEADER_OFFSET_NUM_LINES)
      f.write(bytearray([num_lines]))

  def convert(self, files, output_dir, heights, max_widths, max_colors,
              one_line_dir=None):
    """Converts file(s) to bitmap format."""
    if not files:
      raise BuildImageError('Unable to find file(s) to convert')

    for file in files:
      name, ext = os.path.splitext(os.path.basename(file))

      if name in self.rename_map:
        new_name = self.rename_map[name]
        if not new_name:
          continue
      else:
        new_name = name
      output = os.path.join(output_dir, new_name + self.DEFAULT_OUTPUT_EXT)

      background = self.BACKGROUND_COLORS.get(name, self.DEFAULT_BACKGROUND)
      height = heights[name]
      max_width = max_widths[name]

      # Determine num_lines in order to scale the image
      if one_line_dir and max_width:
        num_lines = self.get_num_lines(file, one_line_dir)
      else:
        num_lines = 1

      if ext == '.svg':
        png_file = os.path.join(self.temp_dir, name + '.png')
        self.convert_svg_to_png(file, png_file, height, num_lines, background)
        file = png_file

      self.convert_to_bitmap(file, height, num_lines, background, output,
                             max_colors)

  def convert_sprite_images(self):
    """Converts sprite images."""
    names = self.formats[KEY_SPRITE_FILES]
    styles = self.formats[KEY_STYLES]
    # Check redundant images
    for filename in glob.glob(os.path.join(self.ASSET_DIR, SVG_FILES)):
      name, _ = os.path.splitext(os.path.basename(filename))
      if name not in names:
        raise BuildImageError('Sprite image %r not specified in %s' %
                              (filename, FORMAT_FILE))
    # Convert images
    files = []
    heights = {}
    for name, category in names.items():
      style = get_config_with_defaults(styles, category)
      files.append(os.path.join(self.ASSET_DIR, name + '.svg'))
      heights[name] = style[KEY_HEIGHT]
    max_widths = defaultdict(lambda: None)
    self.convert(files, self.output_dir, heights, max_widths,
                 self.ASSET_MAX_COLORS)

  def build_generic_strings(self):
    """Builds images of generic (locale-independent) strings."""
    dpi = self.config[DPI_KEY]

    names = self.formats[KEY_GENERIC_FILES]
    styles = self.formats[KEY_STYLES]
    fonts = self.formats[KEY_FONTS]
    default_font = fonts[KEY_DEFAULT]

    files = []
    heights = {}
    max_widths = {}
    for txt_file in glob.glob(os.path.join(self.strings_dir, '*.txt')):
      name, _ = os.path.splitext(os.path.basename(txt_file))
      category = names[name]
      style = get_config_with_defaults(styles, category)
      convert_text_to_image(None, txt_file, default_font, self.stage_dir,
                            height=style[KEY_HEIGHT],
                            max_width=style[KEY_MAX_WIDTH],
                            dpi=dpi,
                            bgcolor=style[KEY_BGCOLOR],
                            fgcolor=style[KEY_FGCOLOR])
      files.append(os.path.join(self.stage_dir, name + '.png'))
      heights[name] = style[KEY_HEIGHT]
      max_widths[name] = style[KEY_MAX_WIDTH]
    self.convert(files, self.output_dir, heights, max_widths,
                 self.text_max_colors)

  def _check_text_width(self, output_dir, heights, max_widths):
    """Check if the width of text image will exceed canvas boundary."""
    for filename in glob.glob(os.path.join(output_dir,
                                           '*' + self.DEFAULT_OUTPUT_EXT)):
      name, _ = os.path.splitext(os.path.basename(filename))
      max_width = max_widths[name]
      if not max_width:
        continue
      max_width_px = self._to_px(max_width)
      with open(filename, 'rb') as f:
        f.seek(BMP_HEADER_OFFSET_NUM_LINES)
        num_lines = f.read(1)[0]
      height_px = self._to_px(heights[name] * num_lines)
      with Image.open(filename) as image:
        width_px = height_px * image.size[0] // image.size[1]
      if width_px > max_width_px:
        raise BuildImageError('%s: Image width %dpx greater than max width '
                              '%dpx' % (filename, width_px, max_width_px))

  def _copy_missing_bitmaps(self):
    """Copy missing (not yet translated) strings from locale 'en'."""
    en_files = glob.glob(os.path.join(self.output_ro_dir, 'en',
                                      '*' + self.DEFAULT_OUTPUT_EXT))
    for locale_info in self.locales:
      locale = locale_info.code
      if locale == 'en':
        continue
      ro_locale_dir = os.path.join(self.output_ro_dir, locale)
      for en_file in en_files:
        filename = os.path.basename(en_file)
        locale_file = os.path.join(ro_locale_dir, filename)
        if not os.path.isfile(locale_file):
          print("WARNING: Locale '%s': copying '%s'" % (locale, filename))
          shutil.copyfile(en_file, locale_file)

  def generate_localized_pngs(self, names, json_dir):
    """Generates PNG files for localized strings."""
    styles = self.formats[KEY_STYLES]
    fonts = self.formats[KEY_FONTS]
    default_font = fonts[KEY_DEFAULT]
    dpi = self.config[DPI_KEY]

    # Ignore SIGINT in child processes
    sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    pool = multiprocessing.Pool(multiprocessing.cpu_count())
    signal.signal(signal.SIGINT, sigint_handler)

    results = []
    for locale_info in self.locales:
      locale = locale_info.code
      print(locale, end=' ', flush=True)
      inputs = parse_locale_json_file(locale, json_dir)

      # Walk locale directory to add pre-generated texts such as language names.
      for txt_file in glob.glob(os.path.join(self.locale_dir, locale, '*.txt')):
        name, _ = os.path.splitext(os.path.basename(txt_file))
        with open(txt_file, 'r', encoding='utf-8-sig') as f:
          inputs[name] = f.read().strip()

      output_dir = os.path.join(self.stage_locale_dir, locale)
      os.makedirs(output_dir, exist_ok=True)

      for name, category in names.items():
        # Ignore missing translation
        if locale != 'en' and name not in inputs:
          continue

        # Write to text file
        text_file = os.path.join(output_dir, name + '.txt')
        with open(text_file, 'w', encoding='utf-8-sig') as f:
          f.write(inputs[name] + '\n')

        # Convert to PNG file
        style = get_config_with_defaults(styles, category)
        args = (
            locale,
            os.path.join(output_dir, '%s.txt' % name),
            fonts.get(locale, default_font),
            output_dir,
        )
        kwargs = {
            'height': style[KEY_HEIGHT],
            'max_width': style[KEY_MAX_WIDTH],
            'dpi': dpi,
            'bgcolor': style[KEY_BGCOLOR],
            'fgcolor': style[KEY_FGCOLOR],
        }
        results.append(pool.apply_async(convert_text_to_image, args, kwargs))

    print()
    pool.close()

    try:
      for r in results:
        r.get()
    except KeyboardInterrupt:
      pool.terminate()
      pool.join()
      exit('Aborted by user')
    else:
      pool.join()

  def convert_localized_pngs(self, names):
    """Converts PNGs of localized strings to BMPs."""
    styles = self.formats[KEY_STYLES]
    fonts = self.formats[KEY_FONTS]
    default_font = fonts[KEY_DEFAULT]

    heights = {}
    max_widths = {}
    for name, category in names.items():
      style = get_config_with_defaults(styles, category)
      heights[name] = style[KEY_HEIGHT]
      max_widths[name] = style[KEY_MAX_WIDTH]

    # Using stderr to report progress synchronously
    print('  processing:', end='', file=sys.stderr, flush=True)
    for locale_info in self.locales:
      locale = locale_info.code
      ro_locale_dir = os.path.join(self.output_ro_dir, locale)
      stage_locale_dir = os.path.join(self.stage_locale_dir, locale)
      print(' ' + locale, end='', file=sys.stderr, flush=True)
      os.makedirs(ro_locale_dir)
      self.convert(
          glob.glob(os.path.join(stage_locale_dir, PNG_FILES)),
          ro_locale_dir, heights, max_widths, self.text_max_colors,
          one_line_dir=os.path.join(stage_locale_dir, ONE_LINE_DIR))
      self._check_text_width(ro_locale_dir, heights, max_widths)
    print(file=sys.stderr)

  def build_localized_strings(self):
    """Builds images of localized strings."""
    # Sources are one .grd file with identifiers chosen by engineers and
    # corresponding English texts, as well as a set of .xlt files (one for each
    # language other than US English) with a mapping from hash to translation.
    # Because the keys in the xlt files are a hash of the English source text,
    # rather than our identifiers, such as "btn_cancel", we use the "grit"
    # command line tool to process the .grd and .xlt files, producing a set of
    # .json files mapping our identifier to the translated string, one for every
    # language including US English.

    # Create a temporary directory to place the translation output from grit in.
    json_dir = tempfile.mkdtemp()

    # This invokes the grit build command to generate JSON files from the XTB
    # files containing translations.  The results are placed in `json_dir` as
    # specified in firmware_strings.grd, i.e. one JSON file per locale.
    subprocess.check_call([
        'grit',
        '-i', os.path.join(self.locale_dir, STRINGS_GRD_FILE),
        'build',
        '-o', os.path.join(json_dir),
    ])

    # Make a copy to avoid modifying `self.formats`
    names = copy.deepcopy(self.formats[KEY_LOCALIZED_FILES])
    if DIAGNOSTIC_UI:
      names.update(self.formats[KEY_DIAGNOSTIC_FILES])

    # TODO(b/163109632): Merge generate_localized_pngs() and
    # convert_localized_pngs(), and parallelize them altogether.
    self.generate_localized_pngs(names, json_dir)
    shutil.rmtree(json_dir)

    self.convert_localized_pngs(names)
    self._copy_missing_bitmaps()

  def move_language_images(self):
    """Renames language bitmaps and move to self.output_dir.

    The directory self.output_dir contains locale-independent images, and is
    used for creating vbgfx.bin by archive_images.py.
    """
    for locale_info in self.locales:
      locale = locale_info.code
      ro_locale_dir = os.path.join(self.output_ro_dir, locale)
      old_file = os.path.join(ro_locale_dir, 'language.bmp')
      new_file = os.path.join(self.output_dir, 'language_%s.bmp' % locale)
      if os.path.exists(new_file):
        raise BuildImageError('File already exists: %s' % new_file)
      shutil.move(old_file, new_file)

  def build_glyphs(self):
    """Builds glyphs of ascii characters."""
    os.makedirs(self.stage_font_dir, exist_ok=True)
    files = []
    # TODO(b/163109632): Parallelize the conversion of glyphs
    for c in range(ord(' '), ord('~') + 1):
      name = f'idx{c:03d}_{c:02x}'
      txt_file = os.path.join(self.stage_font_dir, name + '.txt')
      with open(txt_file, 'w', encoding='ascii') as f:
        f.write(chr(c))
        f.write('\n')
      convert_text_to_image(None, txt_file, GLYPH_FONT, self.stage_font_dir,
                            height=DEFAULT_GLYPH_HEIGHT, use_svg=True)
      files.append(os.path.join(self.stage_font_dir, name + '.svg'))
    heights = defaultdict(lambda: DEFAULT_GLYPH_HEIGHT)
    max_widths = defaultdict(lambda: None)
    font_output_dir = os.path.join(self.output_dir, 'font')
    os.makedirs(font_output_dir)
    self.convert(files, font_output_dir, heights, max_widths,
                 self.text_max_colors)

  def copy_images_to_rw(self):
    """Copies localized images specified in boards.yaml for RW override."""
    if not self.config[RW_OVERRIDE_KEY]:
      print('  No localized images are specified for RW, skipping')
      return

    for locale_info in self.locales:
      locale = locale_info.code
      rw_locale_dir = os.path.join(self.output_ro_dir, locale)
      ro_locale_dir = os.path.join(self.output_rw_dir, locale)
      os.makedirs(rw_locale_dir)

      for name in self.config[RW_OVERRIDE_KEY]:
        ro_src = os.path.join(ro_locale_dir, name + self.DEFAULT_OUTPUT_EXT)
        rw_dst = os.path.join(rw_locale_dir, name + self.DEFAULT_OUTPUT_EXT)
        shutil.copyfile(ro_src, rw_dst)

  def create_locale_list(self):
    """Creates locale list as a CSV file.

    Each line in the file is of format "code,rtl", where
    - "code": language code of the locale
    - "rtl": "1" for right-to-left language, "0" otherwise
    """
    with open(os.path.join(self.output_dir, 'locales'), 'w') as f:
      for locale_info in self.locales:
        f.write('{},{}\n'.format(locale_info.code,
                                 int(locale_info.rtl)))

  def build(self):
    """Builds all images required by a board."""
    # Clean up output directory
    if os.path.exists(self.output_dir):
      shutil.rmtree(self.output_dir)
    os.makedirs(self.output_dir)

    if not os.path.exists(self.stage_dir):
      raise BuildImageError('Missing stage folder. Run make in strings dir.')

    # Clean up temp directory
    if os.path.exists(self.temp_dir):
      shutil.rmtree(self.temp_dir)
    os.makedirs(self.temp_dir)

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
    formats = yaml.load(f)
  board_config = load_boards_config(BOARDS_CONFIG_FILE)[board]

  print('Building for ' + board)
  check_fonts(formats[KEY_FONTS])
  print('Output dir: ' + OUTPUT_DIR)
  converter = Converter(board, formats, board_config, OUTPUT_DIR)
  converter.build()


if __name__ == '__main__':
  main()
