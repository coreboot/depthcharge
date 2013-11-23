/*
 * Copyright 2013 Google Inc. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "base/list.h"
#include "drivers/sound/route.h"

static int route_enable_components(SoundRoute *route)
{
	int res = 0;

	SoundRouteComponent *component;
	list_for_each(component, route->components, list_node) {
		if (!component->enabled) {
			if (component->ops.enable(&component->ops))
				res = -1;
			else
				component->enabled = 1;
		}
	}

	return res;
}

static int route_start(SoundOps *me, uint32_t frequency)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);

	if (route_enable_components(route))
		return 1;

	if (!route->source->start)
		return -1;
	return route->source->start(route->source, frequency);
}

static int route_stop(SoundOps *me)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);
	if (!route->source->stop)
		return -1;
	return route->source->stop(route->source);
}

static int route_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);

	if (route_enable_components(route))
		return 1;

	if (!route->source->play)
		return -1;
	return route->source->play(route->source, msec, frequency);
}

SoundRoute *new_sound_route(SoundOps *source)
{
	SoundRoute *route = xzalloc(sizeof(*route));
	route->ops.start = &route_start;
	route->ops.stop = &route_stop;
	route->ops.play = &route_play;
	route->source = source;
	return route;
}
