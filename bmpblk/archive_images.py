#!/usr/bin/env python
# Copyright 2015 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""

Usage:
  ./archive_images.py -a path_to_archiver -d input_output_dir

  input_output_dir should points to the directory where images are created by
  build_images.py. The script outputs archives to input_output_dir.

  path_to_archiver should points to the tool which bundles files into a blob,
  which can be unpacked by Depthcharge.
"""

from collections import defaultdict
import getopt
import glob
import os
import subprocess
import sys


LOCALE_DIR = 'locale'
LOCALE_RO_DIR = os.path.join(LOCALE_DIR, 'ro')
LOCALE_RW_DIR = os.path.join(LOCALE_DIR, 'rw')
GLYPH_DIR = 'glyph'


def archive_images(archiver, output, name, files):
    """Archives files.

    Args:
        archiver: path to the archive tool
        output: path to the output directory
        name: name of the archive file
        files: list of files to be archived
    """
    archive = os.path.join(output, name)
    args = ' '.join(files)
    command = f'{archiver} {archive} create {args}'
    subprocess.check_call(command, shell=True)


def archive_generic(archiver, output):
    """Archives generic (locale-independent) images.

    Args:
        archiver: path to the archive tool
        output: path to the output directory
    """
    images = sorted(glob.glob(os.path.join(output, '*.bmp')))
    archive_images(archiver, output, 'vbgfx.bin', images)


def archive_localized(archiver, output, pattern):
    """Archives localized images.

    Args:
        archiver: path to the archive tool
        output: path to the output directory
        pattern: filename with a '%s' to fill in the locale code
    """
    locale_images = defaultdict(lambda: [])

    for path in sorted(glob.glob(os.path.join(output, '*'))):
        files = sorted(glob.glob(os.path.join(path, '*.bmp')))
        locale = os.path.basename(path)
        for file in files:
            locale_images[locale].append(file)

    # create archives of localized images
    for locale, images in locale_images.items():
        archive_images(archiver, output, pattern % locale, images)


def archive_glyphs(archiver, output):
    """Archives glyph images.

    Args:
        archiver: path to the archive tool
        output: path to the output directory
    """
    glyph_dir = os.path.join(output, GLYPH_DIR)
    images = sorted(glob.glob(os.path.join(glyph_dir, '*.bmp')))
    archive_images(archiver, output, 'font.bin', images)


def main(args):
    """Archives images."""
    opts, args = getopt.getopt(args, 'a:d:')
    archiver = ''
    output = ''

    for opt, arg in opts:
        if opt == '-a':
            archiver = arg
        elif opt == '-d':
            output = arg
        else:
            assert False, 'Invalid option'
    if args or not archiver or not output:
        assert False, 'Invalid usage'

    print('Archiving vbfgx.bin', file=sys.stderr, flush=True)
    archive_generic(archiver, output)
    print('Archiving locales for RO', file=sys.stderr, flush=True)
    ro_locale_dir = os.path.join(output, LOCALE_RO_DIR)
    rw_locale_dir = os.path.join(output, LOCALE_RW_DIR)
    archive_localized(archiver, ro_locale_dir, 'locale_%s.bin')
    if os.path.exists(rw_locale_dir):
        print('Archiving locales for RW', file=sys.stderr, flush=True)
        archive_localized(archiver, rw_locale_dir, 'rw_locale_%s.bin')
    print('Archiving font.bin', file=sys.stderr, flush=True)
    archive_glyphs(archiver, output)


if __name__ == '__main__':
    main(sys.argv[1:])
