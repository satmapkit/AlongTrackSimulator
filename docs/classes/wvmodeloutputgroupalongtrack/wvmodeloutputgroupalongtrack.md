---
layout: default
title: WVModelOutputGroupAlongTrack
parent: WVModelOutputGroupAlongTrack
grand_parent: Classes
nav_order: 1
mathjax: true
---

#  WVModelOutputGroupAlongTrack

Create an along-track output group for a satellite mission.


---

## Declaration
```matlab
 self = WVModelOutputGroupAlongTrack(model,missionName,ats)
```
## Parameters
+ `model`  WVModel scalar — parent model instance providing domain geometry and output file context
+ `missionName`  text scalar — mission key used by AlongTrackSimulator
+ `ats`  AlongTrackSimulator scalar — simulator used to compute and project tracks into the model domain

## Returns
+ `self`  WVModelOutputGroupAlongTrack instance

## Discussion

  Initializes the output group and precomputes projected along-track pass-overs through
  the model domain for repeat-cycle missions. The resulting tracks and first-passover
  times are used to schedule output and to write pass-over samples into NetCDF.
 
            
