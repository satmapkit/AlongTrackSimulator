---
layout: default
title: nodalPeriodForMissionWithName
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 27
mathjax: true
---

#  nodalPeriodForMissionWithName

Return the nodal period for a mission (including J2 nodal precession).


---

## Declaration
```matlab
 T = nodalPeriodForMissionWithName(mission)
```
## Parameters
+ `mission`  text (mission key in missionParameters)

## Returns
+ `T`  double — nodal period [s]

## Discussion

  Uses a J2-based correction to compute the period in the Earth-fixed frame.
 
        
