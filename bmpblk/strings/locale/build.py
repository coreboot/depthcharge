#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build localized text resources by extracting firmware localization strings
   and convert into TXT and PNG files into stage folder.

Usage:
   ./build.py <locale-list>
"""

# TODO(hungte) Read, write and handle UTF8 BOM.

import Image
import glob
import os
import re
import subprocess
import sys
import yaml

SCRIPT_BASE = os.path.dirname(os.path.abspath(__file__))
KEY_LOCALES = 'locales'
KEY_INPUTS = 'inputs'
KEY_FILES = 'files'
KEY_FONTS = 'fonts'
KEY_STYLES = 'styles'

FIRMWARE_STRINGS_FILE = 'firmware_strings.txt'
FORMAT_FILE = 'format.yaml'
TXT_TO_PNG_SVG = os.path.join(SCRIPT_BASE, '..', 'text_to_png_svg')
BACKGROUND_IMAGE = os.path.join(SCRIPT_BASE, '..', '..', 'images',
                                'Background.png')
OUTPUT_DIR = os.path.join(SCRIPT_BASE, '..', '..', 'build', '.stage', 'locale')


class DataError(Exception):
  pass


def GetImageWidth(filename):
  """Returns the width of given image file."""
  return Image.open(filename).size[0]


def ParseLocaleInputFile(locale_dir, input_format):
  """Parses a FIRMWARE_STRINGS_FILE in given locale directory for BuildTextFiles

  Args:
    locale: The locale folder with FIRMWARE_STRINGS_FILE.
    input_format: Format description for each line in FIRMWARE_STRINGS_FILE.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  input_file = os.path.join(locale_dir, FIRMWARE_STRINGS_FILE)
  with open(input_file, 'r') as f:
    input_data = f.readlines()
  if len(input_data) != len(input_format):
    raise DataError('Input file for locale <%s> does not match input format.' %
                    locale_dir)
  input_data = [s.strip() for s in input_data]
  result = dict(zip(input_format, input_data))

  # Walk locale directory to add pre-generated items.
  for input_file in glob.glob(os.path.join(locale_dir, "*.txt")):
    if os.path.basename(input_file) == FIRMWARE_STRINGS_FILE:
      continue
    name, _ = os.path.splitext(os.path.basename(input_file))
    with open(input_file, "r") as f:
      result[name] = f.read()
  return result


def CreateFile(file_name, contents, output_dir):
  """Creates a text file in output directory by given contents.

  Args:
    file_name: Output file name without extension.
    contents: A list of strings for file content.
    output_dir: The directory to store output file.
  """
  output_name = os.path.join(output_dir, file_name + '.txt')
  with open(output_name, 'w') as f:
    f.write('\n'.join(contents) + '\n')


def ModifyContent(input_data, command):
  """Modifies some input content with given Regex commands.

  Args:
    input_data: Input string to be modified.
    command: Regex commands to execute.

  Returns:
    Processed output string.
  """
  if not command.startswith('s/'):
    raise DataError('Unknown command: %s' % command)
  _, pattern, repl, _ = command.split('/')
  return re.sub(pattern, repl, input_data)


def BuildTextFiles(inputs, files, output_dir):
  """Builds text files from given input data.

  Args:
    inputs: Dictionary of contents for given file name.
    files: List of file records: [name, content].
    output_dir: Directory to generate text files.
  """
  for file_name, file_content in files.iteritems():
    if file_content is None:
      CreateFile(file_name, [inputs[file_name]], output_dir)
    else:
      contents = []
      for data in file_content:
        if '@' in data:
          name, _, command = data.partition('@')
          contents.append(ModifyContent(inputs[name], command))
        else:
          contents.append('' if data == '' else inputs[data])
      CreateFile(file_name, contents, output_dir)


def ConvertPngFiles(locale, max_width, files, styles, fonts, output_dir):
  """Converts text files into PNG image files.

  Args:
    locale: Locale (language) to select implicit rendering options.
    max_width: Maximum allowed image width.
    files: List of file names to generate.
    styles: Dictionary to get associated per-file style options.
    fonts: Dictionary to get associated per-file font options.
    output_dir: Directory to generate image files.
  """
  for file_name in files:
    input_file = os.path.join(output_dir, file_name + '.txt')
    command = [TXT_TO_PNG_SVG, "--lan=%s" % locale, "--outdir=%s" % output_dir]
    if file_name in styles:
      command.append(styles[file_name])
    if locale in fonts:
      command.append("--font='%s'" % fonts[locale])
    font_size = os.getenv("FONTSIZE")
    if font_size is not None:
      command.append('--point=%r' % font_size)
    command.append(input_file)

    # print command
    subprocess.check_call(' '.join(command), shell=True)
    # Check output file size
    output_file = os.path.join(output_dir, file_name + '.png')
    if max_width < GetImageWidth(output_file):
      raise DataError("Error: message too long: %s/%s" % (locale, file_name))


def main(argv):
  with open(FORMAT_FILE) as f:
    formats = yaml.load(f)
  max_width = GetImageWidth(BACKGROUND_IMAGE) * (4 / 5.0)

  # Decide locales to build.
  if len(argv) > 0:
    locales = argv
  else:
    locales = os.getenv('LOCALES', '').split()
  if not locales:
    locales = formats[KEY_LOCALES]

  for locale in locales:
    print locale,
    inputs = ParseLocaleInputFile(locale, formats[KEY_INPUTS])
    output_dir = os.path.normpath(os.path.join(OUTPUT_DIR, locale))
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    BuildTextFiles(inputs, formats[KEY_FILES], output_dir)
    ConvertPngFiles(locale, max_width, formats[KEY_FILES], formats[KEY_STYLES],
                    formats[KEY_FONTS], output_dir)


if __name__ == '__main__':
  main(sys.argv[1:])
