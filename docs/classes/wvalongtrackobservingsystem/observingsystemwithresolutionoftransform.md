---
layout: default
title: observingSystemWithResolutionOfTransform
parent: WVAlongTrackObservingSystem
grand_parent: Classes
nav_order: 13
mathjax: true
---

#  observingSystemWithResolutionOfTransform

create a new WVObservingSystem with a new resolution


---

## Declaration
```matlab
 os = observingSystemWithResolutionOfTransform(self,wvtX2)
```
## Parameters
+ `wvtX2`  the WVTransform with increased resolution

## Returns
+ `force`  a new instance of WVObservingSystem

## Discussion

  Subclasses to should override this method an implement the
  correct logic.
 
        
