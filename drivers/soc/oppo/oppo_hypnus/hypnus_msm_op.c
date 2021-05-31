/*
 * Copyright (C) 2015-2018 OPPO, Inc.
 * Author: Chuck Huang <huangcheng-m@oppo.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/sched/stat.h>
#include <linux/sched/core_ctl.h>
#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_pwrctrl.h"
#include "hypnus_op.h"
#include <linux/mmc/host.h>
#include <linux/ufshcd-platform.h>

#define PM_QOS_DISABLE_SLEEP_VALUE	(43)

static struct pm_qos_request hypnus_lpm_qos_request;

static int msm_get_running_avg(int *avg, int *big_avg, int *iowait_avg)
{
/* TODO qinyonghui*/
#if 0
	int max_nr, big_max_nr;

	sched_get_nr_running_avg(avg, iowait_avg, big_avg,
				&max_nr, &big_max_nr);
#endif

	return 0;
}

static int msm_get_cpu_load(u32 cpu)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	return sched_get_cpu_util(cpu);
#else
	return 0;
#endif
}
static int msm_set_cpu_freq_limit(u32 c_index, u32 min, u32 max)
{
	struct cpufreq_policy *policy;
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	int cpu;

	if (!hypdata)
		return -EINVAL;

	if (hypdata->forceflag == THERM_FORCE_FLAG) {
		hypdata->ceiling_freq[c_index] = max;
		hypdata->floor_freq[c_index] = min;
	}

	cpu = cpumask_first(&hypdata->cluster_data[c_index].cluster_mask);
	if (!cpu_online(cpu))
		return 0;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return -EINVAL;

	policy->user_policy.min = min;
	policy->user_policy.max = max;
	cpufreq_cpu_put(policy);

	/* TODO qinyonghui */
	cpufreq_update_policy(cpu);
	return 0;
}

static int msm_set_lpm_gov(u32 type)
{
	switch (type) {
		case LPM_DEFAULT:
			pm_qos_update_request(&hypnus_lpm_qos_request, PM_QOS_DEFAULT_VALUE);
			break;
		case LPM_USE_GOVERNOR:
			break;
		case LPM_DISABLE_SLEEP:
			pm_qos_update_request(&hypnus_lpm_qos_request, PM_QOS_DISABLE_SLEEP_VALUE);
			break;
	}
	return 0;
}

/* mmc */
int hypnus_mmc_scaling_enable(int index, int value){
	int ret = 0;
	if (index >= MAX_MMC_STORE_HOST || mmc_store_host[index] == NULL){
		pr_err("hypnus_mmc_scaling_enable index err!\n");
		return -1;
	}
	ret = mmc_scaling_enable(mmc_store_host[index], value);
	return ret;
}

static int msm_set_storage_clk_scaling(u32 type)
{
	return 0;
}

static int msm_display_init(void)
{
	/* Todo */
	return 0;
}

static u64 msm_get_frame_cnt(void)
{
	/* Todo */
	return 0;
}

static int msm_get_display_resolution(unsigned int *xres, unsigned int *yres)
{
	/* Todo */
	return 0;
}

static int msm_set_ddr_state(u32 state)
{
	/* Todo */
	return 0;
}

static int msm_set_thermal_policy(bool use_default)
{
	/* Todo */
	return 0;
}

static struct hypnus_chipset_operations msm_op = {
	.name = "msm",
	.get_running_avg = msm_get_running_avg,
	.get_cpu_load = msm_get_cpu_load,
	.set_cpu_freq_limit = msm_set_cpu_freq_limit,
	.isolate_cpu = sched_isolate_cpu,
	.unisolate_cpu = sched_unisolate_cpu,
	.display_init = msm_display_init,
	.get_frame_cnt = msm_get_frame_cnt,
	.get_display_resolution = msm_get_display_resolution,
	.set_lpm_gov = msm_set_lpm_gov,
	.set_storage_scaling = msm_set_storage_clk_scaling,
	.set_sched_prefer_idle = NULL,
	.set_ddr_state = msm_set_ddr_state,
	.set_thermal_policy = msm_set_thermal_policy,
};

void hypnus_chipset_op_init(struct hypnus_data *pdata)
{
	pm_qos_add_request(&hypnus_lpm_qos_request, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	pdata->cops = &msm_op;
}
