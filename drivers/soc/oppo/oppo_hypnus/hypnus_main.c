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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/cpufreq.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/topology.h>
#include <linux/notifier.h>

#if defined(CONFIG_ARCH_QCOM)
#include <linux/msm_kgsl.h>
#endif

#include "hypnus.h"
#include "hypnus_op.h"
#include "hypnus_dev.h"
#include "hypnus_sysfs.h"
#include "hypnus_uapi.h"

#define CREATE_TRACE_POINTS
#include "hypnus_trace.h"

static struct hypnus_data g_hypdata;

long hypnus_ioctl_get_rq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_rq_prop *prop = data;

	if (!pdata->cops->get_running_avg)
		return -ENOTSUPP;

	pdata->cops->get_running_avg(&prop->avg, &prop->big_avg,
					&prop->iowait_avg);

	return 0;
}

long hypnus_ioctl_get_cpuload(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_cpuload_prop *prop = data;
	unsigned int cpu = 0;

	if (!pdata->cops->get_cpu_load)
		return -ENOTSUPP;

	for_each_possible_cpu(cpu) {
		if (cpu_online(cpu))
			prop->cpu_load[cpu] = pdata->cops->get_cpu_load(cpu);
		else
			prop->cpu_load[cpu] = -1;
	}

	return 0;
}

long hypnus_ioctl_submit_cpufreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_cpufreq_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_cpu_freq_limit)
		return -ENOTSUPP;

	pdata->forceflag = prop->forceflag;

	trace_hypnus_ioctl_submit_cpufreq(prop);
	for (i = 0; i < pdata->cluster_nr; i++) {
		ret |= pdata->cops->set_cpu_freq_limit(i,
				prop->freq_prop[i].min,
				prop->freq_prop[i].max);
	}

	return ret;
}

static int hypnus_cpufreq_thermal_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct hypnus_data *hypdata = hypnus_get_hypdata();
	unsigned long min, max;
	unsigned int cpu = policy->cpu;
	unsigned int cluster_id = topology_physical_package_id(cpu);

	if (val != CPUFREQ_ADJUST)
		return NOTIFY_OK;

	/*
	 * Enable the thermal limit, make sure the policy->max is lower than therm limit max,
	 * and the policy->min is lower than therma limit min.
	 */
	if (hypdata->forceflag == THERM_FORCE_FLAG) {
		max = hypdata->ceiling_freq[cluster_id];
		min = hypdata->floor_freq[cluster_id];

		if (policy->max > max)
			policy->max = max;

		if (policy->min > min)
			policy->min = min;

		pr_debug("forceflag = %d, c_id = %d, max = %d, min = %d \n", hypdata->forceflag, cluster_id,
				max, min);
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_thermal_notifier = {
	.notifier_call = hypnus_cpufreq_thermal_notifier,
	.priority = -255,
};

long hypnus_ioctl_set_lpm_gov(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_lpm_prop *prop = data;

	if (!pdata->cops->set_lpm_gov)
		return -ENOTSUPP;

	return pdata->cops->set_lpm_gov(prop->lpm_gov);
}

long hypnus_ioctl_set_storage_scaling(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_storagescaling_prop *prop = data;

	if (!pdata->cops->set_storage_scaling)
		return -ENOTSUPP;

	return pdata->cops->set_storage_scaling(prop->storage_scaling);
}

long hypnus_ioctl_submit_lpm(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	return 0;
}

int hypnus_ioclt_submit_thermal_policy(struct hypnus_data *pdata)
{
	/* Todo */
	return 0;
}

long hypnus_ioctl_submit_ddr(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	int ret;
	struct hypnus_ddr_prop *prop = data;
	int state;

	if (!pdata->cops->set_ddr_state)
		return -ENOTSUPP;

	state = prop->state;
	ret = pdata->cops->set_ddr_state(state);
	if (ret)
		pr_err("%s err %d\n", __func__, ret);

	return ret;
}

static inline unsigned int
cpu_available_count(struct cpumask *cluster_mask)
{
	struct cpumask mask;

	cpumask_and(&mask, cluster_mask, cpu_online_mask);
	cpumask_andnot(&mask, &mask, cpu_isolated_mask);

	return cpumask_weight(&mask);
}

long hypnus_ioctl_submit_decision(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	return 0;
}

long hypnus_ioctl_poll_status(struct hypnus_data *pdata, unsigned int cmd, void *data)
{
	struct hypnus_poll_status *status = data;
	unsigned long t, flags;
	int ret = 0;

	if (!pdata)
		return -EINVAL;

	t = wait_for_completion_interruptible_timeout(&pdata->wait_event, 5*HZ);
	if (!t) {
		ret = -ETIMEDOUT;
	}
	spin_lock_irqsave(&pdata->lock, flags);
	status->screen_on = pdata->screen_on;
	spin_unlock_irqrestore(&pdata->lock, flags);
	return ret;
}

long hypnus_ioctl_get_soc_info(struct hypnus_data *pdata, unsigned int cmd, void *data)
{
	struct hypnus_soc_info *info = data;
	int i = 0;

	info->cluster_nr = pdata->cluster_nr;
	info->dsp_nr = pdata->dsp_nr;
	info->npu_nr = pdata->npu_nr;
	for (i = 0; i < pdata->cluster_nr; i++) {
		info->cluster[i].cluster_id = i;
		info->cluster[i].cpu_mask = cpumask_bits(&pdata->cluster_data[i].cluster_mask)[0];
		info->cluster[i].cpufreq.min = pdata->cluster_data[i].cpufreq_min;
		info->cluster[i].cpufreq.max = pdata->cluster_data[i].cpufreq_max;
	}
	return 0;
}

long hypnus_ioctl_submit_dspfreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_dspfreq_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_dsp_freq_limit)
		return -ENOTSUPP;

	for (i = 0; i < pdata->dsp_nr; i++) {
		ret |= pdata->cops->set_dsp_freq_limit(i,
				prop->freq_prop[i].min,
				prop->freq_prop[i].max);
	}

	return ret;
}

