/*
 * Copyright 2015 Google Inc.
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

#include <libpayload.h>

#include "base/init_funcs.h"
#include "base/device_tree.h"

typedef struct {
	uint32_t	rev;
	/* DVFS version string */
	char		dvfs_ver[60];
	uint32_t	rate;
	uint32_t	min_volt;
	uint32_t	gpu_min_volt;
	char		clock_src[32];
	uint32_t	clk_src_emc;
	uint32_t	needs_training;
	uint32_t	training_pattern;
	uint32_t	trained;
	uint32_t	periodic_training;
	uint32_t	trained_dram_clktree_c0d0u0;
	uint32_t	trained_dram_clktree_c0d0u1;
	uint32_t	trained_dram_clktree_c0d1u0;
	uint32_t	trained_dram_clktree_c0d1u1;
	uint32_t	trained_dram_clktree_c1d0u0;
	uint32_t	trained_dram_clktree_c1d0u1;
	uint32_t	trained_dram_clktree_c1d1u0;
	uint32_t	trained_dram_clktree_c1d1u1;
	uint32_t	current_dram_clktree_c0d0u0;
	uint32_t	current_dram_clktree_c0d0u1;
	uint32_t	current_dram_clktree_c0d1u0;
	uint32_t	current_dram_clktree_c0d1u1;
	uint32_t	current_dram_clktree_c1d0u0;
	uint32_t	current_dram_clktree_c1d0u1;
	uint32_t	current_dram_clktree_c1d1u0;
	uint32_t	current_dram_clktree_c1d1u1;
	uint32_t	run_clocks;
	uint32_t	tree_margin;
	uint32_t	num_burst;
	uint32_t	num_burst_per_ch;
	uint32_t	num_trim;
	uint32_t	num_trim_per_ch;
	uint32_t	num_mc_regs;
	uint32_t	num_up_down;
	uint32_t	vref_num;
	uint32_t	training_mod_num;
	uint32_t	dram_timing_num;

	uint32_t	burst_regs[221];
	uint32_t	burst_reg_per_ch[8];
	uint32_t	shadow_regs_ca_train[221];
	uint32_t	shadow_regs_quse_train[221];
	uint32_t	shadow_regs_rdwr_train[221];
	uint32_t	trim_regs[138];
	uint32_t	trim_perch_regs[10];

	/* Vref regs, impacted by training. */
	uint32_t	vref_perch_regs[4];

	/* DRAM timing parameters are required in some calculations. */
	uint32_t	dram_timings[5];
	uint32_t	training_mod_regs[20];
	uint32_t	save_restore_mod_regs[12];
	uint32_t	burst_mc_regs[33];
	uint32_t	la_scale_regs[24];

	uint32_t	min_mrs_wait;
	uint32_t	emc_mrw;
	uint32_t	emc_mrw2;
	uint32_t	emc_mrw3;
	uint32_t	emc_mrw4;
	uint32_t	emc_mrw9;
	uint32_t	emc_mrs;
	uint32_t	emc_emrs;
	uint32_t	emc_emrs2;
	uint32_t	emc_auto_cal_config;
	uint32_t	emc_auto_cal_config2;
	uint32_t	emc_auto_cal_config3;
	uint32_t	emc_auto_cal_config4;
	uint32_t	emc_auto_cal_config5;
	uint32_t	emc_auto_cal_config6;
	uint32_t	emc_auto_cal_config7;
	uint32_t	emc_auto_cal_config8;
	uint32_t	emc_cfg_2;
	uint32_t	emc_sel_dpd_ctrl;
	uint32_t	emc_fdpd_ctrl_cmd_no_ramp;
	uint32_t	dll_clk_src;
	uint32_t	clk_out_enb_x_0_clk_enb_emc_dll;
	uint32_t	latency;
} tegra210_emc_table;

#define OFFSET_MEMBER_EMC(x)	offsetof(tegra210_emc_table, x), \
					member_size(tegra210_emc_table, x)
#define OFFSET_EMC(x)		offsetof(tegra210_emc_table, x)

static const struct {
	const char *compat;
	size_t offset;
} str_prop_table[] = {
	{"nvidia,dvfs-version", OFFSET_EMC(dvfs_ver)},
	{"nvidia,source", OFFSET_EMC(clock_src)},
};

