/*
 * Copyright 2026 Google LLC.
 * Depthcharge Driver for Qualcomm WSA8845 Smart Amplifier over SoundWire
 */

#ifndef __DRIVERS_SOUND_WSA8845_SNDW_H__
#define __DRIVERS_SOUND_WSA8845_SNDW_H__

#include <drivers/bus/soundwire/soundwire.h>
#include <drivers/sound/sndw_common.h>
#include <drivers/sound/sound.h>

extern const sndw_codec_id wsa8845_id;

/*
 * new_wsa8845_sndw - Allocate and initialize the WSA8845 Soundwire device.
 * sndw: Pointer to the SoundWire bus operations.
 * device_index: Device index on the SoundWire bus.
 * beep_duration: Default duration for the beep operation.
 *
 * Return: Pointer to the allocated SoundDevice_sndw structure.
 */
SoundDevice_sndw *new_wsa8845_sndw(SndwOps *sndw, uint32_t device_index, uint32_t beep_duration);

#endif /* __DRIVERS_SOUND_WSA8845_SNDW_H__ */
