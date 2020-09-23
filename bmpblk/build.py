#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Build localized text resources by extracting firmware localization strings
   and convert into TXT and PNG files into stage folder.

Usage:
   ./build.py <locale-list>
"""

import signal
import enum
import glob
import json
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image
import yaml

SCRIPT_BASE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_NAME = '_DEFAULT_'
KEY_LOCALES = 'locales'
KEY_FILES = 'files'
KEY_FONTS = 'fonts'
KEY_STYLES = 'styles'
VENDOR_INPUTS = 'vendor_inputs'
VENDOR_FILES = 'vendor_files'
DIAGNOSTIC_FILES = 'diagnostic_files'

STRINGS_GRD_FILE = 'firmware_strings.grd'
STRINGS_JSON_FILE_TMPL = '{}.json'
VENDOR_STRINGS_FILE = 'vendor_strings.txt'
FORMAT_FILE = 'format.yaml'
VENDOR_FORMAT_FILE = 'vendor_format.yaml'
TXT_TO_PNG_SVG = os.path.join(SCRIPT_BASE, 'text_to_png_svg')
LOCALE_DIR = os.path.join(SCRIPT_BASE, 'strings', 'locale')
OUTPUT_DIR = os.path.join(os.getenv('OUTPUT', os.path.join(SCRIPT_BASE,
                                                           'build')),
                          '.stage', 'locale')

VENDOR_STRINGS_DIR = os.getenv("VENDOR_STRINGS_DIR")
VENDOR_STRINGS = VENDOR_STRINGS_DIR != None

# Regular expressions used to eliminate spurious spaces and newlines in
# translation strings.
NEWLINE_PATTERN = re.compile(r'([^\n])\n([^\n])')
NEWLINE_REPLACEMENT = r'\1 \2'
CRLF_PATTERN = re.compile(r'\r\n')
MULTIBLANK_PATTERN = re.compile(r'   *')


class DataError(Exception):
  pass


def GetImageWidth(filename):
  """Returns the width of given image file."""
  return Image.open(filename).size[0]

def ParseLocaleInputFile(locale_dir, strings_file, input_format):
  """Parses firmware string file in given locale directory for BuildTextFiles

  Args:
    locale: The locale folder with firmware string files.
    strings_file: The name of the string txt file
    input_format: Format description for each line in strings_file.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  input_file = os.path.join(locale_dir, strings_file)
  with open(input_file, 'r', encoding='utf-8-sig') as f:
    input_data = f.readlines()
  if len(input_data) != len(input_format):
    raise DataError('Input file <%s> for locale <%s> '
                    'does not match input format.' %
                    (strings_file, locale_dir))
  input_data = [s.strip() for s in input_data]
  return dict(zip(input_format, input_data))

def ParseLocaleInputJsonFile(locale, strings_json_file_tmpl, json_dir):
  """Parses given firmware string json file for BuildTextFiles

  Args:
    locale: The name of the locale, e.g. "da" or "pt-BR".
    strings_json_file_tmpl: The template for the json input file name.
    json_dir: Directory containing json output from grit.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  result = LoadLocaleJsonFile(locale, strings_json_file_tmpl, json_dir)
  original = LoadLocaleJsonFile("en", strings_json_file_tmpl, json_dir)
  for tag in original:
    if not tag in result:
      # Use original English text, in case translation is not yet available
      print('WARNING: locale "%s", missing entry %s' % (locale, tag))
      result[tag] = original[tag]

  return result

def LoadLocaleJsonFile(locale, strings_json_file_tmpl, json_dir):
  result = {}
  filename = os.path.join(json_dir, strings_json_file_tmpl.format(locale))
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

def ParseLocaleInputFiles(locale, vendor_format, json_dir):
  """Parses all firmware string files in given locale directory for
  BuildTextFiles

  Args:
    locale: The name of the locale, e.g. "da" or "pt-BR".
    vendor_format: Format description for each line in VENDOR_STRINGS_FILE.
    json_dir: Directory containing json output from grit.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  result = dict()
  result.update(ParseLocaleInputJsonFile(locale,
                                         STRINGS_JSON_FILE_TMPL,
                                         json_dir))

  # Parse vendor files if enabled
  if VENDOR_STRINGS:
    print(' (vendor specific strings)')
    result.update(
      ParseLocaleInputFile(os.path.join(VENDOR_STRINGS_DIR, locale),
                                        VENDOR_STRINGS_FILE,
                                        vendor_format))

  # Walk locale directory to add pre-generated items.
  for input_file in glob.glob(os.path.join(LOCALE_DIR, locale, "*.txt")):
    if os.path.basename(input_file) == VENDOR_STRINGS_FILE:
      continue
    name, _ = os.path.splitext(os.path.basename(input_file))
    with open(input_file, 'r', encoding='utf-8-sig') as f:
      result[name] = f.read().strip()

  return result


