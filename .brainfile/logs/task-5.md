---
id: task-5
title: Fix duplicate condition in ColourPalette.cpp
column: todo
position: 4
description: Fix duplicate `if (secondHand == 60)` condition in `ChangePalettePeriodically3()` function in src/ColourPalette.cpp. Both lines 113 and 128 check for the same condition. Need to determine which one is correct or if they should be combined.
priority: high
tags:
  - cleanup
  - duplicate
  - logic
createdAt: "2026-03-09T06:58:07.639Z"
contract:
  status: ready
  deliverables:
    - type: file
      path: src/ColourPalette.cpp
      description: Fix duplicate condition
  validation:
    commands:
      - pio run
  constraints:
    - Do not change functionality
    - Fix duplicate logic
    - Preserve palette behavior
---

## Description
Fix duplicate `if (secondHand == 60)` condition in `ChangePalettePeriodically3()` function in src/ColourPalette.cpp. Both lines 113 and 128 check for the same condition. Need to determine which one is correct or if they should be combined.
