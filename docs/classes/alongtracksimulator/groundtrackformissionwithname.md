---
layout: default
title: groundTrackForMissionWithName
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 18
mathjax: true
---

#  groundTrackForMissionWithName

Compute the ground track for a mission over one or more orbits.


---

## Declaration
```matlab
 [lat,lon,time] = groundTrackForMissionWithName(mission,options)
```
## Parameters
+ `mission`  text (mission key in missionParameters)
+ `options.time`  numeric or datetime vector (optional) — sample times
+ `options.N_orbits`  double (default 1) — number of orbits to simulate when options.time is not provided

## Returns
+ `lat`  double column vector — latitude [deg]
+ `lon`  double column vector — longitude [deg]
+ `time`  same type as options.time or numeric seconds — time stamps for each point

## Discussion

  Returns latitude/longitude as a function of time for the specified mission's
  orbital elements. By default, the track starts at the mission start date and
  spans options.N_orbits orbits; you may override the time vector.
 
                
