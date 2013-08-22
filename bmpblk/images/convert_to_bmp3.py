#!/usr/bin/env python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import getopt
import os
import re
import sys

import Image


BACKGROUND_COLOR = (255, 255, 255)
DEFAULT_OUTPUT_EXT = '.bmp'


def parse_scale_factor(pattern, original_size):
  # Format: w%xh%, or wxh!
  factor = re.findall(r'^([0-9]+)%x([0-9]+)%$', pattern)
  if factor:
    w, h = factor[0]
    return (int(int(w) / 100.0 * original_size[0]),
            int(int(h) / 100.0 * original_size[1]))

  factor = re.findall(r'^([0-9]+)x([0-9]+)!?$', pattern)
  if factor:
    return map(int, factor[0])

  raise Exception('Unknown scaling parameter: %s', pattern)


def convert_to_bmp(input_file, scale, background=BACKGROUND_COLOR,
                   output_file=None):
  """Converts any image input files into BMP format.

  Args:
    input_file: An image file.
    scale: Scale parameters to apply, in ImageMagick syntax.
    background: Background color to use when the input file has transparency.
    output_file: File name to output, or None to output by the name of
                 input_file with extension ".bmp".
  Returns:
    A new image created in outdir. Returns None.
  """
  source = Image.open(input_file)
  if output_file is None:
    output_file = os.path.splitext(input_file)[0] + DEFAULT_OUTPUT_EXT

  # Process alpha channel and transparency.
  if source.mode == 'RGBA':
    target = Image.new('RGB', source.size, background)
    source.load()  # required for source.split()
    mask = source.split()[-1]
    target.paste(source, mask=mask)
  elif (source.mode == 'P') and ('transparency' in source.info):
    exit('Sorry, PNG with RGBA palette is not supported.')
  elif source.mode != 'RGB':
    target = source.convert('RGB')
  else:
    target = source

  # Process scaling
  if scale:
    new_size = parse_scale_factor(scale, source.size)
    target = target.resize(new_size, Image.BICUBIC)

  # Export and downsample color space.
  target.convert('P', dither=None, palette=Image.ADAPTIVE).save(output_file)


def parse_color_param(param):
  """Parses a color parameter in format rrggbb into color tuple."""
  if len(param) != 6:
    exit("Sorry, color param '%s' is not supported." % param)
  return (int(param[0:2], 16),
          int(param[2:4], 16),
          int(param[4:6], 16))

def main(argv):
  scale_param = ''
  outdir = ''
  background = BACKGROUND_COLOR
  try:
    opts, args = getopt.getopt(argv[1:], '',
                               ('scale=', 'outdir=', 'background='))
    for key, value in opts:
      if key == '--scale':
        scale_param = value
      elif key == '--outdir':
        outdir = value
      elif key == '--background':
        background = parse_color_param(value)
    if len(args) < 1:
      raise Exception('need more param')
  except:
    exit('Usage: ' + argv[0] + '[--scale WxH! | --scale W%xH%] [--outdir DIR] '
         '[--background rrggbb] files(s)...')

  if outdir and not os.path.isdir(outdir):
    os.makedirs(outdir)

  for source_file in args:
    source_name, _ = os.path.splitext(os.path.basename(source_file))
    output_file = os.path.join(outdir, source_name) + DEFAULT_OUTPUT_EXT
    convert_to_bmp(source_file, scale_param, background, output_file)


if __name__ == '__main__':
  main(sys.argv)
