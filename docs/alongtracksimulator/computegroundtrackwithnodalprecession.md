---
layout: default
title: computeGroundTrackWithNodalPrecession
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 7
mathjax: true
---

#  computeGroundTrackWithNodalPrecession

COMPUTE_GROUND_TRACK Computes the satellite ground track including nodal precession


---

## Discussion

  Inputs:
    a      - Semi-major axis (km)
    e      - Eccentricity
    i      - Inclination (degrees)
    RAAN_0 - Initial Right Ascension of Ascending Node (degrees)
    omega  - Argument of Perigee (degrees)
    M0     - Initial Mean Anomaly (degrees)
    t      - Time array (seconds)
 
  Outputs:
    lat    - Latitude (degrees)
    lon    - Longitude (degrees)
