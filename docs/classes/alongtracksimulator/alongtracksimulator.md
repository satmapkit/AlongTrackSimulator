---
layout: default
title: AlongTrackSimulator
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 1
mathjax: true
---

#  AlongTrackSimulator

Create an AlongTrackSimulator instance.


---

## Declaration
```matlab
 self = AlongTrackSimulator()
```
## Returns
+ `self`  AlongTrackSimulator instance

## Discussion

  Loads the mission parameter catalog and (when applicable) adjusts mission
  parameters so that repeat-cycle missions have an exact repeat by solving for
  a semi-major axis consistent with the requested passes-per-cycle.
 
      
