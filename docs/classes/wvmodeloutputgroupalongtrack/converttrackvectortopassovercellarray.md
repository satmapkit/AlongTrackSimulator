---
layout: default
title: convertTrackVectorToPassoverCellArray
parent: WVModelOutputGroupAlongTrack
grand_parent: Classes
nav_order: 4
mathjax: true
---

#  convertTrackVectorToPassoverCellArray

Convert a track time series into a cell array of pass-overs.


---

## Declaration
```matlab
 tracks = convertTrackVectorToPassoverCellArray(alongtrack)
```
## Parameters
+ `alongtrack`  struct — input track with fields:

## Returns
+ `tracks`  cell column vector — pass-over structs with fields x, y, t

## Discussion

  Splits a continuous along-track struct (with fields t, x, y) into individual pass-overs
  using gaps in time. Each output cell contains a struct with fields x, y, and t.
 
          - t: double vector — model time stamps (s)
    - x: double vector — projected x coordinate (m)
    - y: double vector — projected y coordinate (m)
    alongtrack is a struct with fields (t,x,y)
