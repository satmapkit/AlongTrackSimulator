---
layout: default
title: computeGroundTrackWithNodalPrecessionSimple
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 8
mathjax: true
---

#  computeGroundTrackWithNodalPrecessionSimple

computes the ground track (latitude and longitude) of a satellite given


---

## Declaration
```matlab
 [lat, lon] = computeGroundTrackWithNodalPrecessionSimple(semi_major_axis, e, incl, RAAN_0, omega, M0, t)
```
## Parameters
+ `semi_major_axis`  semi-major axis [km]
+ `e`  eccentricity (0 for circular, >0 for elliptical)
+ `incl`  inclination [degrees]
+ `RAAN_0`  right ascension of ascending node [rad]
+ `omega`  argument of perigee [rad]
+ `M0`  initial mean anomaly at t = 0 [rad]
+ `t`  time vector [s]

## Returns
+ `lat`  latitude (deg) vector corresponding to times in t
+ `lon`  longitude (deg) vector corresponding to times in t

## Discussion
the orbital parameters and time vector.
 
                      
