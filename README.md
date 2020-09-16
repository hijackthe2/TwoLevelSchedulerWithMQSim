# Two-Level Scheduler With MQSim
- An I/O scheduler for NVMe SSD     
- Usage for [MQSim](https://github.com/CMU-SAFARI/MQSim)      
- implemente TSU scheduler FLIN, based on FLIN paper

## Added Item
- TSU schedule policy: **SPEED_LIMIT**, see TSU_SpeedLimit.h and TSU_SpeedLimit.cpp   
- Plane Allocation Scheme: **RBGC** (**R**ound robin address **B**ypass **GC**)

## To Do
- request size 与 request queue length 成反比的实验     
- hard code 与使用 ml 的实验     
- 描绘 plane 状态的功能   
- read 和 write之间限速的功能
