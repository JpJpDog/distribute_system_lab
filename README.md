518021911058 沈瑜石 shenyushi@sjtu.edu.cn

我没有改头文件和rdt_sim.cc 但加了rdt_util.h 和 rdt_util.cc，以及改了Makefile。  
rdt_util中存放了主要的常数以及一些公用的类和函数。  

## 设计与实现方法
我使用了SR方法。  
data packet的结构如下：

| checksum | pkt_type | seq_n | pkt_size | payload | total |
| -------- | -------- | ----- | -------- | ------- | ----- |
| 2        | 1        | 1     | 1        | 123     | 128   |

ack packet的结构如下：

| checksum | pkt_type | seq_n | useless | total |
| -------- | -------- | ----- | ------- | ----- |
| 2        | 1        | 1     | 16      | 20    |

### SlidingWindow类

在rdt_sender.cc和rdt_receiver.cc中分别实现了SlidingWindow，用来控制seq_n的选择。它作为单例被Init创建，最后被Final销毁。  
SlidingWindow都使用循环队列的方式，维护base_和top_作为队列的前后界（前闭后开）以及base_seq_n_作为元素base_的seq_n。  
Sender_SlidingWindow的队列元素包括已发出的backups_和bool值status_。backups_用于备份发出的包以便超时重发，status_表示是否接收到ack。  
send函数返回下一个seq_n的值，如果超出了window的大小，则返回-1。  
backup与get_backup分别通过输入的seq_n保存和获取一个包。  
ack函数通过输入的seq_n，维护窗口性质，自动将窗口向前移动。如果输入ack的包不能使窗口移动则返回false，否则返回true。  

Receiver_SlidingWindow的队列元素包括一个大小为最大payload大小的数组，size_和status_。只保存去掉包头的payload部分，并用size_表示大小。status_表示是否收到data。  
seq_to_index将seq_n转为index（index表示原数组的下标），如果seq_n不在[base,base+window_size)中，如果在[base-window_size,base)中，则返回-1,否则返回-2。  
put函数将去掉包头的数据放在index位置。  
get_after_base函数将base_以后所有接收到的数据复制到out_data中，并将新收到的数据直接复制到第一个。同时移动窗口。

Sender另写了一个Sender_PktMake的函数来将数据制作为包（截取大小，加入checksum等工作）。它接收一个继承自StringManager的类型作为数据，下面再讲。  
Checksum使用了Internet Checksum。  

### 大致流程

整体上，sender和receiver如下工作。
Sender从上层接收到数据data后先检查checksum。然后如果buffer不为空，直接存入buffer。否则反复调用window的send函数。如果成功将data分为包的大小，用backup函数将包备份进window，然后以seq_n为key开启计时器，将数据发往下一层。如果不成功则将剩下的数据整体存入buffer。  
Sender如果从下层接收到ack，同样先检查checksum。然后首先关闭计时器，然后调用window的ack函数。如果返回true，则说明窗口向前移动了，从buffer中取出数据，重复之前发包的过程，直到buffer为空或send返回-1。  
Sender如果timeout，则将timer的key作为seq_n用get_backup找到之前的包，重发。

Receiver收到下层数据并检查checksum后，先调用window的seq_to_index，如果返回-2则直接返回。如果返回-1，则给sender再发一次ack。否则就获取了当前数据应该放置的index。如果index为window的base_，则调用get_after_base，移动窗口并将连续数据传给上层。否则调用push放数据进window。

### VirtualTimer
使用一个计时器模拟多个。作为单例出现在sender中始终。  
方法是用一个链表保存计时器的key和预计结束时间（以及传入的timeout后的函数指针）。每新加入一个（key，time）对时，获取当前时间，计算出结束时间，按结束时间的先后顺序插入至链表中，如果插到了第一个，则重新开启物理计时器。  
当计时器人为结束时，从链表中取出。如果为第一个且存在下一个，则用结束时间减当前时间的方法开启物理计时器。如果只有一个，则关闭物理计时器。  
当物理计时器timeout时，调用虚拟计时器的timeout，表示链表第一个timeout了。以key为参数调用函数。如果存在下一个，用同样的方法重启物理计时器。  

### CircleQueueBuffer
Buffer用于存储暂时没有发送的数据。在sender中以单例的方式存在。  
因为要有先进先出的性质，用循环队列实现。  
所以Sender_PktMake函数既要接收msg形式的数据，又要接收循环队列的格式，以及需要保存当前读取位置。于是定义了一个StringManager的纯虚类。PlainStringManager继承自StringManager，接收一个无所有权普通字符串和它的长度，定义了Sender_PktMake中需要的各种操作（判断为空，剩余大小，指针向前，复制一部分数据等）。同时CircleQueueBuffer也继承自StringManager，同样实现了各种纯虚函数。这样可以通用一个Sender_PktManager。

## 遇到的问题
1. 一开始将ack包设置的太短，加上checksum一共4个字节（后面没算checksum）。在currupt较高时，有很大概率在currput的时候重新“蒙对”一个合法的ack包。这个bug卡了很久才一点点定位是这个原因。只要将ack包加长一点，checksum就不太可能全都蒙对了，即可解决。  
这个问题真是太坑了，希望能提醒以后做lab的同学。主要症状为receiver跳过了正好max_seqn个包（max_seqn为windows_size整数倍时），或者sender收到了一个从未发出的ack包导致receiver落后与sender，sender就会反复超时却不能被receiver接收（receiver实际没有收到，窗口被卡主了）。  
由此发现checksum的有效性和包的长度也有关，不能太短。所以ack在tcp等协议中常常以放到一起发出，或者由receiver方的data顺便带出，减少包的浪费。
2. 开始将kMaxSeqN正好设为kWindowSize的两倍。容易sender重新发出data后，receiver收到上一个data，并将窗口后移，使得重发的data被误认为是新的。这个问题由于要等到被误认为是新的包被提交时才发现，比较难debug。解决方法是加大kMaxSeqN。
