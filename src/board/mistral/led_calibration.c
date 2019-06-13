/*
 * Copyright 2019 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <string.h>
#include <base/vpd_util.h>
#include "drivers/video/led_lp5562_programs.h"
#include "drivers/video/led_lp5562_calibration.h"
#include "led_calibration.h"

/* Number of predefined colors */
#define NDEFCOLORS 3

const char *color_names[NDEFCOLORS] = {
	"white",
	"yellow",
	"red"
};

struct lp5562_calibrated_color calibrated_colors[NDEFCOLORS] = {
	{ {255, 255, 255}, {120, 120, 120} },
	{ {255, 204,   0}, {120, 120, 120} },
	{ {255,   0,   0}, {120, 120, 120} },
};

static const char key_name[] = "led_calibration";

int calibrate_led(void)
{
	const char *separators = " ,;";
	char buff[256];
	char *p = buff;
	int idx;

	if (!vpd_gets(key_name, buff, sizeof(buff))) {
		printf("LED_LP5562: Valid caldata not found\n");
		return 0;
	}

	/*
	 * Parse calibration data
	 * Calibration data is list of entries separated by semicolon(;)
	 * An entry is list of values separated by commna(,)
	 * first value is ASCII string color name,
	 * second value is ASCII decimal string of current,
	 * 3rd, 4th, 5th values are brightness of Red, Green and Blue.
	 */
	while (*p != 0) {
		int color;
		int rgb;
		int len;

		len = strcspn(p, separators);
		if (len < 1)
			break;
		for (color = 0; color < NDEFCOLORS; color++) {
			if (strncmp(color_names[color], p, len) == 0)
				break;
		}
		if (color >= NDEFCOLORS) {
			len = strcspn(p, ";");
			if (len < 0)
				break;
			p += (len + 1);
			continue;
		}

		p += len;
		p += strspn(p, separators);
		calibrated_colors[color].current_values[0] =
		calibrated_colors[color].current_values[1] =
		calibrated_colors[color].current_values[2] = atol(p);

		p += strcspn(p, separators);
		p += strspn(p, separators);
		for (rgb = 0; rgb < 3; rgb++) {
			calibrated_colors[color].pwm_values[rgb] = atol(p);
			p += strcspn(p, separators);
			p += strspn(p, separators);
		}
	}

	/*
	 * Apply calibration data
	 */
	idx = 0;
	while (mistral_calibration_database[idx].pattern != NULL) {
		led_lp5562_calibrate(
			&mistral_calibration_database[idx],
			calibrated_colors);
		idx++;
	}
	return 0;
}

