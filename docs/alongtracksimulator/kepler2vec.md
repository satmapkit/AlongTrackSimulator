---
layout: default
title: kepler2vec
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 19
mathjax: true
---

#  kepler2vec

KEPLER2  Vectorized solution of Kepler's equation: M = E - e*sin(E)


---

## Discussion
E = kepler2(M, e) solves for the eccentric anomaly E given mean anomaly M
    and eccentricity e.  M and e may be arrays of the same size.
