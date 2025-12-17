---
layout: default
title: repeatGroundTrackForMissionWithName
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 37
mathjax: true
---

#  repeatGroundTrackForMissionWithName

Compute the full repeat-cycle ground track for a mission.


---

## Declaration
```matlab
 [lat,lon,time] = repeatGroundTrackForMissionWithName(mission)
```
## Parameters
+ `mission`  text (mission key in missionParameters)

## Returns
+ `lat`  double column vector — latitude [deg]
+ `lon`  double column vector — longitude [deg]
+ `time`  numeric column vector — elapsed seconds from start of repeat cycle

## Discussion

  For missions with a defined repeat cycle, returns the ground track over one
  complete repeat period (repeat_cycle days). For missions
  without a defined repeat cycle, this function will throw an
  error.
 
            
