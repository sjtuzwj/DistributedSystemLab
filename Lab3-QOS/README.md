# Lab2: QoS
517021910799 朱文杰
## Parameter Deduction

### srTCM
#### burst size充足的场景
|`flow_id`|`cir`|`cbs`|`ebs`|
|-|-|-|-|
|0|160000000|640000|1728000|
|1|80000000|320000|864000|
|2|40000000|160000|432000|
|3|20000000|800000|216000|
- 1.28Gbps = 160Mbyte/s = 160,000,000, 故flow0的cir为160000000,其他flow按8:4:2:1即可.
- 由于仅仅对带宽有要求,因此对于flow0的cbs我用每个周期的平均packet size 乘以平均packet num, ebs则是采用最大值。
- 使用代码time = time * hz / 1000000000 + cycle将ns转换为dpdk api所需要的CPU周期
```
fid: 0, send: 1558217, pass: 1558217
fid: 1, send: 1527152, pass: 1279326
fid: 2, send: 1453338, pass: 651735
fid: 3, send: 1479507, pass: 339811
```
运行结果如上,可以看出,flow0由于ebs已经超过了最大的burst size,因此不会发生丢包,而后续的flow随着cbs,ebs的减小,丢包的概率上升,并且通过比大致呈4:2:1
#### burst size不足的场景

|`flow_id`|`cir`|`cbs`|`ebs`|
|-|-|-|-|
|0|160000000|64000|172800|
|1|80000000|32000|86400|
|2|40000000|16000|43200|
|3|20000000|80000|21600|
- cir并没有发生改变,因为对于带宽要求是固定的
- cbs和ebs都降低了一个数量级，此时可以看出，由于被标记为RED的包变多，丢包率显著上升了
```
fid: 0, send: 1324061, pass: 407651
fid: 1, send: 1237043, pass: 215610
fid: 2, send: 1290655, pass: 125296
fid: 3, send: 1283882, pass: 89816
```

### RED
#### 黄绿全进队列，红色全丢
|`color`|`wq_log2`|`min_th`|`max_th`|`maxp_inv`|
|-|-|-|-|-|
|GREEN|RTE_RED_WQ_LOG2_MIN|1022|1023|10|
|YELLOW|RTE_RED_WQ_LOG2_MIN|1022|1023|10|
|RED|RTE_RED_WQ_LOG2_MIN|0|1|10|
- 这里将黄绿max_th设置为uint16下最大的1023,使得黄绿队列足够长,而红色则设置为0/1,也就是丢红色包。
```
fid: 0, send: 1558217, pass: 1558217
fid: 1, send: 1527152, pass: 1279326
fid: 2, send: 1453338, pass: 651735
fid: 3, send: 1479507, pass: 339811
```
#### 黄绿队列长度有限，红色全丢
|`color`|`wq_log2`|`min_th`|`max_th`|`maxp_inv`|
|-|-|-|-|-|
|GREEN|RTE_RED_WQ_LOG2_MIN|22|23|10|
|YELLOW|RTE_RED_WQ_LOG2_MIN|22|23|10|
|RED|RTE_RED_WQ_LOG2_MIN|0|1|10|
- 可以看出，由于黄绿队列变短,能够通过的黄绿包变少了。因此丢包率显著上升
```
fid: 0, send: 1398669, pass: 214894
fid: 1, send: 1348123, pass: 231936
fid: 2, send: 1402962, pass: 148095
fid: 3, send: 1410750, pass: 92546
```
#### 所有包均进队列
|`color`|`wq_log2`|`min_th`|`max_th`|`maxp_inv`|
|-|-|-|-|-|
|GREEN|RTE_RED_WQ_LOG2_MIN|1022|1023|10|
|YELLOW|RTE_RED_WQ_LOG2_MIN|1022|1023|10|
|RED|RTE_RED_WQ_LOG2_MIN|1022|1023|10|
- 此时所有flow均未发生丢包
```
fid: 0, send: 1380123, pass: 1380123
fid: 1, send: 1304761, pass: 1304761
fid: 2, send: 1341965, pass: 1341965
fid: 3, send: 1283635, pass: 1283635
```
#### 修改maxp_inv和wq_log2
wq_log2的修改没有显著影响，因为wq反映的是对于流量变化的敏感程度，而本程序的流量比较平稳。  
对黄绿全进队列，红色全丢的情况单独修改黄色的maxp_inv为2后输出结果并没有什么变化。猜测是因为实际运行时都没有达到最大标记概率,所以反应不明显。  
因此我简单地把wq_log2和maxp_inv设为同一个值。
```
fid: 0, send: 1843146, pass: 1843146
fid: 1, send: 1872560, pass: 1259223
fid: 2, send: 1856372, pass: 633748
fid: 3, send: 1835861, pass: 321581
```


## DPDK API

- `rte_panic`: 检测到错误时报错并终止程序
- `rte_meter_srtcm_config`: 用参数初始化srtcm配置，其余参数分别为带宽,突发尺寸，额外突发尺寸
- `rte_red_config_init`: 用参数初始化red配置，其余参数分别为过滤器权重的对数，最小队列阈值，最大队列阈值,最大标记概率倒数
- `rte_red_rt_data_init`: 用参数初始化red数据
- `rte_meter_srtcm_color_blind_check`: 根据srtcm的配置标记包的颜色,以CBS和EBS作为阈值
- `rte_red_mark_queue_empty`: 清空red算法的包队列
- `rte_red_enqueue`: 根据red的配置决定是进队还是丢包
- `rte_get_tsc_hz`:获得时钟频率
- `rte_get_tsc_cycles`:获得当前的时钟周期