#include "qos.h"

#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

// a profile include max CBS, EBS length and update time and amount
struct rte_meter_srtcm_profile app_srtcm_profile;

// the params have to deduce
struct rte_meter_srtcm_params app_srtcm_params = {
    .cir = 1000000 * 46, .cbs = 2048, .ebs = 2048};

// the run time context of each flow
struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];

/**
 * srTCM
 */
int qos_meter_init(void) {
  int ret, i;
  ret = rte_meter_srtcm_profile_config(&app_srtcm_profile, &app_srtcm_params);
  if (ret) return ret;
  for (i = 0; i < APP_FLOWS_MAX; i++) {
    ret = rte_meter_srtcm_config(app_flows + i, &app_srtcm_profile);
    if (ret) return ret;
  }
  return 0;
}

enum qos_color qos_meter_run(uint32_t flow_id, uint32_t pkt_len,
                             uint64_t time) {
  uint64_t cur_time = rte_rdtsc();
  return rte_meter_srtcm_color_blind_check(app_flows + flow_id,
                                           &app_srtcm_profile, cur_time);
}

/**
 * WRED
 */

int qos_dropper_init(void) { /* to do */
}

int qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time) {
  /* to do */
}