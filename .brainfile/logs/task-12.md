---
id: task-12
title: Create POV display task with microsecond timing
description: Implement the POV display task that runs at high priority (PRIO 5) with microsecond-accurate timing using ets_delay_us. The task must decompress 3-3-2 bit-packed images, use shadow buffering to minimize mutex hold time, and include esp_task_wdt_reset() calls. Use Context7 MCP to research ESP32-C3 microsecond timing and task watchdog.
priority: critical
tags:
  - freertos
  - pov
  - timing
  - led
  - esp32-c3
assignee: engineer
createdAt: "2026-03-15T18:49:34.658Z"
contract:
  status: delivered
  deliverables:
    - type: file
      path: src/main.cpp
      description: Implement povDisplayTask function
    - type: file
      path: include/Globals.h
      description: Add task handle declarations
    - type: file
      path: src/main.cpp
      description: Add task creation in setup()
  validation:
    commands:
      - grep -r "povDisplayTask" src/
      - grep -r "ets_delay_us" src/
      - grep -r "esp_task_wdt_reset" src/
  constraints:
    - Must use ets_delay_us for microsecond timing
    - Must implement 3-3-2 bit decompression
    - Must use shadow buffering to minimize mutex hold time
    - Must include task watchdog reset
    - Must run at priority 5 per blueprint
  metrics:
    pickedUpAt: "2026-03-15T19:22:04.203Z"
    reworkCount: 0
    deliveredAt: "2026-03-15T19:22:55.768Z"
    duration: 52
completedAt: "2026-03-15T19:38:16.396Z"
updatedAt: "2026-03-15T19:38:16.396Z"
---

## Description
Implement the POV display task that runs at high priority (PRIO 5) with microsecond-accurate timing using ets_delay_us. The task must decompress 3-3-2 bit-packed images, use shadow buffering to minimize mutex hold time, and include esp_task_wdt_reset() calls. Use Context7 MCP to research ESP32-C3 microsecond timing and task watchdog.
