---
layout: default
title: computeGroundTrackCircularOrbit
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 6
mathjax: true
---

#  computeGroundTrackCircularOrbit

computeGroundTrack computes the ground track (latitude and longitude) 


---

## Discussion
of a satellite given the orbital parameters and time vector.
 
  Inputs:
    a         - Semi-major axis [km]
    e         - Eccentricity (0 for circular, >0 for elliptical)
    incl      - Inclination [rad]
    RAAN      - Right Ascension of Ascending Node [rad]
    argPerigee- Argument of perigee [rad]
    M0        - Initial mean anomaly at t = 0 [rad]
    t         - Time vector [s]
    mu        - Earth's gravitational parameter [km^3/s^2]
    Re        - Earth's radius [km]
    omega_e   - Earth's rotation rate [rad/s]
 
  Outputs:
    lat       - Latitude (deg) vector corresponding to times in t
    lon       - Longitude (deg) vector corresponding to times in t
