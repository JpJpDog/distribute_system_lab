#include "qos.h"

#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

// a profile include max CBS, EBS length and update time and amount
static struct rte_meter_srtcm_profile app_srtcm_profile[APP_FLOWS_MAX];

// the params have to deduce
static struct rte_meter_srtcm_params app_srtcm_params[APP_FLOWS_MAX] = {
    {.cir = 160000000, .cbs = 640000, .ebs = 320000},
    {.cir = 80000000, .cbs = 320000, .ebs = 160000},
    {.cir = 40000000, .cbs = 160000, .ebs = 80000},
    {.cir = 20000000, .cbs = 80000, .ebs = 40000},
};

// the run time context of each flow
static struct rte_meter_srtcm app_srtcms[APP_FLOWS_MAX];

static struct rte_red_config app_red_config[APP_FLOWS_MAX][3];

static struct rte_red_params app_red_params[APP_FLOWS_MAX][3] = {
    {
        {.min_th = 245, .max_th = 270, .maxp_inv = 10, .wq_log2 = 8},
        {.min_th = 245, .max_th = 270, .maxp_inv = 10, .wq_log2 = 8},
        {.min_th = 245, .max_th = 270, .maxp_inv = 10, .wq_log2 = 8},
    },
    {
        {.min_th = 30, .max_th = 106, .maxp_inv = 4, .wq_log2 = 5},
        {.min_th = 10, .max_th = 106, .maxp_inv = 2, .wq_log2 = 5},
        {.min_th = 3, .max_th = 106, .maxp_inv = 2, .wq_log2 = 5},
    },
    {
        {.min_th = 20, .max_th = 53, .maxp_inv = 2, .wq_log2 = 4},
        {.min_th = 10, .max_th = 53, .maxp_inv = 2, .wq_log2 = 4},
        {.min_th = 4, .max_th = 53, .maxp_inv = 2, .wq_log2 = 4},
    },
    {
        {.min_th = 4, .max_th = 26, .maxp_inv = 8, .wq_log2 = 3},
        {.min_th = 2, .max_th = 26, .maxp_inv = 4, .wq_log2 = 3},
        {.min_th = 2, .max_th = 26, .maxp_inv = 2, .wq_log2 = 3},
    },
};

static struct rte_red app_reds[APP_FLOWS_MAX];

static inline int rte_red_config_init2(struct rte_red_config* config,
                                       struct rte_red_params* params) {
  return rte_red_config_init(config, params->wq_log2, params->min_th,
                             params->max_th, params->maxp_inv);
}

static int queue_len[APP_FLOWS_MAX];

static uint64_t last_time;

/**
 * srTCM
 */
int qos_meter_init(void) {
  int ret, i;

  for (i = 0; i < APP_FLOWS_MAX; i++) {
    ret = rte_meter_srtcm_profile_config(app_srtcm_profile + i,
                                         app_srtcm_params + i);
    if (ret) return ret;
  }
  for (i = 0; i < APP_FLOWS_MAX; i++) {
    ret = rte_meter_srtcm_config(app_srtcms + i, app_srtcm_profile + i);
    if (ret) return ret;
  }
  return 0;
}

enum qos_color qos_meter_run(uint32_t flow_id, uint32_t pkt_len,
                             uint64_t time) {
  return rte_meter_srtcm_color_blind_check(
      app_srtcms + flow_id, app_srtcm_profile + flow_id, time, pkt_len);
}

/**
 * WRED
 */

int qos_dropper_init(void) {
  int ret, i, j;
  for (i = 0; i < APP_FLOWS_MAX; i++) {
    for (j = 0; j < 3; j++) {
      ret = rte_red_config_init2(&(app_red_config[i][j]),
                                 &(app_red_params[i][j]));
      if (ret) return ret;
    }
  }
  for (i = 0; i < APP_FLOWS_MAX; i++) {
    ret = rte_red_rt_data_init(app_reds + i);
    if (ret) return ret;
  }
  memset(queue_len, 0, sizeof(*queue_len) * APP_FLOWS_MAX);
  last_time = 0;
  return 0;
}

// extern int count1, count2, count3;

extern FILE* debug_file;

int qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time) {
  int ret;
  if (time != last_time) {
    last_time = time;
    memset(queue_len, 0, sizeof(*queue_len) * APP_FLOWS_MAX);
    rte_red_mark_queue_empty(app_reds + flow_id, time);
  }
  ret = rte_red_enqueue(&(app_red_config[flow_id][color]), app_reds + flow_id,
                        queue_len[flow_id], time);
  if (ret) {
    return 1;
  } else {
    queue_len[flow_id]++;
    return 0;
  }
}