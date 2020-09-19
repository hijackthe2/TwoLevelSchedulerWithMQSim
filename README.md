# Two-Level Scheduler With MQSim
- An I/O scheduler for NVMe SSD     
- Usage for [MQSim](https://github.com/CMU-SAFARI/MQSim)      
- implemente TSU scheduler FLIN, based on FLIN paper

## Added Item
- TSU schedule policy: **SPEED_LIMIT**, see TSU_SpeedLimit.h and TSU_SpeedLimit.cpp   
- Plane Allocation Scheme: **RBGC** (**R**ound robin address **B**ypass **GC**)

## To Do
- request size 与 request queue length 成反比的实验     
- hard code 与使用 ml (machine learning) 的实验     
- 描绘 plane 状态的功能 (√ see function collect_results in Flash_Chip.h and Flash_Chip.cpp)    
- read 和 write之间限速的功能

## Experiment
- hard code VS ml  
    | | original | hard code | ml |
    | :-:| :-: | :-: | :-: |
    | # execution GC  | 42114 | 23990 | 374774 |
    | # non-execution GC | 178202 | 196312 | 220302 |
    | # total | 220316 | 220302 | 220311 |
    | runtime | 7min | 8min49s | 17min17s |

    1. hard code 使用 ml 训练集中块内无效页数量的3/4 quatile 210, 即块内无效页数量>210不执行GC
    2. 从实验中看出，hard code 比 ml 效果好, 并且执行时间短，但比 origianl 的时间长, 但这样的开销可接受