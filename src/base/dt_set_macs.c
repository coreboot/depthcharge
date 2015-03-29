
#include <libpayload.h>
#include "base/device_tree.h"

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
