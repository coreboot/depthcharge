/*
 * Copyright 2013 Google LLC
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#ifndef __DRIVERS_SOUND_ROUTE_H__
#define __DRIVERS_SOUND_ROUTE_H__

#include <commonlib/list.h>

#include "drivers/sound/sound.h"

typedef struct SoundRouteComponentOps
{
	int (*enable)(struct SoundRouteComponentOps *me);
	// The disable() callback is optional. Only use when necessary!
	int (*disable)(struct SoundRouteComponentOps *me);
	// Some components also need play() process
	int (*play)(struct SoundRouteComponentOps *me);
} SoundRouteComponentOps;

typedef struct
{
	SoundRouteComponentOps ops;

	int enabled;
	struct list_node list_node;
} SoundRouteComponent;

typedef struct SoundRoute
{
	SoundOps ops;
	SoundOps *source;

	struct list_node components;
} SoundRoute;

SoundRoute *new_sound_route(SoundOps *source);

#endif /* __DRIVERS_SOUND_ROUTE_H__ */
