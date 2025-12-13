---
layout: default
title: writeTimeStepToNetCDFFile
parent: WVModelOutputGroupAlongTrack
grand_parent: Classes
nav_order: 12
mathjax: true
---

#  writeTimeStepToNetCDFFile

Override the behavior of the superclass.


---

## Discussion
When we reach a time point where the model stops, we will
  actually write all the time points for the passover. The
  incrementsWrittenToGroup will accurately reflect the length
  of the time dimension, but timeOfLastIncrementWrittenToGroup
  will be the time at which the model stopped.
