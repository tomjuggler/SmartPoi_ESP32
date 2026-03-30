---
id: task-14
title: Modify web server to run as separate FreeRTOS task
description: Adapt the existing web server (ElegantOTA) to run as a separate FreeRTOS task at priority 3. Ensure the server can handle HTTP requests concurrently with POV display and file operations. Use Context7 MCP to research AsyncWebServer integration with FreeRTOS tasks.
priority: medium
tags:
  - freertos
  - webserver
  - async
  - elegantota
  - esp32-c3
assignee: engineer
createdAt: "2026-03-15T18:50:08.167Z"
contract:
  status: delivered
  deliverables:
    - type: file
      path: src/tasks.cpp
      description: Modify elegantOTATask to run as FreeRTOS task
    - type: file
      path: include/tasks.h
      description: Update task function declarations
    - type: file
      path: src/main.cpp
      description: Update web server task initialization
  validation:
    commands:
      - grep -r "elegantOTATask" src/
      - grep -r "xTaskCreate.*WEB_TASK_PRIO" src/
      - grep -r "AsyncWebServer.*task" src/
  constraints:
    - Must run at priority 3 per blueprint
    - Must maintain existing ElegantOTA functionality
    - Must handle HTTP requests concurrently
    - Must not block POV display task
  metrics:
    pickedUpAt: "2026-03-15T19:24:23.567Z"
    reworkCount: 0
    deliveredAt: "2026-03-15T19:28:59.094Z"
    duration: 276
completedAt: "2026-03-15T19:38:28.178Z"
updatedAt: "2026-03-15T19:38:28.178Z"
---

## Description
Adapt the existing web server (ElegantOTA) to run as a separate FreeRTOS task at priority 3. Ensure the server can handle HTTP requests concurrently with POV display and file operations. Use Context7 MCP to research AsyncWebServer integration with FreeRTOS tasks.
