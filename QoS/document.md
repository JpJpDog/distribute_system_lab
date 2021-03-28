### rte_meter APIs note

1. ```c
   enum rte_color;
   ``` 
2. ```c
    struct rte_meter_srtcm_params;
    ``` 
    include CIR,CBS,EBS size.  
    to set profile
3. ```c
    struct rte_meter_srtcm_profile;
    ```
    configuration of srTCM, can shared by multiple objects.  
    include upper limit of CBS and EBS, the update period (CPU circles) and update amount of C, E buckets
4. ```c
    struct rte_meter_srtcm;
    ```
    store runtime context of per traffic flow.  
    include last update time and last C,E tokens number.
4. ```c
    int rte_meter_srtcm_profile_config(struct rte_meter_srtcm_profile *p, struct rte_meter_srtcm_params *params);
    ```
    set `p`'s CBS and EBS uplimit according to `params`  
    update period and amount is set by calling `rte_meter_get_tb_params`, which  
    configure the update period at least `RTE_METER_TB_PERIOD_MIN` and update amount proper according to update speed and CIR.  
5. ```c
    int rte_meter_srtcm_config(struct rte_meter_srtcm *m, struct rte_meter_srtcm_profile *p);
    ```
    init runtime context for each flow. full CBS and EBS according to `p` and set current time by `rte_get_tsc_cycles`.  
6. ```c
    static inline enum rte_color rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m, struct rte_meter_srtcm_profile *p, uint64_t time, uint32_t pkt_len);
    ```
    update `m->time` according to how many integer period num in difference of `time` and `m->time`.   
    add `m->cbs` and `m->ebs` according to period num and handle the overflow (larger than `p->cbs`, `p->ebs`)  
    minus `m->cbs` and `m->ebs` by bytes according to `pkt_len` and return color.


### qos_meter example note
1. configure the profile and each flow runtime context 
2. in the main_loop, for every received pkt, first decide with flow will be sent and then call `app_pkt_handle` to decide with `rte_pktmbuf_free` it or transmit it by `rte_eth_tx_buffer`.
3. `app_pkt_handle` seems to be simple blind color ?