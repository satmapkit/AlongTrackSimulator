---
layout: default
title: kepler2vec
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 23
mathjax: true
---

#  kepler2vec

Solve Kepler's equation for eccentric anomaly E (fastest version)


---

## Declaration
```matlab
 E = kepler2vec(M,e)
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
 
          
