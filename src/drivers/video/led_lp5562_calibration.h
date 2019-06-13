// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LED_LP5562_CALIBRATION_H_
#define LED_LP5562_CALIBRATION_H_

/* Size of LP5562 program memry, as in 16-bit word steps */
#define NPROGSIZE	16

/* Number of consecutive ramp instructions */
#define NMAXRAMPS	2

/* LP5562 color component number, associated with engine number */
enum lp5562_components {
	blue = 0,
	green,
	red
};

/* LP5562 code type for calibration (PWM value replacement) */
enum lp5562_code_types {
	invalid = 0,
	set_pwm,
	ramp
};

/*
 * Structure to indicate where / how to replace PWM value
 * in a LP5562 program code.
 */
struct lp5562_calibration_code_map {
	enum lp5562_components component;
	enum lp5562_code_types code_type;
	/* Code offset, as in 16bit/word binary */
	uint8_t code_offset;
	/* Number of consecutive ramp instructions */
	uint8_t ramp_num;
	struct {
		/* RAMP duration, in msec */
		uint16_t ramp_duration;
		/* PWM division, -128 to +128 */
		int16_t ramp_division;
	} ramp_instructions[NMAXRAMPS];
};

/*
 * Structure to store calibration data for a pattern.
 */
struct lp5562_calibration_data {
	TiLp5562Program *pattern;
	/* Index to calibrated color table */
	int color_index;
	/* 0-100 % */
	int relative_brightness;
	const struct lp5562_calibration_code_map *code_map;
};

/*
 * Structure to store calibration data.
 */
struct lp5562_calibrated_color {
	uint8_t pwm_values[3];
	uint8_t current_values[3];
};

int led_lp5562_calibrate(const struct lp5562_calibration_data *cal_data,
			 struct lp5562_calibrated_color *calibrated_colors);


extern const struct lp5562_calibration_data mistral_calibration_database[];

#endif  // LED_LP5562_CALIBRATION_H_

