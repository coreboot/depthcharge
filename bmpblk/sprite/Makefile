# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This makefile lets you MANUALLY generate the QR code images, and should NOT be
# run automatically.

all: qr_rec.svg

# Web UI: http://goqr.me/
API_URL = https://api.qrserver.com/v1/create-qr-code/

# The colors are adjusted at runtime, and the quiet zone is drawn at runtime as
# the padding around the QR code.
# qzone: Border width in terms of number of squares
PARAMS = color=ffffff&bgcolor=000000&qzone=0&format=svg

qr_rec.svg:
	curl "$(API_URL)?$(PARAMS)&data=https://goo.gle/mydevice_recovery" -o $@
