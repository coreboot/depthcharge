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

import glob
import multiprocessing
import os
import re
import subprocess
import sys

from PIL import Image
import yaml

SCRIPT_BASE = os.path.dirname(os.path.abspath(__file__))
KEY_LOCALES = 'locales'
KEY_INPUTS = 'inputs'
KEY_FILES = 'files'
KEY_FONTS = 'fonts'
KEY_STYLES = 'styles'
DETACHABLE_INPUTS = 'detachable_inputs'
DETACHABLE_FILES = 'detachable_files'
KEYBOARD_FILES = 'keyboard_files'

FIRMWARE_STRINGS_FILE = 'firmware_strings.txt'
DETACHABLE_STRINGS_FILE = 'detachable_strings.txt'
FORMAT_FILE = 'format.yaml'
TXT_TO_PNG_SVG = os.path.join(SCRIPT_BASE, '..', 'text_to_png_svg')
OUTPUT_DIR = os.path.join(os.getenv('OUTPUT', os.path.join(SCRIPT_BASE, '..',
                                                           '..', 'build')),
                          '.stage', 'locale')


class DataError(Exception):
  pass


def GetImageWidth(filename):
  """Returns the width of given image file."""
  return Image.open(filename).size[0]


def ParseLocaleInputFile(locale_dir, input_format, detachable_format):
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
    if (os.path.basename(input_file) == FIRMWARE_STRINGS_FILE or
        os.path.basename(input_file) == DETACHABLE_STRINGS_FILE):
      continue
    name, _ = os.path.splitext(os.path.basename(input_file))
    with open(input_file, "r") as f:
      result[name] = f.read().strip()

  # Now parse detachable menu strings
  if os.getenv("DETACHABLE_UI") == "1":
    print " (detachable_ui enabled)"
    detach_input_file = os.path.join(locale_dir, DETACHABLE_STRINGS_FILE)
    with open(detach_input_file, 'r') as df:
      detach_input_data = df.readlines()
    if len(detach_input_data) != len(detachable_format):
      raise DataError('Input file for locale <%s> does not match input format.'
                      % locale_dir)
    detach_input_data = [s.strip() for s in detach_input_data]
    result.update(dict(zip(detachable_format, detach_input_data)))

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


def ConvertPngFile(locale, file_name, styles, fonts, output_dir):
  """Converts text files into PNG image files.

  Args:
    locale: Locale (language) to select implicit rendering options.
    file_name: String of input file name to generate.
    styles: Dictionary to get associated per-file style options.
    fonts: Dictionary to get associated per-file font options.
    output_dir: Directory to generate image files.
  """
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

  if subprocess.call(' '.join(command), shell=True,
                     stdout=subprocess.PIPE) != 0:
    return False

  # Check output file size
  output_file = os.path.join(output_dir, file_name + '.png')

  return True

def main(argv):
  with open(FORMAT_FILE) as f:
    formats = yaml.load(f)

  # Decide locales to build.
  if len(argv) > 0:
    locales = argv
  else:
    locales = os.getenv('LOCALES', '').split()
  if not locales:
    locales = formats[KEY_LOCALES]

  pool = multiprocessing.Pool(multiprocessing.cpu_count())
  results = []
  for locale in locales:
    print locale,
    inputs = ParseLocaleInputFile(locale, formats[KEY_INPUTS],
                                  formats[DETACHABLE_INPUTS])
    output_dir = os.path.normpath(os.path.join(OUTPUT_DIR, locale))
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    files = formats[KEY_FILES]
    styles = formats[KEY_STYLES]
    if os.getenv("DETACHABLE_UI") == "1":
      files.update(formats[DETACHABLE_FILES])
    else:
      files.update(formats[KEYBOARD_FILES])
    BuildTextFiles(inputs, files, output_dir)

    results += [pool.apply_async(ConvertPngFile,
                                 (locale, file_name,
                                  styles, formats[KEY_FONTS],
                                  output_dir))
                for file_name in formats[KEY_FILES]]
  print ""
  pool.close()
  pool.join()
  if not all((r.get() for r in results)):
    exit("Failed to render some locales.")


if __name__ == '__main__':
  main(sys.argv[1:])
