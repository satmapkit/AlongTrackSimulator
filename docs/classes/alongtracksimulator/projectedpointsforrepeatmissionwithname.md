---
layout: default
title: projectedPointsForRepeatMissionWithName
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 35
mathjax: true
---

#  projectedPointsForRepeatMissionWithName

Project repeat-cycle ground-track points into a local Transverse Mercator box.


---

## Declaration
```matlab
 alongtrack = projectedPointsForMissionWithName(missionName,requiredOptions,options)
```
## Parameters
+ `missionName`  string — mission key
+ `requiredOptions.Lx`  double — box width [m]
+ `requiredOptions.Ly`  double — box height  [m]
+ `requiredOptions.lat0`  double — box reference latitude [deg]
+ `requiredOptions.lon0`  double — central meridian for projection [deg]
+ `options.time`  numeric/datetime vector — times at which to sample the orbit
+ `options.origin`  {'lower-left','center'} (default 'lower-left') — coordinate origin convention for returned (x,y)

## Discussion

  Similar to -projectedPointsForMissionWithName, but works only
  for missions with repeat cycles and returns exactly one
  repeat cycle through the box.
 
  Computes the ground track for the requested mission within a bounding box
  centered at (lon0,lat0) with size (Lx,Ly). Points are
  returned in projected coordinates (using a transverse
  Mercator projector). By default the origin is set to the
  lower-left of the bounding box, but can be optionally set to
  the center.
  
  The function returns a struct with fields (x,y,t)
    - x: double column vector — projected x-coordinates
    - y: double column vector — projected y-coordinates
    - t: column vector — times corresponding to each point (sorted)
    - repeatCycle: time of the repeat cycle [s]
 
                    - Returns: alongtrack struct with field (x,y,t,repeatCycle)
