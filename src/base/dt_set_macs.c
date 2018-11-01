
#include <libpayload.h>
#include "base/device_tree.h"
#include "base/vpd_util.h"

typedef struct  {
	uint8_t b[6];
} mac_addr_t;

int dt_set_mac_addresses(DeviceTree *tree, const DtPathMap *maps)
{
	int i, rv = 0;
	const DtPathMap *map = maps;

	for (i = 0;
	     map->dt_path && (i < lib_sysinfo.num_macs);
	     i++, map++) {

		rv |= dt_set_bin_prop_by_path
			(tree, map->dt_path, lib_sysinfo.macs[i].mac_addr,
			 sizeof(lib_sysinfo.macs[i].mac_addr),
			 map->force_create);
	}

	return rv;
}

/*
 * Decode string representation of a MAC address (6 bytes each represented by
 * two hex digit characters, optionally separated by colons). The string must
 * be null-terminated.
 */
static int decode_mac(const char *mac_addr_str, mac_addr_t *mac_addr)
{
	int i, j;

	for (i = 0, j = 0; i < sizeof(mac_addr->b); i++, j += 2) {
		if (mac_addr_str[j] == ':')
			j++;

		if (!isxdigit(mac_addr_str[j]) ||
		    !isxdigit(mac_addr_str[j + 1]))
			return 1;

		mac_addr->b[i] = hex2bin(mac_addr_str[j]) << 4;
		mac_addr->b[i] |= hex2bin(mac_addr_str[j + 1]);
	}

	return 0;
}

static int dt_set_mac_addresses_from_vpd(DeviceTreeFixup *dt_fixup,
					 DeviceTree *tree)
{
	VpdDeviceTreeFixup *vpd_fixup = container_of(dt_fixup,
						     VpdDeviceTreeFixup, fixup);
	const VpdDeviceTreeMap *map;
	char mac_addr_str[sizeof("00:00:00:00:00:00")];
	mac_addr_t *mac_addr;

	for (map = vpd_fixup->map; map->vpd_name != NULL; map++) {
		if (!vpd_gets(map->vpd_name, mac_addr_str,
			      sizeof(mac_addr_str))) {
			printf("VPD entry '%s' does not exist\n", map->vpd_name);
			continue;
		}

		mac_addr = xzalloc(sizeof(*mac_addr));

		if (decode_mac(mac_addr_str, mac_addr)) {
			printf("MAC address '%s' in the VPD has an invalid format: %s\n",
			       map->vpd_name, mac_addr_str);
			continue;
		}

		if (dt_set_bin_prop_by_path(tree, map->dt_path, mac_addr,
					    sizeof(*mac_addr), 0))
			printf("Failed to patch DT entry '%s'\n",
			       map->dt_path);
	}

	return 0;
}

void dt_register_vpd_mac_fixup(const VpdDeviceTreeMap *map)
{
	VpdDeviceTreeFixup *dt_fixup = xzalloc(sizeof(*dt_fixup));

	dt_fixup->fixup.fixup = dt_set_mac_addresses_from_vpd;
	dt_fixup->map = map;

	list_insert_after(&dt_fixup->fixup.list_node, &device_tree_fixups);
}