long hypnus_ioctl_submit_npufreq(struct hypnus_data *pdata,
	unsigned int cmd, void *data)
{
	struct hypnus_npufreq_prop *prop = data;
	int i, ret = 0;

	if (!pdata->cops->set_npu_freq_limit)
		return -ENOTSUPP;

	for (i = 0; i < pdata->npu_nr; i++) {
		ret |= pdata->cops->set_npu_freq_limit(i,
				prop->freq_prop[i].min,
				prop->freq_prop[i].max);
	}

	return ret;
}

long hypnus_ioctl_set_fpsgo(struct hypnus_data *pdata, unsigned int cmd, void *data)
{
	unsigned int *val = (unsigned int *)data;
	unsigned int enable;

	if (!pdata->cops->set_fpsgo_engine)
		return -ENOTSUPP;

	enable = !!(*val);
	return pdata->cops->set_fpsgo_engine(enable);
}

static int hypnus_parse_cpu_topology(struct hypnus_data *pdata)
{
	struct list_head *head = get_cpufreq_policy_list();
	struct cpufreq_policy *policy;
	int cluster_nr = 0;

	if (!head)
		return -EINVAL;

	list_for_each_entry(policy, head, policy_list) {
		int first_cpu = cpumask_first(policy->related_cpus);
		int index, cpu;
		struct cpu_data *pcpu = NULL;

		if (unlikely(first_cpu > NR_CPUS)) {
			pr_err("Wrong related cpus 0x%x\n",
				(int)cpumask_bits(policy->related_cpus)[0]);
			return -EINVAL;
		}

		for_each_cpu(cpu, policy->related_cpus) {
			pcpu = &pdata->cpu_data[cpu];
			pcpu->id = cpu;
			pcpu->cluster_id = topology_physical_package_id(cpu);
		}

		index = topology_physical_package_id(first_cpu);
		pr_info("cluster idx = %d, cpumask = 0x%x\n", index,
				(int)cpumask_bits(policy->related_cpus)[0]);
		pdata->cluster_data[index].id = index;
		cpumask_copy(&pdata->cluster_data[index].cluster_mask,
				policy->related_cpus);
		pdata->cluster_data[index].cpufreq_min = policy->cpuinfo.min_freq;
		pdata->cluster_data[index].cpufreq_max = policy->cpuinfo.max_freq;
		pr_info("min freq: %u, max freq: %u\n", pdata->cluster_data[index].cpufreq_min,
					pdata->cluster_data[index].cpufreq_max);
		cluster_nr++;
	}
	pdata->cluster_nr = cluster_nr;
	pr_info("Totally %d clusters\n", pdata->cluster_nr);
	return 0;
}

