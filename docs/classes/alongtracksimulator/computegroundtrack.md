---
layout: default
title: computeGroundTrack
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 10
mathjax: true
---

#  computeGroundTrack

computes the ground track of a satellite given including the nodal precession (unoptimized version)


---

## Declaration
```matlab
 [lat, lon] = computeGroundTrack(semi_major_axis, e, incl, RAAN, omega, M0, t)
```
## Parameters
+ `semi_major_axis`  Semi-major axis [km]
+ `e`  Eccentricity (0 for circular, >0 for elliptical)
+ `incl`  Inclination [degrees]
+ `RAAN_0`  Right Ascension of Ascending Node [rad]
+ `argPerigee`  Argument of perigee [rad]
+ `M0`  Initial mean anomaly at t = 0 [rad]
+ `t`  Time vector [s]

## Returns
+ `lat`  Latitude (deg) vector corresponding to times in t
+ `lon`  Longitude (deg) vector corresponding to times in t

## Discussion

                      
