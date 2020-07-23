#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

"""Convert raw image file to BMP format."""
import argparse

from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('width', type=int, help='Image width')
    parser.add_argument('height', type=int, help='Image height')
    parser.add_argument('input', help='Input raw RGB file (3x8-bit pixels)')
    parser.add_argument('output', help='Output BMP file')
    args = parser.parse_args()

    with open(args.input, 'rb') as f:
        content = f.read()
    with Image.frombytes('RGB', (args.width, args.height), content) as image:
        image.save(args.output, format='BMP')


if __name__ == '__main__':
    main()
