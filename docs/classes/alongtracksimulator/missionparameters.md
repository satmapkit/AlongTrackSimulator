---
layout: default
title: missionParameters
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 22
mathjax: true
---

#  missionParameters

Mission parameter dictionary keyed by mission abbreviation.


---

## Discussion

  A dictionary (string -> struct) describing orbit geometry and metadata
  for supported altimetry missions. The constructor loads a default catalog and
  may adjust some parameters (e.g., semi-major axis) to enforce exact repeat cycles.
 
  
