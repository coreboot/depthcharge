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
import os
import subprocess
import sys
import yaml

def archive_images(archiver, output, name, files):
  """Archive files.

  Args:
    archiver: path to the archive tool
    output: path to the output directory
    name: name of the archive file
    files: list of files to be archived
  """
  archive = os.path.join(output, name)
  paths = [ os.path.join(output, x) for x in files]
  args = ' '.join(paths)
  command = '%s %s create %s' % (archiver, archive, args)
  subprocess.call(command, shell=True)

def archive(archiver, output):
  """Archive language indepdent and depndent images.

  Args:
    archiver: path to the archive tool
    output: path to the output directory
  """
  # Everything comes from DEFAULT.yaml
  default_yaml = os.path.join(output, 'DEFAULT.yaml')
  with open(default_yaml, 'r') as yaml_file:
    config = yaml.load(yaml_file)

  # image section contains list of images used by the board
  config_images = config['images']
  base_images = []
  locale_images = defaultdict(lambda: [])
  for name, path in config_images.iteritems():
    dir = os.path.dirname(path)
    base = os.path.basename(path)
    if not dir:
      # language independent files are placed at root dir
      base_images.append(base)
    else:
      # assume everything else is language dependent files
      lang = os.path.basename(dir)
      locale_images[lang].append(path)

  # create archive for base (language independent) images
  archive_images(archiver, output, 'vbgfx.bin', base_images)

  # create archives for language dependent files
  for lang, images in locale_images.iteritems():
    archive_images(archiver, output, 'locale_%s.bin' % lang, images)

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
