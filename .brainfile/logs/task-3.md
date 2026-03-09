---
id: task-3
title: Remove duplicate function declaration in tasks.h
column: todo
position: 2
description: Remove duplicate declaration of `checkFileSpace` function in include/tasks.h (declared twice on lines 10 and 16). Keep only one declaration to eliminate redundancy.
priority: high
tags:
  - cleanup
  - duplicate
  - header
createdAt: "2026-03-09T06:57:33.394Z"
contract:
  status: ready
  deliverables:
    - type: file
      path: include/tasks.h
      description: Remove duplicate checkFileSpace declaration
  validation:
    commands:
      - pio run
  constraints:
    - Do not change functionality
    - Keep one valid declaration
    - Ensure compilation still works
---

## Description
Remove duplicate declaration of `checkFileSpace` function in include/tasks.h (declared twice on lines 10 and 16). Keep only one declaration to eliminate redundancy.
