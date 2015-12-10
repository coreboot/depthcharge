#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Scans images in current folder and generates a new YAML file for bmpblk_utility
to create bitmap block files.

Usage:
  ./make_default_yaml.py <locales>
"""

import Image
import os
import sys
import yaml

OUTPUT_FILE = "DEFAULT.yaml"
SCREEN_LIST = []
OPTIONAL_SCREENS = {}

# Default image names.
DEFAULT_BACKGROUND = 'Background'
TONORM_IMAGE_GROUP=[["VerificationOff"], ["@verif_off"], ["@tonorm"]]


class ImageInfo(object):
  """Keeps minimal information for an image file.

  Attributes:
    width: Image width.
    height: Image height.
    path: Image file path.
    name: A name (derived from path) to be used in YAML file.
  """

  def __init__(self, width, height, path):
    """Constructor for ImageInfo."""
    self.width = width
    self.height = height
    self.path = path
    self.name = os.path.splitext(path)[0].replace('locale/', '')
    if self.name == 'hwid_placeholder':
      self.name = '$HWID'
      self.path = 'hwid_fonts.font'


class ImageDatabase(object):
  """Manages and caches all accessed image files.

  Attributes:
    database: Dictionary for cached images' information.
  """

  def __init__(self):
    self.database = {}

  def get_image_info(self, name):
    """Gets the information from an image file.

    Args:
      name: Name of image file, without file extension.

    Returns:
      An ImageInfo object.
    """
    image = self.database.get(name, None)
    if image is None:
      path = name + '.bmp'
      raw_image = Image.open(path)
      image = ImageInfo(raw_image.size[0], raw_image.size[1], path)
      self.database[name] = image
    return image


class Screen(object):
  """An object to represent a firmware bitmap screen.

  Attributes:
    x, y: X and Y as insert location for next image.
    xpad, pyad: Padding size when adding multiple images.
    image_db: Database to load images.
    images: List of images on this screen (for YAML output).
    locale: Locale to be used for current screen.
    name: Name of screen for YAML output.
  """

  def __init__(self, name, locale, image_db, background=DEFAULT_BACKGROUND):
    self.x = 0
    self.y = 0
    # Arbitrary padding
    self.ypad = 5
    self.xpad = 5
    self.images = []
    self.image_db = image_db
    self.locale = locale
    self.name = name
    self.add_right_below(background)

  def get_image(self, name):
    """Gets the ImageInfo for a specified image name.

    Args:
      name: Name of image. '@' in name will be converted to current locale.

    Returns:
      An ImageInfo object containing image information.
    """
    assert type(name) is str
    return self.image_db.get_image_info(
        name.replace('@', 'locale/%s/' % self.locale))

  def move_pos_up(self, move):
    """Moves current insert location (to add new images) up.

    Args:
      move: Pixels to move.
    """
    self.y -= move

  def move_pos_down(self, move):
    """Moves current insert location (to add new images) down.

    Args:
      move: Pixels to move.
    """
    self.y += move

  def move_pos_left(self, move):
    """Moves current insert location (to add new images) left.

    Args:
      move: Pixels to move.
    """
    self.x -= move

  def move_pos_right(self, move):
    """Moves current insert location (to add new images) right.

    Args:
      move: Pixels to move.
    """
    self.x += move

  def add_right_below(self, *images):
    """Adds multiple images just by right-below of current insert location."""
    self.add_images(self.x, self.y, *images)

  def add_centered(self, *images):
    """Adds multiple images by using current insert location as center point."""
    y = round(self.y - self.get_max_height(*images) / 2)
    x = round(self.x - self.total_width(*images) / 2)
    self.add_images(x, y, *images)

  def add_centered_below(self, *images):
    """Adds multiple images, centered by current insert location horizontally,
       and below current insert location.
    """
    x = self.x - self.total_width(*images) / 2
    self.add_images(x, self.y, *images)

  def insert_centered_below(self, *images):
    """Adds a list of images and updates the insert location below the new
       images with padding.
    """
    self.add_centered_below(*images)
    height = self.get_max_height(*images)
    self.move_pos_down(height + self.ypad)

  def set_centered_y_percent(self, percent, image=DEFAULT_BACKGROUND):
    """Move insert location to the vertical mid line of a specified image,
    and percent% towards the bottom.
    Assumes image was inserted at 0,0 so only good for the background image.

    Args:
      percent: Percentage towards bottom.
      image: Reference image to calculate position.
    """
    self.x = round(self.get_width(image) / 2)
    self.y = round(self.get_height(image) * (percent / 100.0))

  def add_right_above(self, *images):
    """Adds a list of images right above insert location."""
    y = self.y - self.get_max_height(*images)
    self.add_images(self.x, y, *images)

  def add_left_above(self, *images):
    """Adds a list of images left above insert location."""
    y = self.y - self.get_max_height(*images)
    x = self.x - self.total_width(*images)
    self.add_images(x, y, *images)

  def get_width(self, image):
    """Returns width of an image."""
    return self.get_image(image).width

  def get_height(self, image):
    """Returns height of an image."""
    return self.get_image(image).height

  def total_width(self, *images):
    """Returns the width of a list of images (with padding)."""
    widths = [self.get_width(i) for i in images] or [0]
    return sum(widths) + self.xpad * (len(widths) - 1)

  def get_max_height(self, *images):
    """Returns the max height of a list of images."""
    return max((self.get_height(i) for i in images))

  def add_images(self, x, y, *images):
    """Adds a list of images at provided location.  Images of different heights
    are added centered on the tallest image.

    Args:
      x, y: Position to add image.
      images: List of images to add.
    """
    max_height = self.get_max_height(*images)
    for i in images:
      tmp_y = y + (max_height - self.get_height(i)) / 2
      # Workaround for th locale (with over-height text) to align properly.
      if self.get_image(i).name == 'th/model':
        tmp_y = y - 9
      self.images.append([int(x), int(tmp_y), self.get_image(i).name])
      x += self.get_width(i) + self.xpad

  def calculate_image_groups_size(self, *image_lists):
    """Calculates the width and height occupied by a list of image groups, with
    each group aligned horizontally. An input like ([A, B], [C], [D, E, F]) will
    be positioned as:   A B
                         C
                       D E F
    Use insert_centered_image_groups() to put the image_lists into screen.

    Args:
      image_lists: A list of image groups.

    Returns:
      (width, height) of the bounding box.
    """
    height = sum((self.get_max_height(*image_list)
                  for image_list in image_lists))
    height += self.ypad * (len(image_lists) - 1)
    width = max((self.total_width(*image_list)
                 for image_list in image_lists))
    return (width, height)

  def insert_centered_image_groups(self, *image_lists):
    """Adds a list of grouped images vertically and centered, with each group
    aligned horizontally. See calculate_image_groups_size for more details.

    Args:
      image_lists: A list of image groups.
    """
    _, height = self.calculate_image_groups_size(*image_lists)
    self.move_pos_up(height / 2)
    for image_list in image_lists:
      self.insert_centered_below(*image_list)

  def move_top_aligned_with_groups(self, *image_lists):
    """Move insert position to top-aligned with a list of grouped images.
    Use insert_centered_below after this to add new images.

    Args:
      image_lists: A list of image groups that will be inserted by
      insert_centered_image_groups later.
    """
    _, height = self.calculate_image_groups_size(*image_lists)
    self.move_pos_up(height / 2)

  def insert_centered_vertical(self, *images):
    """Adds multiple images vertically by using current insert location as
    center point."""
    image_lists = [[image] for image in images]
    self.insert_centered_image_groups(*image_lists)

  def add_header(self, do_locale=True):
    """Adds a standard header (with logo, divider bar, and locale indicators).

    Args:
      do_locale: True to show locale indicators.
    """
    self.set_centered_y_percent(15)
    self.add_centered_below("divider_top")
    self.move_pos_left(self.get_width("divider_top") / 2 )
    self.move_pos_up(self.ypad)
    self.add_right_above("chrome_logo")
    self.move_pos_right(self.get_width("divider_top"))
    if do_locale:
      self.add_left_above("arrow_left", "@language", "arrow_right")

  def add_footer(self, do_url=False):
    """Adds a standard footer (with divider bar, HWID, and recovery URL).

    Args:
      do_url: True to show recovery URL, otherwise no URL.
    """
    self.set_centered_y_percent(80)
    self.insert_centered_below("divider_btm")
    if do_url:
      # Temporarily change padding to zero because both help_*_text and url
      # have margins.
      # TODO(hungte) Prevent changing self.xpad.
      old_xpad = self.xpad
      self.xpad = 0
      self.insert_centered_below("@for_help_left", "Url", "@for_help_right")
      self.xpad = old_xpad
    else:
      # For some locales like th, we need to prevent text overlapping divider.
      # Doubling with ypad seems like a good idea.
      self.move_pos_down(self.ypad)

    self.insert_centered_below("@model_left", 'hwid_placeholder', "@model_right")

# --- Screen Definitions --------------------------------------------------

def NewScreen(f):
  """Decorator to register a screen generation function.

  Args:
    f: A function that returns a Screen object.
  """
  SCREEN_LIST.append(f)
  return f


def OptionalScreen(f):
  """Decorator to register a generation function for optional screen.

  Args:
    f: A function that returns a Screen object.
  """
  OPTIONAL_SCREENS[f.__name__] = f
  return f


@NewScreen
def ScreenDev(locale, image_database):
  s = Screen('devel', locale, image_database)
  s.add_header()
  s.set_centered_y_percent(50)
  # This screen must be top-aligned with TONORM screen.
  s.move_top_aligned_with_groups(*TONORM_IMAGE_GROUP)
  s.insert_centered_below("VerificationOff")
  s.insert_centered_below("@verif_off")
  s.insert_centered_below("@devmode")
  s.add_footer(do_url=True)
  return s


@NewScreen
def ScreenOsBroken(locale, image_database):
  s = Screen('os_broken', locale, image_database)
  s.insert_centered_vertical("@os_broken")
  return s


@NewScreen
def ScreenYuck(locale, image_database):
  s = Screen('yuck', locale, image_database)
  s.add_header()
  s.set_centered_y_percent(50)
  s.insert_centered_image_groups(["@yuck"], ["BadSD", "BadUSB"])
  s.add_footer(do_url=True)
  return s


@NewScreen
def ScreenInsert(locale, image_database):
  s = Screen('insert', locale, image_database)
  s.add_header()
  s.set_centered_y_percent(50)
  s.insert_centered_vertical("Warning", "@insert")
  s.add_footer(do_url=True)
  return s


@NewScreen
def ScreenToDeveloper(locale, image_database):
  s = Screen('todev', locale, image_database)
  s.add_header()
  s.set_centered_y_percent(50)
  s.add_centered("@todev")
  s.add_footer()
  return s


@NewScreen
def ScreenToNormal(locale, image_database):
  s = Screen('tonorm', locale, image_database)
  s.add_header()
  s.set_centered_y_percent(50)
  s.insert_centered_image_groups(*TONORM_IMAGE_GROUP)
  s.add_footer()
  return s


@NewScreen
def ScreenUpdate(locale, image_database):
  # Update (WAIT) Screen
  s = Screen('update', locale, image_database)
  # Currently WAIT screen does not accept any keyboard input, so we don't
  # display language on menubar.
  s.add_header(do_locale=False)
  s.set_centered_y_percent(50)
  s.add_centered("@update")
  s.add_footer()
  return s


@NewScreen
def ScreenToNormalConfirm(locale, image_database):
  s = Screen('tonorm_confirm', locale, image_database)
  s.add_header(do_locale=False)
  s.set_centered_y_percent(50)
  # This screen must be top-aligned with TONORM screen.
  s.move_top_aligned_with_groups(*TONORM_IMAGE_GROUP)
  s.insert_centered_below("VerificationOn")
  s.insert_centered_below("@verif_on")
  s.insert_centered_below("@reboot_erase")
  s.add_footer()
  return s


@OptionalScreen
def ScreenReserveCharging(locale, image_database):
  s = Screen('charging', locale, image_database,
             background='reserve_charging_background')
  s.set_centered_y_percent(40)
  s.add_centered("reserve_charging")
  return s


@OptionalScreen
def ScreenReserveChargingEmpty(locale, image_database):
  s = Screen('charging_empty', locale, image_database,
             background='reserve_charging_background')
  s.set_centered_y_percent(40)
  s.add_centered("reserve_charging_empty")
  return s


@OptionalScreen
def ScreenWrongPowerSupply(locale, image_database):
  s = Screen('wrong_power_supply', locale, image_database)
  s.add_header(do_locale=False)
  s.set_centered_y_percent(50)
  s.insert_centered_vertical("Warning", "@wrong_power_supply")
  s.add_footer()
  return s

# ------------------------------------------------------------------------

def generate_yaml_output(screens, locales, image_db):
  """Generates YAML output file by given screen data and locales.

  Args:
    screens: List of Screen objects to include.
    locales: List of tuple (locale name, screen list) for bitmap block.
    image_db: Image database containing information for all used image files.

  Returns:
    Plain text YAML output, for bmpblk_utility to create bitmap block file.
  """
  data = {
      'bmpblock': 2.0,
      'compression': 2,  # LZMA
      'images': {
          '$HWID':  'hwid_fonts.font',
      },
      'screens': {},
      'localizations': [],
      'locale_index': [],
  }
  for entry in image_db.database.values():
    data['images'][entry.name] = entry.path
  for entry in screens:
    data['screens']['%s_%s' % (entry.locale, entry.name)] = entry.images
  for locale, screen in locales:
    data['locale_index'].append(locale)
    data['localizations'].append(screen)
  return yaml.dump(data)


def main(locale_list, optional_list):
  """Entry point when executed from command line.

  Args:
    locale_list:  List of locale to build manfest file.
    optional_list:  List of optional screen names to include.
  """
  image_db = ImageDatabase()
  screens = []
  locales = []

  screen_list = SCREEN_LIST
  screen_list += [OPTIONAL_SCREENS['Screen' + name] for name in optional_list]
  print "DEFAULT.yaml: ",
  for locale in locale_list:
    print locale,
    new_screens = [f(locale, image_db) for f in SCREEN_LIST]
    locales.append((locale, ['%s_%s' % (screen.locale, screen.name)
                             for screen in new_screens]))
    screens += new_screens

  with open('DEFAULT.yaml', 'w') as f:
    f.write(generate_yaml_output(screens, locales, image_db))


if __name__ == '__main__':
  optional = os.getenv('OPTIONAL_SCREENS', '').split()
  main(sys.argv[1:], optional)
