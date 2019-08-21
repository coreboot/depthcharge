// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include "led_lp5562_programs.h"
#include "led_lp5562_calibration.h"

static int calculate_step_time(int duration, int increment)
{
/*
 * On LP5562 ramp instruction, duration (D, msec) is
 * D = T * step_time * increment * 1000
 *     where T = 0.49msec(1/2048Hz) or 15.6msec(1/64Hz)
 * Thus,
 * step_time = D/(increment*T)/1000
 * = (D*F)/increment/1000
 * Use F=2048 for short duration, F=64 for long duration.
 */
	int step_time = ((duration * 2048) / increment + 500) / 1000;
	if (step_time > 63) {
		step_time = ((duration * 64) / increment + 500) / 1000;
		if (step_time > 63)
			return -1; /* Overflow */
		step_time |= 0x40;
	}
	return step_time;
}

static void calibrate_set_pwm_instruction(
	const struct lp5562_calibration_code_map *code_map,
	uint8_t *code,
	int offset,
	int intensity)
{
	/*
	 * Calibrate set_pwm instruction:
	 * Just to replace PWM value in the code.
	 */
	code[offset+0] = 0x40;
	code[offset+1] = intensity;
}

static void calibrate_ramp_instruction(
	const struct lp5562_calibration_code_map *code_map,
	uint8_t *code,
	int offset,
	int intensity)
{
	int intensity_reminder = intensity;

	/*
	 * Calibrate ramp instruction:
	 * We have to re-calculate PWM increment and step time in the code.
	 * Because LP5562 only can handle PWM increment in 7-bit range
	 * (+2 to +128) per instruction, we usually use 2 consecutive
	 * ramp instructions to implement full 8-bit PWM swing.
	 * In that case we have to replace multiple PWM values /
	 * and step times.
	 */
	for (int i = 0; i < code_map->ramp_num; i++) {
		int ramp_sign = 0;
		int increment =
			code_map->ramp_instructions[i].ramp_division;
		int duration =
			code_map->ramp_instructions[i].ramp_duration;

		/*
		 * Calculate PWM increment for this instruction.
		 */
		if (increment < 0) {
			increment = -increment;
			ramp_sign = 0x80;
		}

		/*
		 * To handle division remainder, we track remaining
		 * intensity and plase the leftover at the last of
		 * consecutive ramp instructions.
		 * (if there is only one ramp instruction, we always
		 * use the intensity as PWM increment).
		 */
		if ((i == (code_map->ramp_num - 1))) {
			increment = intensity_reminder;
		} else {
			increment = ((intensity * increment) / 256);
			intensity_reminder -= increment;
		}

		/*
		 * If there is no need to ramp PWM value,
		 * substitute the code with "set_pwm 0" instruction
		 * (0x4000) as placeholder.
		 */
		if (increment == 0) {
			code[offset+0] = 0x40;
			code[offset+1] = 0x00;
		} else {
			int step_time =
				calculate_step_time(duration, increment);

			/*
			 * LP5562 ramp/wait duration is correlated with
			 * increment. Smaller the increment, shorter the
			 * duration.
			 * If single ramp/wait instruction could not handle
			 * specified duration, try to incrase increment.
			 */
			while (step_time < 0) {
				increment++;
				step_time = calculate_step_time(
					duration, increment);
			}

			increment = ramp_sign | (increment - 1);
			code[offset+0] = step_time;
			code[offset+1] = increment;
		}
		offset += 2;
	}
}

int led_lp5562_calibrate(const struct lp5562_calibration_data *cal_data,
			 struct lp5562_calibrated_color *cal_colors)
{
	const struct lp5562_calibration_code_map*
		code_map = cal_data->code_map;

	while (code_map->code_type != invalid) {
		int bgr = code_map->component;
		int rgb = (2 - bgr);
		int offset = (code_map->code_offset % NPROGSIZE) * 2;
		uint8_t *code =
			cal_data->pattern->engine_program[bgr].program_text;

		cal_data->pattern->engine_program[bgr].led_current =
			cal_colors[cal_data->color_index].current_values[rgb];

		/*
		 * Calculate calibrated LED intensity (0-255)
		 * +50 is to round off the fraction.
		 */
		int intensity =
			cal_colors[cal_data->color_index].pwm_values[rgb];
		if (cal_data->relative_brightness != 100) {
			intensity = (intensity *
				cal_data->relative_brightness + 50) / 100;
		}

		switch (code_map->code_type) {

		case set_pwm:
			calibrate_set_pwm_instruction(
				code_map, code, offset, intensity);
			break;

		case ramp:
			calibrate_ramp_instruction(
				code_map, code, offset, intensity);
			break;

		default:
			break;
		}

		code_map++;
	}
	return 0;
}

