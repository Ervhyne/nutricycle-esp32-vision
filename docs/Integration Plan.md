## Plan: ESP32-CAM Integration with Non-Vegetable Item Tracking

Connect ESP32-CAM at `http://192.168.1.17/stream` to backend for real-time detection, display ALL non-vegetable items with bounding boxes showing confidence scores (CCTV-style tracking), auto-clearing visual alerts, API-triggered batch records, adaptive stream quality, and auto-reconnection.

### Steps

1. **Create ESP32 stream capture with auto-reconnection** - Add `ESP32StreamCapture` class in detector.py using `cv2.VideoCapture("http://192.168.1.17/stream")` with `.read()` interface, implement automatic reconnection on connection loss with retry logic, track FPS and latency metrics for quality monitoring

2. **Add adaptive stream quality management** - Implement latency monitoring in `ESP32StreamCapture` that detects when lag exceeds 2 seconds, automatically send control commands to ESP32 at `/control?var=framesize&val=<lower>` to reduce resolution (UXGA→SVGA→VGA→QVGA), restore quality when latency normalizes

3. **Filter and display all non-vegetable items with confidence** - Modify `detect()` method in detector.py to filter `type != 'leafy_vegetables'`, draw ALL non-veg bounding boxes with labels `{class_name} {confidence:.0%}`, return clean video when no contaminants present

4. **Implement API-triggered batch tracking system** - Add batch management in app.py to accumulate contamination detections during active batch, create `POST /end_batch` endpoint to finalize current batch and generate summary record (timestamp, duration, contamination types, total count, max confidence), store last 10 batches in memory, reset current batch counters

5. **Add contamination alert banner with network status** - Create alert banner in index.html showing "⚠️ CONTAMINATION DETECTED" (auto-hides when clear), add stream status indicator (FPS, latency, connection state) in corner, update main.js to poll `/statistics` and `/stream_status` every 1 second for real-time updates

6. **Create batch history panel and endpoints** - Add batch history section in index.html displaying last 10 batch records with columns (Batch ID, Start Time, Duration, Items Detected, Types), implement `GET /batch_history` endpoint in app.py returning metadata-only records, update main.js to fetch and display batch history table
