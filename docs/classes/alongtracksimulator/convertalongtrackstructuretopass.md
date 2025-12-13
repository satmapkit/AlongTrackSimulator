---
layout: default
title: convertAlongTrackStructureToPass
parent: AlongTrackSimulator
grand_parent: Classes
nav_order: 10
mathjax: true
---

#  convertAlongTrackStructureToPass

Convert a flat alongtrack struct into a cell array of passes.


---

## Declaration
```matlab
 tracks = convertAlongTrackStructureToPass(alongtrack)
```
## Parameters
+ `alongtrack`  struct — must contain fields x, y, t

## Returns
+ `tracks`  cell array — each cell contains a struct for one pass

## Discussion

  Splits an alongtrack struct (with fields x, y, t) whenever there is a
  discontinuity in time (diff(t) > 1).
 
        
