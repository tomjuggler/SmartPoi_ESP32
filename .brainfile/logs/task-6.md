---
id: task-6
title: Clean up commented includes and declarations in Globals.h
column: todo
position: 5
description: |-
  Clean up commented code in include/Globals.h:
  1. Remove commented includes (lines 17-24)
  2. Remove commented extern declarations (lines 17-24)
  3. Remove commented `auxillary` variable (line 58)
  4. Remove commented function declarations (lines 97-99)
  5. Clean up commented constant (lines 31-32)
  Need to verify which includes are actually needed and uncomment them if necessary.
priority: medium
tags:
  - cleanup
  - commented-code
  - header
createdAt: "2026-03-09T06:58:25.595Z"
contract:
  status: ready
  deliverables:
    - type: file
      path: include/Globals.h
      description: Clean up all commented includes and declarations
  validation:
    commands:
      - pio run
  constraints:
    - Do not change functionality
    - Uncomment needed includes
    - Remove dead code
---

## Description
Clean up commented code in include/Globals.h:
1. Remove commented includes (lines 17-24)
2. Remove commented extern declarations (lines 17-24)
3. Remove commented `auxillary` variable (line 58)
4. Remove commented function declarations (lines 97-99)
5. Clean up commented constant (lines 31-32)
Need to verify which includes are actually needed and uncomment them if necessary.
