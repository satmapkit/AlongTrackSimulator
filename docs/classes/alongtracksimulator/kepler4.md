---
layout: default
title: kepler4
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 21
mathjax: true
---

#  kepler4

Solve Kepler's equation for eccentric anomaly E.


---

## Declaration
```matlab
 E = kepler4(M,e)
```
## Parameters
+ `M`  double (scalar or vector) — mean anomaly [rad]
+ `e`  double (scalar) — eccentricity

## Returns
+ `E`  double (same size as M) — eccentric anomaly [rad]

## Discussion

  Given mean anomaly M and eccentricity e, solves M = E - e*sin(E) using
  a particular iterative scheme. Different kepler* variants implement different
  initial guesses and/or update formulas; see code for details.
 
          
