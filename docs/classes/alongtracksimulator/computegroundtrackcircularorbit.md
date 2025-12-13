---
layout: default
title: computeGroundTrackCircularOrbit
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 6
mathjax: true
---

#  computeGroundTrackCircularOrbit

computes the ground track assuming a circular orbit


---

## Declaration
```matlab
 [lat, lon] = computeGroundTrackCircularOrbit(semi_major_axis, e, incl, RAAN_0, omega, M0, t)
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

                      
