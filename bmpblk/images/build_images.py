#!/usr/bin/env python2
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Prepares image resources into output folder, and generates final bitmap block
output file.

Usage:
  ./build_images.py <board-names>

  Use 'ALL' in <board-names> (or don't specify any names) if you want to build
  every boards defined in configuration file.
'''

import copy
from collections import defaultdict, namedtuple
import glob
import os
import shutil
import subprocess
import sys

from PIL import Image
import yaml

LOCALE_DIR = 'locale'
FONT_DIR = 'font'
PNG_FILES = '*.png'
SVG_FILES = '*.svg'

# YAML key names.
BOARDS_CONFIG = 'boards.yaml'
DEFAULT_NAME = '_DEFAULT_'
SCREEN_KEY = 'screen'
PANEL_KEY = 'panel'
SDCARD_KEY = 'sdcard'
BAD_USB3_KEY = 'bad_usb3'
LOCALES_KEY = 'locales'
RTL_KEY = 'rtl'
HI_RES_KEY = 'hi_res'
TEXT_COLORS_KEY = 'text_colors'

LocaleInfo = namedtuple('LocaleInfo', ['code', 'rtl', 'hi_res'])


class BuildImageError(Exception):
  """The exception class for all errors generated during build image process"""
  pass


class Converter(object):
  """Converts assets, texts, URLs, and fonts to bitmap images.

  Attributes:
    ASSET_DIR (str): Directory of image assets.
    DEFAULT_OUTPUT_EXT (str): Default output file extension.
    DEFAULT_REPLACE_MAP (dict): Default mapping of file replacement. For
      {'a': 'b'}, "a.*" will be converted to "b.*".
    SCALE_BASE (int): See ASSET_SCALES below.
    ASSET_SCALES (dict): Scale of each image asset. Key is the image name and
      value is a tuple (w, h), which are the width and height relative to the
      screen resolution. For example, if SCALE_BASE is 1000, (500, 100) means
      the image will be scaled to 50% of the screen width and 10% of the screen
      height.
    TEXT_SCALES (dict): Scale of each localized text image. The meaning is
      similar to ASSET_SCALES.
    ASSET_MAX_COLORS (int): Maximum colors to use for converting image assets
      to bitmaps.
    DEFAULT_BACKGROUND (tuple): Default background color.
    BACKGROUND_COLORS (dict): Background color of each image. Key is the image
      name and value is a tuple of RGB values.

  """

  ASSET_DIR = 'assets'
  DEFAULT_OUTPUT_EXT = '.bmp'

  DEFAULT_REPLACE_MAP = {
    'rec_sel_desc1_no_sd': '',
    'rec_sel_desc1_no_phone_no_sd': '',
    'rec_disk_step1_desc0_no_sd': '',
    'rec_to_dev_desc1_phyrec': '',
    'rec_to_dev_desc1_power': '',
    'navigate_tablet': '',
    'nav-button_power': '',
    'nav-button_volume_up': '',
    'nav-button_volume_down': '',
    'broken_desc0_phyrec': '',
    'broken_desc1_phyrec': '',
    'broken_desc0_detach': '',
    'broken_desc1_detach': '',
  }

  # scales
  SCALE_BASE = 1000     # 100.0%

  # These are supposed to be kept in sync with the numbers set in depthcharge
  # to avoid runtime scaling, which makes images blurry.
  DEFAULT_ASSET_SCALE = (0, 30)
  DEFAULT_TEXT_SCALE = (0, 30)
  DEFAULT_FONT_SCALE = (0, 36)
  LANG_MENU_SCALE = (0, 36)
  ICON_SCALE = (0, 45)
  STEP_ICON_SCALE = (0, 28)
  TITLE_SCALE = (0, 56)
  BUTTON_SCALE = (0, 26)
  BUTTON_ARROW_SCALE = (0, 20)
  QR_FOOTER_SCALE = (0, 108)
  QR_DESC_SCALE = (0, 230)
  FOOTER_TEXT_SCALE = (0, 24)

  ASSET_SCALES = {
    'separator': None,
    'ic_globe': (0, 20),
    'ic_dropdown': (0, 24),
    'ic_info': ICON_SCALE,
    'ic_warning': ICON_SCALE,
    'ic_dev_mode': ICON_SCALE,
    'ic_restart': ICON_SCALE,
    'ic_1': STEP_ICON_SCALE,
    'ic_1-done': STEP_ICON_SCALE,
    'ic_2': STEP_ICON_SCALE,
    'ic_2-done': STEP_ICON_SCALE,
    'ic_3': STEP_ICON_SCALE,
    'ic_3-done': STEP_ICON_SCALE,
    'ic_done': STEP_ICON_SCALE,
    'ic_error': STEP_ICON_SCALE,
    'ic_dropleft': BUTTON_ARROW_SCALE,
    'ic_dropleft_focus': BUTTON_ARROW_SCALE,
    'ic_dropright': BUTTON_ARROW_SCALE,
    'ic_dropright_focus': BUTTON_ARROW_SCALE,
    'qr_rec': QR_FOOTER_SCALE,
    'qr_rec_phone': QR_DESC_SCALE,
  }

  TEXT_SCALES = {
    'lang_menu': LANG_MENU_SCALE,
    'lang_menu_focus': LANG_MENU_SCALE,
    'firmware_sync_title': TITLE_SCALE,
    'broken_title': TITLE_SCALE,
    'adv_options_title': TITLE_SCALE,
    'rec_sel_title': TITLE_SCALE,
    'rec_step1_title': TITLE_SCALE,
    'rec_phone_step2_title': TITLE_SCALE,
    'rec_disk_step2_title': TITLE_SCALE,
    'rec_disk_step3_title': TITLE_SCALE,
    'rec_invalid_title': TITLE_SCALE,
    'rec_to_dev_title': TITLE_SCALE,
    'dev_title': TITLE_SCALE,
    'dev_to_norm_title': TITLE_SCALE,
    'btn_dev_mode': BUTTON_SCALE,
    'btn_dev_mode_focus': BUTTON_SCALE,
    'btn_rec_by_phone': BUTTON_SCALE,
    'btn_rec_by_phone_focus': BUTTON_SCALE,
    'btn_rec_by_disk': BUTTON_SCALE,
    'btn_rec_by_disk_focus': BUTTON_SCALE,
    'btn_adv_options': BUTTON_SCALE,
    'btn_adv_options_focus': BUTTON_SCALE,
    'btn_secure_mode': BUTTON_SCALE,
    'btn_secure_mode_focus': BUTTON_SCALE,
    'btn_int_disk': BUTTON_SCALE,
    'btn_int_disk_focus': BUTTON_SCALE,
    'btn_ext_disk': BUTTON_SCALE,
    'btn_ext_disk_focus': BUTTON_SCALE,
    'btn_next': BUTTON_SCALE,
    'btn_next_focus': BUTTON_SCALE,
    'btn_back': BUTTON_SCALE,
    'btn_back_focus': BUTTON_SCALE,
    'btn_confirm': BUTTON_SCALE,
    'btn_confirm_focus': BUTTON_SCALE,
    'btn_cancel': BUTTON_SCALE,
    'btn_cancel_focus': BUTTON_SCALE,
    'model': FOOTER_TEXT_SCALE,
    'help_center': FOOTER_TEXT_SCALE,
    'rec_url': FOOTER_TEXT_SCALE,
    'to_navigate': FOOTER_TEXT_SCALE,
    'navigate': FOOTER_TEXT_SCALE,
    'navigate_tablet': FOOTER_TEXT_SCALE,
  }

  # background colors
  DEFAULT_BACKGROUND = (0x20, 0x21, 0x24)
  LANG_HEADER_BACKGROUND = (0x16, 0x17, 0x19)
  LINK_SELECTED_BACKGROUND = (0x2a, 0x2f, 0x39)
  ASSET_MAX_COLORS = 128

  BACKGROUND_COLORS = {
    'ic_dropdown': LANG_HEADER_BACKGROUND,
    'ic_dropleft_focus': LINK_SELECTED_BACKGROUND,
    'ic_dropright_focus': LINK_SELECTED_BACKGROUND,
  }

  def __init__(self, board, config):
    """
    Args:
      board: a string, name of the board to use.
      config: a dictionary of configuration parameters.
    """
    self.board = board
    self.config = config
    self.set_dirs()
    self.set_screen()
    self.set_replace_map()
    self.set_locales()
    self.text_max_colors = self.config[TEXT_COLORS_KEY]

  def set_dirs(self):
    """Set output directory and stage directory"""
    output_base = os.getenv('OUTPUT', os.path.join('..', 'build'))
    self.output_dir = os.path.join(output_base, self.board)
    self.stage_dir = os.path.join(output_base, '.stage')
    self.temp_dir = os.path.join(self.stage_dir, 'tmp')

  def set_screen(self):
    """Set screen width and height"""
    self.screen_width, self.screen_height = self.config[SCREEN_KEY]

    self.stretch = (1, 1)
    if self.config[PANEL_KEY]:
      # Calculate 'stretch'. It's used to shrink images horizontally so that
      # resulting images will look proportional to the original image on the
      # stretched display. If the display is not stretched, meaning aspect
      # ratio is same as the screen where images were rendered (1366x766),
      # no shrinking is performed.
      panel_width, panel_height = self.config[PANEL_KEY]
      self.stretch = (self.screen_width * panel_height,
                      self.screen_height * panel_width)

    if self.stretch[0] > self.stretch[1]:
      raise BuildImageError('Panel aspect ratio (%f) is smaller than screen '
                            'aspect ratio (%f). It indicates screen will be '
                            'shrunk horizontally. It is currently unsupported.'
                            % (float(panel_width) / panel_height,
                               float(self.screen_width) / self.screen_height))

    # Set up square drawing area
    # TODO: Depthcharge should narrow the canvas if the screen is stretched.
    self.canvas_px = min(self.screen_width, self.screen_height)

  def set_replace_map(self):
    """Set a map replacing images.

    For each (key, value), image 'key' will be replaced by image 'value'.
    """
    replace_map = self.DEFAULT_REPLACE_MAP.copy()

    if os.getenv('DETACHABLE') == '1':
      replace_map.update({
        'nav-key_enter': 'nav-button_power',
        'nav-key_up': 'nav-button_volume_up',
        'nav-key_down': 'nav-button_volume_down',
        'navigate': 'navigate_tablet',
        'broken_desc0': 'broken_desc0_detach',
        'broken_desc1': 'broken_desc1_detach',
      })

    physical_presence = os.getenv('PHYSICAL_PRESENCE')
    if physical_presence == 'recovery':
      replace_map['rec_to_dev_desc1'] = 'rec_to_dev_desc1_phyrec'
      replace_map['broken_desc0'] = 'broken_desc0_phyrec'
      replace_map['broken_desc1'] = 'broken_desc1_phyrec'
    elif physical_presence == 'power':
      replace_map['rec_to_dev_desc1'] = 'rec_to_dev_desc1_power'
    elif physical_presence != 'keyboard':
      raise BuildImageError('Invalid physical presence setting %s for board %s'
                            % (physical_presence, self.board))

    if not self.config[SDCARD_KEY]:
      replace_map.update({
        'rec_sel_desc1': 'rec_sel_desc1_no_sd',
        'rec_sel_desc1_no_phone': 'rec_sel_desc1_no_phone_no_sd',
        'rec_disk_step1_desc0': 'rec_disk_step1_desc0_no_sd',
      })

    self.replace_map = replace_map

  def set_locales(self):
    """Set a list of locales for which localized images are converted"""
    # LOCALES environment variable can overwrite boards.yaml
    rtl_locales = set(self.config[RTL_KEY])
    hi_res_locales = set(self.config[HI_RES_KEY])
    # TODO(b/144969853): Support all locales for MENU_UI.
    locales = ['en']
    self.locales = [LocaleInfo(code, code in rtl_locales,
                               code in hi_res_locales)
                    for code in locales]

  def calculate_dimension(self, original, scale):
      """Calculate scaled width and height

      This imitates the function of Depthcharge with the same name.

      Args:
        original: (width, height) of the original image
        scale: (x, y) scale parameter relative to the canvas size using
            SCALE_BASE as a base.

      Returns:
        (width, height) of the scaled image
      """
      dim_width, dim_height = (0, 0)
      scale_x, scale_y = scale
      org_width, org_height = original

      if scale_x == 0 and scale_y == 0:
        raise BuildImageError('Invalid scale parameter: %s' % (scale))
      if scale_x > 0:
        dim_width = self.canvas_px * scale_x / self.SCALE_BASE
      if scale_y > 0:
        dim_height = self.canvas_px * scale_y / self.SCALE_BASE
      if scale_x == 0:
        dim_width = org_width * dim_height / org_height
      if scale_y == 0:
        dim_height = org_height * dim_width / org_width

      dim_width = dim_width * self.stretch[0] / self.stretch[1]

      return (dim_width, dim_height)

  def convert_svg_to_png(self, svg_file, png_file, scale, background):
    """Convert .svg file to .png file"""
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
    if scale:
        width = int(self.canvas_px * scale[0] / self.SCALE_BASE)
        height = int(self.canvas_px * scale[1] / self.SCALE_BASE)
        if width:
            command.extend(['--width', '%d' % width])
        if height:
            command.extend(['--height', '%d' % height])
    command.append(svg_file)
    subprocess.check_call(' '.join(command), shell=True)

  def convert_to_bitmap(self, input, scale, background, output, max_colors):
    """Convert an image file to the bitmap format"""
    image = Image.open(input)

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

    # Process scaling
    if scale:
      new_size = self.calculate_dimension(image.size, scale)
      if new_size[0] == 0 or new_size[1] == 0:
        print 'Scaling', input
        print 'Warning: width or height is 0 after resizing:',
        print 'scale=%s size=%s stretch=%s new_size=%s' % (
              scale, image.size, self.stretch, new_size)
        return
      target = target.resize(new_size, Image.ANTIALIAS)

    # Export and downsample color space.
    target.convert('P', dither=None, colors=max_colors, palette=Image.ADAPTIVE
                   ).save(output)

  def convert(self, files, output_dir, scales, max_colors):
    """Convert file(s) to bitmap format"""
    if not files:
      raise BuildImageError('Unable to find file(s) to convert')

    for file in files:
      name, ext = os.path.splitext(os.path.basename(file))
      output = os.path.join(output_dir, name + self.DEFAULT_OUTPUT_EXT)

      background = self.BACKGROUND_COLORS.get(name, self.DEFAULT_BACKGROUND)
      scale = scales[name]

      if name in self.replace_map:
        name = self.replace_map[name]
        if not name:
          continue
        print 'Replace: %s => %s' % (file, name)
        file = os.path.join(os.path.dirname(file), name + ext)

      if ext == '.svg':
        png_file = os.path.join(self.temp_dir, name + '.png')
        self.convert_svg_to_png(file, png_file, scale, background)
        file = png_file

      self.convert_to_bitmap(file, scale, background, output, max_colors)

  def convert_assets(self):
    """Convert images in assets folder"""
    files = []
    files.extend(glob.glob(os.path.join(self.ASSET_DIR, SVG_FILES)))
    files.extend(glob.glob(os.path.join(self.ASSET_DIR, PNG_FILES)))
    scales = defaultdict(lambda: self.DEFAULT_ASSET_SCALE)
    scales.update(self.ASSET_SCALES)
    self.convert(files, self.output_dir, scales, self.ASSET_MAX_COLORS)

  def convert_texts(self):
    """Convert localized texts"""
    locale_dir = os.path.join(self.stage_dir, LOCALE_DIR)
    # Using stderr to report progress synchronously
    sys.stderr.write('  processing:')
    for locale_info in self.locales:
      locale = locale_info.code
      output_dir = os.path.join(self.output_dir, LOCALE_DIR, locale)
      if locale_info.hi_res:
        scales = defaultdict(lambda: self.DEFAULT_TEXT_SCALE)
        scales.update(self.TEXT_SCALES)
        sys.stderr.write(' ' + locale)
      else:
        # We use low-res images for these locales and turn off scaling
        # to make the files fit in a ROM. Note that these text images will
        # be scaled by Depthcharge to be the same height as hi-res texts.
        scales = defaultdict(lambda: None)
        sys.stderr.write(' ' + locale + '/lo')
      os.makedirs(output_dir)
      self.convert(glob.glob(os.path.join(locale_dir, locale, SVG_FILES)),
                   output_dir, scales, self.text_max_colors)
    sys.stderr.write('\n')

  def move_language_images(self):
    """Rename language bitmaps and move to self.output_dir.

    The directory self.output_dir contains locale-independent images, and is
    used for creating vbgfx.bin by archive_images.py.
    """
    for locale_info in self.locales:
      locale = locale_info.code
      locale_output_dir = os.path.join(self.output_dir, LOCALE_DIR, locale)
      for old_name, new_name in [
        ('lang_header', 'lang_header_%s' % locale),
        ('lang_menu', 'lang_menu_%s' % locale),
        ('lang_menu_focus', 'lang_menu_%s_focus' % locale),
      ]:
        old_file = os.path.join(locale_output_dir, old_name + '.bmp')
        new_file = os.path.join(self.output_dir, new_name + '.bmp')
        if os.path.exists(new_file):
          raise BuildImageError('File already exists: ' % new_file)
        shutil.move(old_file, new_file)

  def convert_fonts(self):
    """Convert font images"""
    scales = defaultdict(lambda: self.DEFAULT_FONT_SCALE)
    font_dir = os.path.join(self.stage_dir, FONT_DIR)
    files = glob.glob(os.path.join(font_dir, PNG_FILES))
    font_output_dir = os.path.join(self.output_dir, FONT_DIR)
    os.makedirs(font_output_dir)
    self.convert(files, font_output_dir, scales, self.text_max_colors)

  def create_locale_list(self):
    """Create locale list as a CSV file

    Each line in the file is of format "code,rtl", where
    - "code": language code of the locale
    - "rtl": "1" for right-to-left language, "0" otherwise
    """
    with open(os.path.join(self.output_dir, 'locales'), 'w') as f:
      for locale_info in self.locales:
        f.write('{},{}\n'.format(locale_info.code,
                                 int(locale_info.rtl)))

  def build_image(self):
    """Builds all images required by a board"""
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

    print 'Converting asset images...'
    self.convert_assets()

    print 'Converting localized text images...'
    self.convert_texts()

    print 'Moving language images to locale-independent directory...'
    self.move_language_images()

    print 'Creating locale list file...'
    self.create_locale_list()

    print 'Converting fonts...'
    self.convert_fonts()


class LegacyConverter(Converter):
  """Converts assets, texts, URLs, and fonts to bitmap images for legacy UIs"""

  ASSET_DIR = 'legacy_assets'

  DEFAULT_REPLACE_MAP = {
    'BadSD': '',
    'BadUSB': '',
    'InsertUSB': '',
    'RemoveDevices': '',
    'RemoveSD': '',
    'RemoveUSB': '',
    'boot_usb_only': '',
    'insert_sd_usb2': '',
    'insert_usb2': '',
    'insert_usb': '',
    'todev_phyrec': '',
    'todev_power': '',
    'reserve_charging': '',
    'reserve_charging_empty': '',
  }

  # scales
  SCALE_BASE = 1000     # 100.0%

  # These are supposed to be kept in sync with the numbers set in depthcharge
  LEGACY_TEXT_HEIGHT = 36   #   3.6%
  DEFAULT_TEXT_SCALE = (0, LEGACY_TEXT_HEIGHT)
  DEFAULT_ASSET_SCALE = (0, 169)

  ASSET_SCALES = {
    'arrow_left': DEFAULT_TEXT_SCALE,
    'arrow_right': DEFAULT_TEXT_SCALE,
    'chrome_logo': (0, 39),
    'divider_btm': (900, 0),
    'divider_top': (900, 0),
    'globe': DEFAULT_TEXT_SCALE,
    'InsertDevices': (0, 371),
    'RemoveDevices': (0, 371),
    'reserve_charging': (0, 117),
    'reserve_charging_empty': (0, 117),
  }

  TEXT_SCALES = {
    'tonorm': (0, 4 * LEGACY_TEXT_HEIGHT),
    'insert_sd_usb2': (0, 2 * LEGACY_TEXT_HEIGHT),
    'insert_usb2': (0, 2 * LEGACY_TEXT_HEIGHT),
    'os_broken': (0, 2 * LEGACY_TEXT_HEIGHT),
    'todev': (0, 4 * LEGACY_TEXT_HEIGHT),
    'todev_phyrec': (0, 4 * LEGACY_TEXT_HEIGHT),
    'todev_power':  (0, 4 * LEGACY_TEXT_HEIGHT),
    'tonorm': (0, 4 * LEGACY_TEXT_HEIGHT),
    'update': (0, 3 * LEGACY_TEXT_HEIGHT),
    'wrong_power_supply': (0, 4 * LEGACY_TEXT_HEIGHT),
    'navigate': (0, 2 * LEGACY_TEXT_HEIGHT),
    'disable_warn': (0, 2 * LEGACY_TEXT_HEIGHT),
    'enable_hint': (0, 2 * LEGACY_TEXT_HEIGHT),
    'confirm_hint': (0, 2 * LEGACY_TEXT_HEIGHT),
    'diag_confirm': (0, 3 * LEGACY_TEXT_HEIGHT),
  }

  # background colors
  DEFAULT_BACKGROUND = (255, 255, 255)
  LEGACY_CHARGE_BACKGROUND = (0, 0, 0)

  BACKGROUND_COLORS = {
    'reserve_charging': LEGACY_CHARGE_BACKGROUND,
    'reserve_charging_empty': LEGACY_CHARGE_BACKGROUND,
  }

  def set_replace_map(self):
    """Set a map replacing images.

    For each (key, value), image 'key' will be replaced by image 'value'.
    """
    sdcard = self.config[SDCARD_KEY]
    bad_usb3 = self.config[BAD_USB3_KEY]
    physical_presence = os.getenv('PHYSICAL_PRESENCE')

    self.replace_map = self.DEFAULT_REPLACE_MAP.copy()

    if not sdcard:
      self.replace_map['BadDevices'] = 'BadUSB'
      self.replace_map['InsertDevices'] = 'InsertUSB'
      self.replace_map['insert'] = ('insert_usb2' if bad_usb3 else 'insert_usb')
      self.replace_map['boot_usb'] = 'boot_usb_only'
    elif bad_usb3:
      self.replace_map['insert'] = 'insert_sd_usb2'

    if physical_presence == 'power':
      self.replace_map['todev'] = 'todev_power'
    elif physical_presence == 'recovery':
      self.replace_map['todev'] = 'todev_phyrec'
    elif physical_presence != 'keyboard':
      raise BuildImageError('Invalid physical presence setting %s for board %s'
                            % (physical_presence, self.board))

    if os.getenv("LEGACY_MENU_UI") == "1":
      self.replace_map['VerificationOff'] = ''

  def set_locales(self):
    """Set a list of locales for which localized images are converted"""
    # LOCALES environment variable can overwrite boards.yaml
    env_locales = os.getenv('LOCALES')
    rtl_locales = set(self.config[RTL_KEY])
    hi_res_locales = set(self.config[HI_RES_KEY])
    if env_locales:
      locales = env_locales.split()
    else:
      locales = self.config[LOCALES_KEY]
      # Check rtl_locales are contained in locales.
      unknown_rtl_locales = rtl_locales - set(locales)
      if unknown_rtl_locales:
        raise BuildImageError('Unknown locales %s in %s' %
                              (list(unknown_rtl_locales), RTL_KEY))
      # Check hi_res_locales are contained in locales.
      unknown_hi_res_locales = hi_res_locales - set(locales)
      if unknown_hi_res_locales:
        raise BuildImageError('Unknown locales %s in %s' %
                              (list(unknown_hi_res_locales), HI_RES_KEY))
    self.locales = [LocaleInfo(code, code in rtl_locales,
                               code in hi_res_locales)
                    for code in locales]

  def convert_url(self):
    """Convert URL and arrows"""
    # URL and arrows should be default height
    scales = defaultdict(lambda: self.DEFAULT_TEXT_SCALE)
    files = glob.glob(os.path.join(self.stage_dir, PNG_FILES))
    self.convert(files, self.output_dir, scales, self.text_max_colors)

  def convert_texts(self):
    """Convert localized texts"""
    locale_dir = os.path.join(self.stage_dir, LOCALE_DIR)
    # Using stderr to report progress synchronously
    sys.stderr.write('  processing:')
    for locale_info in self.locales:
      locale = locale_info.code
      output_dir = os.path.join(self.output_dir, LOCALE_DIR, locale)
      if locale_info.hi_res:
        scales = defaultdict(lambda: self.DEFAULT_TEXT_SCALE)
        scales.update(self.TEXT_SCALES)
      else:
        # We use low-res images for these locales and turn off scaling
        # to make the files fit in a ROM. Note that these text images will
        # be scaled by Depthcharge to be the same height as hi-res texts.
        locale += '/lo'
        scales = defaultdict(lambda: None)
      sys.stderr.write(' ' + locale)
      os.makedirs(output_dir)
      self.convert(glob.glob(os.path.join(locale_dir, locale, PNG_FILES)),
                   output_dir, scales, self.text_max_colors)
    sys.stderr.write('\n')

  def move_language_images(self):
    """Rename language bitmaps and move to self.output_dir."""
    # Do nothing for legacy UIs
    pass

  def build_image(self):
    """Builds all images required by a board"""
    super(LegacyConverter, self).build_image()

    print 'Converting URL images...'
    self.convert_url()


def load_boards_config(filename):
  """Loads the configuration of all boards from a YAML file

  Args:
    filename: file name of a YAML config file.

  Returns:
    A dictionary with keys as board names and values as config parameters.
  """
  with open(filename, 'r') as file:
    raw = yaml.load(file)

  configs = {}
  default = raw[DEFAULT_NAME]
  if not default:
    raise BuildImageError('Default configuration is not found')
  for boards, params in raw.iteritems():
    if boards == DEFAULT_NAME:
      continue
    config = copy.deepcopy(default)
    if params:
      config.update(params)
    for board in boards.replace(',', ' ').split():
      configs[board] = config

  return configs


def main(args):
  """Entry point when executed from command line

  Args:
    args: a list, boards to build. None for all boards.
  """
  configs = load_boards_config(BOARDS_CONFIG)

  targets = args
  if not targets or targets == ['ALL']:
    targets = configs.keys()

  print 'Building for', ', '.join(targets)

  for board in targets:
    if board not in configs:
      raise BuildImageError('%s not found in %s' % (board, BOARDS_CONFIG))
    print 'Building for', board
    if os.getenv('MENU_UI') == '1':
      converter = Converter(board, configs[board])
    else:
      converter = LegacyConverter(board, configs[board])
    converter.build_image()


if __name__ == '__main__':
  try:
    main(sys.argv[1:])
  except BuildImageError, err:
    sys.stderr.write("ERROR: %s\n" % err)