static const struct {
	const char *compat;
	size_t offset;
	size_t size;
} bin_prop_table[] = {
	{"nvidia,revision", OFFSET_MEMBER_EMC(rev)},
	{"clock-frequency", OFFSET_MEMBER_EMC(rate)},
	{"nvidia,emc-min-mv", OFFSET_MEMBER_EMC(min_volt)},
	{"nvidia,gk20a-min-mv", OFFSET_MEMBER_EMC(gpu_min_volt)},
	{"nvidia,src-sel-reg", OFFSET_MEMBER_EMC(clk_src_emc)},
	{"nvidia,needs-training", OFFSET_MEMBER_EMC(needs_training)},
	{"nvidia,training_pattern", OFFSET_MEMBER_EMC(training_pattern)},
	{"nvidia,trained", OFFSET_MEMBER_EMC(trained)},
	{"nvidia,periodic_training", OFFSET_MEMBER_EMC(periodic_training)},
	{"nvidia,trained_dram_clktree_c0d0u0",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c0d0u0)},
	{"nvidia,trained_dram_clktree_c0d0u1",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c0d0u1)},
	{"nvidia,trained_dram_clktree_c0d1u0",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c0d1u0)},
	{"nvidia,trained_dram_clktree_c0d1u1",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c0d1u1)},
	{"nvidia,trained_dram_clktree_c1d0u0",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c1d0u0)},
	{"nvidia,trained_dram_clktree_c1d0u1",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c1d0u1)},
	{"nvidia,trained_dram_clktree_c1d1u0",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c1d1u0)},
	{"nvidia,trained_dram_clktree_c1d1u1",
	 OFFSET_MEMBER_EMC(trained_dram_clktree_c1d1u1)},
	{"nvidia,current_dram_clktree_c0d0u0",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c0d0u0)},
	{"nvidia,current_dram_clktree_c0d0u1",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c0d0u1)},
	{"nvidia,current_dram_clktree_c0d1u0",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c0d1u0)},
	{"nvidia,current_dram_clktree_c0d1u1",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c0d1u1)},
	{"nvidia,current_dram_clktree_c1d0u0",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c1d0u0)},
	{"nvidia,current_dram_clktree_c1d0u1",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c1d0u1)},
	{"nvidia,current_dram_clktree_c1d1u0",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c1d1u0)},
	{"nvidia,current_dram_clktree_c1d1u1",
	 OFFSET_MEMBER_EMC(current_dram_clktree_c1d1u1)},
	{"nvidia,run_clocks", OFFSET_MEMBER_EMC(run_clocks)},
	{"nvidia,tree_margin", OFFSET_MEMBER_EMC(tree_margin)},
	{"nvidia,burst-regs-num", OFFSET_MEMBER_EMC(num_burst)},
	{"nvidia,burst-regs-per-ch-num", OFFSET_MEMBER_EMC(num_burst_per_ch)},
	{"nvidia,trim-regs-num", OFFSET_MEMBER_EMC(num_trim)},
	{"nvidia,trim-regs-per-ch-num", OFFSET_MEMBER_EMC(num_trim_per_ch)},
	{"nvidia,burst-mc-regs-num", OFFSET_MEMBER_EMC(num_mc_regs)},
	/* Is this really correct? */
	{"nvidia,la-scale-regs-num", OFFSET_MEMBER_EMC(num_up_down)},
	{"nvidia,vref-regs-num", OFFSET_MEMBER_EMC(vref_num)},
	{"nvidia,training-mod-regs-num", OFFSET_MEMBER_EMC(training_mod_num)},
	{"nvidia,dram-timing-regs-num", OFFSET_MEMBER_EMC(dram_timing_num)},
	{"nvidia,emc-registers", OFFSET_MEMBER_EMC(burst_regs)},
	{"nvidia,emc-burst-regs-per-ch", OFFSET_MEMBER_EMC(burst_reg_per_ch)},
	{"nvidia,emc-shadow-regs-ca-train",
	 OFFSET_MEMBER_EMC(shadow_regs_ca_train)},
	{"nvidia,emc-shadow-regs-quse-train",
	 OFFSET_MEMBER_EMC(shadow_regs_quse_train)},
	{"nvidia,emc-shadow-regs-rdwr-train",
	 OFFSET_MEMBER_EMC(shadow_regs_rdwr_train)},
	{"nvidia,emc-trim-regs", OFFSET_MEMBER_EMC(trim_regs)},
	{"nvidia,emc-trim-regs-per-ch", OFFSET_MEMBER_EMC(trim_perch_regs)},
	{"nvidia,emc-vref-regs", OFFSET_MEMBER_EMC(vref_perch_regs)},
	{"nvidia,emc-dram-timing-regs", OFFSET_MEMBER_EMC(dram_timings)},
	{"nvidia,emc-training-mod-regs", OFFSET_MEMBER_EMC(training_mod_regs)},
	{"nvidia,emc-save-restore-mod-regs",
	 OFFSET_MEMBER_EMC(save_restore_mod_regs)},
	{"nvidia,emc-burst-mc-regs", OFFSET_MEMBER_EMC(burst_mc_regs)},
	{"nvidia,emc-la-scale-regs", OFFSET_MEMBER_EMC(la_scale_regs)},
	{"nvidia,min-mrs-wait", OFFSET_MEMBER_EMC(min_mrs_wait)},
	{"nvidia,emc-mrw", OFFSET_MEMBER_EMC(emc_mrw)},
	{"nvidia,emc-mrw2", OFFSET_MEMBER_EMC(emc_mrw2)},
	{"nvidia,emc-mrw3", OFFSET_MEMBER_EMC(emc_mrw3)},
	{"nvidia,emc-mrw4", OFFSET_MEMBER_EMC(emc_mrw4)},
	{"nvidia,emc-mrw9", OFFSET_MEMBER_EMC(emc_mrw9)},
	{"nvidia,emc-mrs", OFFSET_MEMBER_EMC(emc_mrs)},
	{"nvidia,emc-emrs", OFFSET_MEMBER_EMC(emc_emrs)},
	{"nvidia,emc-emrs2", OFFSET_MEMBER_EMC(emc_emrs2)},
	{"nvidia,emc-auto-cal-config", OFFSET_MEMBER_EMC(emc_auto_cal_config)},
	{"nvidia,emc-auto-cal-config2",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config2)},
	{"nvidia,emc-auto-cal-config3",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config3)},
	{"nvidia,emc-auto-cal-config4",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config4)},
	{"nvidia,emc-auto-cal-config5",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config5)},
	{"nvidia,emc-auto-cal-config6",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config6)},
	{"nvidia,emc-auto-cal-config7",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config7)},
	{"nvidia,emc-auto-cal-config8",
	 OFFSET_MEMBER_EMC(emc_auto_cal_config8)},
	{"nvidia,emc-cfg-2", OFFSET_MEMBER_EMC(emc_cfg_2)},
	{"nvidia,emc-sel-dpd-ctrl", OFFSET_MEMBER_EMC(emc_sel_dpd_ctrl)},
	{"nvidia,emc-fdpd-ctrl-cmd-no-ramp",
	 OFFSET_MEMBER_EMC(emc_fdpd_ctrl_cmd_no_ramp)},
	{"nvidia,dll-clk-src", OFFSET_MEMBER_EMC(dll_clk_src)},
	{"nvidia,clk-out-enb-x-0-clk-enb-emc-dll",
	 OFFSET_MEMBER_EMC(clk_out_enb_x_0_clk_enb_emc_dll)},
	{"nvidia,emc-clock-latency-change", OFFSET_MEMBER_EMC(latency)},
};

