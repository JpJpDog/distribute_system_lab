**Q1**:  
一个2M的大页相比4K的页，可以在一条TLB映射更多的虚拟地址，减少TLBmiss，较少访存的时间。  
**Q2**:  
首先主线程调用rte_eal_init初始化。主线程然后调用*RTE_LCORE_FOREACH_WORKER*宏，遍历每一个逻辑核，调用 *rte_eal_remote_launch* 在这个核上开启从线程，执行lcore_hello。然后主线程自己也执行lcore_hello。然后主线程调用	*rte_eal_mp_wait_lcore* 等待从线程运行完毕，最后返回。  
**Q3**: 
1. rte_eth_tx_burst(port_id, queue_id, tx_pkts, nb_pkts)发送包  
   port_id 为接收包的接口号，queue_id  为接收队列的id。
   tx_pkts 为一个指向 *struct rte_mbuf* 的指针数组，用于存放接收包的内容，nb_pkts为tx_pkts向量的长度。 返回值为接收包的数量  
2. rte_eth_tx_burst 和tx_burst相似，用来接收包。  
3. rte_pktmbuf_free  
