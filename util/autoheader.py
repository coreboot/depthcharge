#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Custom autoheader formatting for coreboot.

Coreboot has the following header file requirements not implemented in
Linux kernel Kconfig:

- Booleans with false value are defined to 0, instead of undefined.
- Unreachable integers are defined to 0, instead of undefined.

This script generates the header file with those requirements in mind
(this would normally be done by genconf).
"""

import argparse
import os
import sys

import kconfiglib


def repr_symbol_value(sym):
    """Get the C representation of a symbol's value.

    Args:
        sym: The Kconfig symbol.

    Returns:
        Valid C tokens representing the value.
    """
    val = sym.str_value

    if sym.orig_type is kconfiglib.BOOL:
        return '1' if val == 'y' else '0'
    if sym.orig_type is kconfiglib.STRING:
        return '"{}"'.format(kconfiglib.escape(val))
    if (sym.orig_type is kconfiglib.HEX
            and val and not val.lower().startswith('0x')):
        val = '0x{}'.format(val)

    assert sym.orig_type in (kconfiglib.INT, kconfiglib.HEX)
    return val if val else '0'


def get_header_contents(kconfig):
    return ''.join(
        '#define {}{} {}\n'.format(
            kconfig.config_prefix, sym.name, repr_symbol_value(sym))
        for sym in kconfig.unique_defined_syms)


def main(argv):
    # Arguments are a subset of those supported by genconfig.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header-path", help="Path to output file")
    parser.add_argument("kconfig", nargs="?", default="Kconfig",
                        help="Path to Kconfig")

    args = parser.parse_args(argv)

    kconf = kconfiglib.Kconfig(args.kconfig)
    kconf.load_config()

    header_path = args.header_path
    if header_path is None:
        header_path = os.getenv('KCONFIG_AUTOHEADER', 'config.h')

    # We only write the header contents if changed.
    try:
        with open(header_path) as f:
            contents = f.read()
    except EnvironmentError:
        # This will force a write of the header.
        contents = None

    desired_contents = get_header_contents(kconf)
    if contents != desired_contents:
        with open(header_path, 'w') as f:
            f.write(desired_contents)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