static int cpu_data_init(struct hypnus_data *pdata, unsigned int cpuid)
{
	unsigned int cpu;
	struct cpufreq_policy *policy;
	struct cpu_data *cpudata = &pdata->cpu_data[cpuid];
	struct cluster_data *c_cluster;
	unsigned int first_cpu;

	if (!cpu_online(cpuid))
		return 0;

	policy = cpufreq_cpu_get(cpuid);
	if (!policy)
		return 0;

	for_each_cpu(cpu, policy->related_cpus) {
		cpudata = &pdata->cpu_data[cpu];
		c_cluster = &pdata->cluster_data[cpudata->cluster_id];
		first_cpu = cpumask_first(&c_cluster->cluster_mask);
		cpudata->id_in_cluster = cpu - first_cpu;
		c_cluster->num_cpus = cpumask_weight(&c_cluster->cluster_mask);
		c_cluster->avail_cpus = c_cluster->num_cpus;

		if (cpu_online(cpu)) {
			cpudata->online = true;
			c_cluster->online_cpus++;
		}
	}
	cpufreq_cpu_put(policy);

	return 0;
}

struct hypnus_data *hypnus_get_hypdata(void)
{
	return &g_hypdata;
}

int __init dsp_info_init(struct hypnus_data *pdata)
{
	pdata->dsp_nr = 2; /* TO BE IMPROVED */
	return 0;
}

int __init npu_info_init(struct hypnus_data *pdata)
{
	pdata->npu_nr = 1; /* TO BE IMPROVED */
	return 0;
}

int __init hypnus_init(void)
{
	struct hypnus_data *pdata;
	unsigned int cpu;
	int ret;

	pdata = &g_hypdata;

	spin_lock_init(&pdata->lock);
	ret = hypnus_parse_cpu_topology(pdata);
	if (ret)
		goto err_hypnus_init;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		ret = cpu_data_init(pdata, cpu);
		if (ret < 0) {
			pr_err("%s cpu data init err!\n", __func__);
			goto err_hypnus_init;
		}
	}
	put_online_cpus();

	/* initialize chipset operation hooks */
	hypnus_chipset_op_init(pdata);

	cpufreq_register_notifier(&cpufreq_thermal_notifier, CPUFREQ_POLICY_NOTIFIER);

	dsp_info_init(pdata);

	npu_info_init(pdata);

	ret = hypnus_sysfs_init(pdata);
	if (ret)
		goto err_hypnus_init;

	ret = hypnus_dev_init(pdata);
	if (ret)
		goto err_dev_init;

	display_info_register(pdata);

	return 0;

err_dev_init:
	hypnus_sysfs_remove(pdata);
err_hypnus_init:
	return ret;
}

static void __exit hypnus_exit(void)
{
	struct hypnus_data *pdata = &g_hypdata;

	hypnus_dev_uninit(pdata);
	hypnus_sysfs_remove(pdata);
	display_info_unregister(pdata);
	cpufreq_unregister_notifier(&cpufreq_thermal_notifier, CPUFREQ_POLICY_NOTIFIER);
}


//module_init(hypnus_init);
module_exit(hypnus_exit);

MODULE_DESCRIPTION("Hypnus system controller");
MODULE_VERSION(HYPNUS_VERSION);
MODULE_LICENSE("GPL");
