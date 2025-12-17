---
layout: default
title: wvmOutputGroupForRepeatMissionWithName
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 40
mathjax: true
---

#  wvmOutputGroupForRepeatMissionWithName

Build a WaveVortexModel output group for an along-track sampling pattern.


---

## Declaration
```matlab
 outputGroup = wvmOutputGroupForRepeatMissionWithName(model, missionName)
```
## Parameters
+ `model`  WVModel
+ `missionName`  string — mission key

## Returns
+ `outputGroup`  WVModelOutputGroup

## Discussion

  Convenience function that uses the model's domain (wvt.Lx/Ly and latitude)
  to generate repeat-cycle projected track points.
 
          
