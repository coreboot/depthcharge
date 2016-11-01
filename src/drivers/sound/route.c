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

static int route_enable_components(SoundRoute *route, int enable)
{
	int res = 0;

	SoundRouteComponent *component;
	list_for_each(component, route->components, list_node) {
		if (component->enabled != enable) {
			if (enable) {
				if (component->ops.enable(&component->ops))
					res = -1;
				else
					component->enabled = 1;
			} else {
				if (!component->ops.disable)
					continue;
				if (component->ops.disable(&component->ops))
					res = -1;
				else
					component->enabled = 0;
			}
		}
	}

	return res;
}

static int route_start(SoundOps *me, uint32_t frequency)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);
	int res;

	if (!route->source->start)
		return -1;

	res = route_enable_components(route, 1);
	if (res)
		goto err;

	res = route->source->start(route->source, frequency);
	if (res)
		goto err;

	return 0;

err:
	route_enable_components(route, 0);
	return res;
}

static int route_stop(SoundOps *me)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);
	int res;

	if (!route->source->stop)
		return -1;
	res = route->source->stop(route->source);
	res |= route_enable_components(route, 0);
	return res;
}

static int route_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);
	int res;

	if (!route->source->play)
		return -1;

	res = route_enable_components(route, 1);
	if (res)
		goto out;

	res = route->source->play(route->source, msec, frequency);

out:
	res |= route_enable_components(route, 0);
	return res;
}

static int route_set_volume(SoundOps *me, uint32_t volume)
{
	SoundRoute *route = container_of(me, SoundRoute, ops);

	if (!route->source->set_volume)
		return -1;
	return route->source->set_volume(route->source, volume);
}

SoundRoute *new_sound_route(SoundOps *source)
{
	SoundRoute *route = xzalloc(sizeof(*route));
	route->ops.start = &route_start;
	route->ops.stop = &route_stop;
	route->ops.play = &route_play;
	route->ops.set_volume = &route_set_volume;
	route->source = source;
	return route;
}
