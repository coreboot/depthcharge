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
DIAGNOSTIC_FILES = 'diagnostic_files'

STRINGS_GRD_FILE = 'firmware_strings.grd'
STRINGS_JSON_FILE_TMPL = '{}.json'
FORMAT_FILE = 'format.yaml'
TXT_TO_PNG_SVG = os.path.join(SCRIPT_BASE, 'text_to_png_svg')
LOCALE_DIR = os.path.join(SCRIPT_BASE, 'strings', 'locale')
STAGE_DIR = os.path.join(os.getenv('OUTPUT',
                                   os.path.join(SCRIPT_BASE, 'build')),
                         '.stage')

# Regular expressions used to eliminate spurious spaces and newlines in
# translation strings.
NEWLINE_PATTERN = re.compile(r'([^\n])\n([^\n])')
NEWLINE_REPLACEMENT = r'\1 \2'
CRLF_PATTERN = re.compile(r'\r\n')
MULTIBLANK_PATTERN = re.compile(r'   *')


class DataError(Exception):
  pass


def _load_locale_json_file(locale, json_dir):
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


def parse_locale_json_file(locale, json_dir):
  """Parses given firmware string json file for build_text_files.

  Args:
    locale: The name of the locale, e.g. "da" or "pt-BR".
    json_dir: Directory containing json output from grit.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  result = _load_locale_json_file(locale, json_dir)
  original = _load_locale_json_file('en', json_dir)
  for tag in original:
    if tag not in result:
      # Use original English text, in case translation is not yet available
      print('WARNING: locale "%s", missing entry %s' % (locale, tag))
      result[tag] = original[tag]

  return result


def parse_locale_input_files(locale, json_dir):
  """Parses all firmware string files for the given locale.

  Args:
    locale: The name of the locale, e.g. "da" or "pt-BR".
    json_dir: Directory containing json output from grit.

  Returns:
    A dictionary for mapping of "name to content" for files to be generated.
  """
  result = parse_locale_json_file(locale, json_dir)

  # Walk locale directory to add pre-generated texts such as language names.
  for input_file in glob.glob(os.path.join(LOCALE_DIR, locale, "*.txt")):
    name, _ = os.path.splitext(os.path.basename(input_file))
    with open(input_file, 'r', encoding='utf-8-sig') as f:
      result[name] = f.read().strip()

  return result


def create_file(file_name, contents, output_dir):
  """Creates a text file in output directory by given contents.

  Args:
    file_name: Output file name without extension.
    contents: A list of strings for file content.
    output_dir: The directory to store output file.
  """
  output_name = os.path.join(output_dir, file_name + '.txt')
  with open(output_name, 'w', encoding='utf-8-sig') as f:
    f.write('\n'.join(contents) + '\n')


def build_text_files(inputs, files, output_dir):
  """Builds text files from given input data.

  Args:
    inputs: Dictionary of contents for given file name.
    files: List of file records: [name, content].
    output_dir: Directory to generate text files.
  """
  for file_name, file_content in files.items():
    if file_content is None:
      create_file(file_name, [inputs[file_name]], output_dir)
    else:
      contents = []
      for data in file_content:
        contents.append(inputs[data])
      create_file(file_name, contents, output_dir)


def convert_text_to_png(locale, file_name, styles, fonts, output_dir):
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
    inputs = parse_locale_input_files(locale, json_dir)
    output_dir = os.path.normpath(os.path.join(STAGE_DIR, 'locale', locale))
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    files = formats[KEY_FILES]
    styles = formats[KEY_STYLES]

    # Now parse strings for optional features
    if os.getenv("DIAGNOSTIC_UI") == "1" and DIAGNOSTIC_FILES in formats:
      files.update(formats[DIAGNOSTIC_FILES])

    build_text_files(inputs, files, output_dir)

    results += [pool.apply_async(convert_text_to_png,
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
