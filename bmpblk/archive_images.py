#!/usr/bin/env python
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Usage:
  ./archive_images.py -a path_to_archiver -d input_output_dir

  input_output_dir should points to the directory where images are created by
  build_images.py. The script outputs archives to input_output_dir.

  path_to_archiver should points to the tool which bundles files into a blob,
  which can be unpacked by Depthcharge.
'''

from collections import defaultdict
import getopt
import glob
import os
import subprocess
import sys

def archive_images(archiver, output, name, files):
  """Archive files.

  Args:
    archiver: path to the archive tool
    output: path to the output directory
    name: name of the archive file
    files: list of files to be archived
  """
  archive = os.path.join(output, name)
  args = ' '.join(files)
  command = '%s %s create %s' % (archiver, archive, args)
  subprocess.check_call(command, shell=True)

def archive(archiver, output):
  """Archive base images and localized images

  Args:
    archiver: path to the archive tool
    output: path to the output directory
  """
  base_images = glob.glob(os.path.join(output, "*.bmp"))

  # create archive of base images
  archive_images(archiver, output, 'vbgfx.bin', base_images)

  locale_images = defaultdict(lambda: [])
  dirs = glob.glob(os.path.join(output, "locale/*"))
  for dir in dirs:
    files = glob.glob(os.path.join(dir, "*.bmp"))
    locale = os.path.basename(dir)
    for file in files:
      locale_images[locale].append(file)

  # create archives of localized images
  for locale, images in locale_images.items():
    archive_images(archiver, output, 'locale_%s.bin' % locale, images)

def main(args):
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

  archive(archiver, output)

if __name__ == '__main__':
  main(sys.argv[1:])