void emc_add_bin_prop(DeviceTreeNode *node, const char *name, uint32_t *data,
		      size_t size)
{
	size_t i;
	for (i = 0; i < (size / sizeof(uint32_t)); i++)
		data[i] = htobel(data[i]);

	dt_add_bin_prop(node, (char *)name, data, size);
}

static uintptr_t emc_find_entry(tegra210_emc_table *table, size_t entries,
				uint32_t rate)
{
	int i;
	for (i = 0; i < entries; i++) {
		if (table[i].rate != rate)
			continue;

		return (uintptr_t)&table[i];
	}

	return 0;
}

static int emc_device_tree(DeviceTreeFixup *fixup, DeviceTree *tree)
{
	const char *table_compat = "nvidia,ram-code";
	const char *entry_compat = "nvidia,tegra21-emc-table";

	uint32_t ram_code = htobel(lib_sysinfo.ram_code);

	DeviceTreeNode *node = dt_find_prop_value(tree->root, table_compat,
						  &ram_code, sizeof(ram_code));

	if (node == NULL) {
		printf("EMC Error: No device tree nodes found.\n");
		return 0;
	} else
		printf("EMC: found node %s\n", node->name);

	DeviceTreeNode *child = NULL;
	tegra210_emc_table *table = (void *)lib_sysinfo.mtc_start;
	size_t entries = lib_sysinfo.mtc_size / sizeof(tegra210_emc_table);
	size_t count = 0;
	uint32_t *rate_ptr;
	uint32_t rate;
	size_t rate_size;

	printf("EMC: MTC table @ %p with entries 0x%zx\n", (void *)table,
	       entries);

	do {
		child = dt_find_next_compat_child(node, child, entry_compat);

		if (child == NULL)
			break;

		dt_find_bin_prop(child, "clock-frequency", (void **)&rate_ptr,
				 &rate_size);

		if ((rate_ptr == NULL) || (rate_size == 0)) {
			printf("EMC Error: No clock-frequency field\n");
			break;
		}

		rate = betohl(*rate_ptr);

		uintptr_t entry_ptr = emc_find_entry(table, entries, rate);
		if (entry_ptr == 0) {
			printf("EMC Error: No entry for %x\n", rate);
			continue;
		}

		count++;

		int j;
		for (j = 0; j < ARRAY_SIZE(bin_prop_table); j++) {
			emc_add_bin_prop(child, bin_prop_table[j].compat,
					 (uint32_t *)(entry_ptr +
						      bin_prop_table[j].offset),
					 bin_prop_table[j].size);
		}

		for (j = 0; j < ARRAY_SIZE(str_prop_table); j++) {
			dt_add_string_prop(child,
				(char *)str_prop_table[j].compat,
				(char *)(entry_ptr + str_prop_table[j].offset));
		}

	} while (1);

	printf("EMC: Initialized 0x%zx entries in emc table\n", count);
	return 0;
}

static DeviceTreeFixup emc_dt_fixup = {
	.fixup = emc_device_tree,
};

static int emc_setup(void)
{
	list_insert_after(&emc_dt_fixup.list_node, &device_tree_fixups);
	return 0;
}

INIT_FUNC(emc_setup);
