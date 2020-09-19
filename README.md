# Two-Level Scheduler With MQSim
- An I/O scheduler for NVMe SSD     
- Usage for [MQSim](https://github.com/CMU-SAFARI/MQSim)      
- implemente TSU scheduler FLIN, based on FLIN paper

## Added Item
- TSU schedule policy: **SPEED_LIMIT**, see TSU_SpeedLimit.h and TSU_SpeedLimit.cpp   
- Plane Allocation Scheme: **RBGC** (**R**ound robin address **B**ypass **GC**)

## To Do
- request size 与 request queue length 成反比的实验 (√)     
- hard code 与使用 ml (machine learning) 的实验 (√)     
- 描绘 plane 状态的功能 (√ see function collect_results in Flash_Chip.h and Flash_Chip.cpp)    
- read 和 write之间限速的功能

## Experiment
- hard code VS ml  
    | | original | hard code | ml |
    | :-:| :-: | :-: | :-: |
    | # execution GC  | 42114 | 23990 | 374774 |
    | # non-execution GC | 178202 | 196312 | 220302 |
    | # total | 220316 | 220302 | 220311 |
    | runtime | 8min44s | 8min49s | 17min17s |
    | delay(us) | 1138 | 1010 | 1092 |

    1. hard code 使用 ml 训练集中块内无效页数量的3/4 quantile (210), 即块内无效页数量>210不执行GC
    2. hard code 比 ml 效果好, 并且执行时间短, 与 origianl 的时间差不多

- request size 与 request queue length 成反比的实验
    all workloads are read and streaming
    | size (kb) / length | alone (us) | shared (us) |
    | :-:| :-: | :-: |
    | 8 / 64  | 184 | 273 |
    | 64 / 8 | 173 | 408 |
    | | | |
    | 16 / 64 | 282 | 488 |
    | 64 / 16 | 282 | 488 |

    1. 进入 TSU 事务的大小最大为16 kB
    2. 事务的入队速率比事务的大小更能影响公平性, 入队的速率小的负载在单独运行时的延迟较短, 但往往被延缓
    3. 除去一些个别的测试用例, 写具有和读一样的结论