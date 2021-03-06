#include <stdlib.h>
#include <time.h>

#include "qos.h"
#include "rte_common.h"
#include "rte_mbuf.h"

int main(int argc, char **argv) {
  int ret, i, j;

  /** init EAL */
  ret = rte_eal_init(argc, argv);
  if (ret < 0) rte_panic("Cannot init EAL\n");

  if (ret = qos_meter_init()) printf("meter init fail! fail num %d\n", ret);
  if (ret = qos_dropper_init()) printf("dropper init fail! fail num %d\n", ret);

  srand(time(NULL));
  uint64_t time = 0;
  int cnt_send[APP_FLOWS_MAX];
  int cnt_pass[APP_FLOWS_MAX];
  for (i = 0; i < APP_FLOWS_MAX; i++) {
    cnt_send[i] = cnt_pass[i] = 0;
  }

  for (i = 0; i < 10; i++) {
    /** 1000 packets per period averagely */
    int burst = 500 + rand() % 1000;

    for (j = 0; j < burst; j++) {
      uint32_t flow_id = (uint32_t)(rand() % APP_FLOWS_MAX);

      /** 640 bytes per packet averagely */
      uint32_t pkt_len = (uint32_t)(128 + rand() % 1024);

      /** get color */
      enum qos_color color = qos_meter_run(flow_id, pkt_len, time);

      /** make decision: whether drop */
      int pass = qos_dropper_run(flow_id, color, time);

      cnt_send[flow_id] += pkt_len;
      cnt_pass[flow_id] += pass ? 0 : pkt_len;
    }
    time += 1000000;
  }

  for (i = 0; i < APP_FLOWS_MAX; i++) {
    printf("fid: %d, send: %d, pass: %d %lf\n", i, cnt_send[i], cnt_pass[i],
           (double)cnt_pass[i] / cnt_send[i]);
  }

  return 0;
}