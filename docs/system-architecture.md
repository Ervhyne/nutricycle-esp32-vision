# System Architecture

## Overview

NutriCycle is a food waste detection system using ESP32-S3 camera and YOLO object detection.

## Components

1. **ESP32-S3 Camera Module** - Captures images of food waste
2. **Backend Server** - Processes images using YOLO for detection
3. **Frontend Dashboard** - Displays detection results and analytics

## Data Flow

```
ESP32-S3 Camera → Backend (YOLO Detection) → Frontend Dashboard
```