def CreateFile(file_name, contents, output_dir):
  """Creates a text file in output directory by given contents.

  Args:
    file_name: Output file name without extension.
    contents: A list of strings for file content.
    output_dir: The directory to store output file.
  """
  output_name = os.path.join(output_dir, file_name + '.txt')
  with open(output_name, 'w', encoding='utf-8-sig') as f:
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
  for file_name, file_content in files.items():
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
    fonts: Dictionary to get associated per-file font options. The value at
      DEFAULT_NAME is used when |locale| is not in the dict, and the '--font'
      option is omitted when neither exist.
    output_dir: Directory to generate image files.
  """
  input_file = os.path.join(output_dir, file_name + '.txt')
  command = [TXT_TO_PNG_SVG, "--lan=%s" % locale, "--outdir=%s" % output_dir]
  if file_name in styles:
    command.append(styles[file_name])
  default_font = fonts.get(DEFAULT_NAME)
  font = fonts.get(locale, default_font)
  if font:
    command.append("--font='%s'" % font)
  font_size = os.getenv("FONTSIZE")
  if font_size is not None:
    command.append('--point=%r' % font_size)
  command.append('--margin="0 0"')
  # TODO(b/159399377): Set different widths for titles and descriptions.
  # Currently only wrap lines for descriptions.
  if '_desc' in file_name:
    # Without the --width option set, the minimum height of the output SVG
    # image is roughly 22px (for locale 'en'). With --width=WIDTH passed to
    # pango-view, the width of the output seems to always be (WIDTH * 4 / 3),
    # regardless of the font being used. Therefore, set the max_width in
    # points as follows to prevent drawing from exceeding canvas boundary in
    # depthcharge runtime.
    # Some of the numbers below are from depthcharge:
    # - 1000: UI_SCALE
    # - 50: UI_MARGIN_H
    # - 228: UI_REC_QR_SIZE
    # - 24: UI_REC_QR_MARGIN_H
    # - 24: UI_DESC_TEXT_HEIGHT
    if file_name == 'rec_phone_step2_desc':
      max_width = 1000 - 50 * 2 - 228 - 24 * 2
    else:
      max_width = 1000 - 50 * 2
    max_width_pt = int(22 * max_width / 24 / (4 / 3))
    command.append('--width=%d' % max_width_pt)
  command.append(input_file)

  if subprocess.call(' '.join(command), shell=True,
                     stdout=subprocess.PIPE) != 0:
    return False

  # Check output file size
  output_file = os.path.join(output_dir, file_name + '.png')

  return True

def main(argv):
  with open(FORMAT_FILE, encoding='utf-8') as f:
    formats = yaml.load(f)

  if VENDOR_STRINGS:
    with open(os.path.join(VENDOR_STRINGS_DIR, VENDOR_FORMAT_FILE),
              encoding='utf-8') as f:
      formats.update(yaml.load(f))

  json_dir = None
  # Sources are one .grd file with identifiers chosen by engineers and
  # corresponding English texts, as well as a set of .xlt files (one for each
  # language other than US english) with a mapping from hash to translation.
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
      '-i', os.path.join(LOCALE_DIR, STRINGS_GRD_FILE),
      'build',
      '-o', os.path.join(json_dir)
  ])

  # Decide locales to build.
  if len(argv) > 0:
    locales = argv
  else:
    locales = os.getenv('LOCALES', '').split()
  if not locales:
    locales = formats[KEY_LOCALES]

  # Ignore SIGINT in child processes
  sigint_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
  pool = multiprocessing.Pool(multiprocessing.cpu_count())
  signal.signal(signal.SIGINT, sigint_handler)

  results = []
  for locale in locales:
    print(locale, end=' ', flush=True)
    inputs = ParseLocaleInputFiles(locale,
                                   formats[VENDOR_INPUTS] if VENDOR_STRINGS
                                                          else None,
                                   json_dir)
    output_dir = os.path.normpath(os.path.join(OUTPUT_DIR, locale))
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    files = formats[KEY_FILES]
    styles = formats[KEY_STYLES]

    # Now parse strings for optional features
    if os.getenv("DIAGNOSTIC_UI") == "1" and DIAGNOSTIC_FILES in formats:
      files.update(formats[DIAGNOSTIC_FILES])

    if VENDOR_STRINGS:
      files.update(formats[VENDOR_FILES])
    BuildTextFiles(inputs, files, output_dir)

    results += [pool.apply_async(ConvertPngFile,
                                 (locale, file_name,
                                  styles, formats[KEY_FONTS],
                                  output_dir))
                for file_name in formats[KEY_FILES]]
  pool.close()
  if json_dir is not None:
    shutil.rmtree(json_dir)
  print()

  try:
    success = [r.get() for r in results]
  except KeyboardInterrupt:
    pool.terminate()
    pool.join()
    exit('Aborted by user')
  else:
    pool.join()
    if not all(success):
      exit('Failed to render some locales')


if __name__ == '__main__':
  main(sys.argv[1:])
