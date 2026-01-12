# NutriCycle Backend

Desktop detection pipeline using YOLOv8 and OpenCV with webcam support.

## Quick Start

1. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

2. Run the server:
   ```bash
   python app.py
   ```

3. Open frontend (in browser):
   - Open `frontend/index.html` directly, or
   - Serve frontend: `cd ../frontend && python -m http.server 8080`

## Features

- ✅ Real-time webcam object detection
- ✅ YOLOv8 inference pipeline
- ✅ MJPEG video streaming
- ✅ Detection statistics and logging
- ✅ REST API endpoints
- ✅ CORS enabled for frontend

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Server status |
| `/video_feed` | GET | Live MJPEG stream |
| `/start_detection` | POST | Start detection |
| `/stop_detection` | POST | Stop detection |
| `/statistics` | GET | Get detection stats |
| `/reset_statistics` | POST | Reset counters |

## Structure

- `app.py` - Flask server with video streaming
- `detection/detector.py` - YOLOv8 detection engine
- `detection/logger.py` - Detection event logging
- `models/` - Place trained models here (e.g., `best.pt`)
- `logs/` - Detection logs (auto-created)

## Model Setup

### Using Pretrained Model (Default)
The system uses YOLOv8n pretrained model by default for testing.

### Using Custom Model
1. Train your model (see `scripts/train_yolo.py`)
2. Place `best.pt` in `models/` directory
3. Detector will automatically use it

## Detection Categories

- 🌿 Leafy Vegetables (Green)
- 🔴 Plastic (Red)
- 🟠 Metal (Orange)
- 🟡 Paper (Yellow)
- ⚪ Unknown (Gray)

## Logging

Detection logs are saved in `logs/` directory:
- Format: `detections_YYYYMMDD_HHMMSS.json`
- Contains timestamps, object types, confidence scores, and bounding boxes

## ESP32 Integration (Modes)

The server can operate in two modes:

- **Uploader mode** (recommended and now default): Python will run in uploader-only mode by default and will not attempt to connect directly to ESP32 or webcam. Disable `AUTO_CONNECT_ESP32` in `config.py` (or environment) to keep this behavior. In this mode devices POST frames to the Node gateway (`/upload`) and Node forwards frames to Python's `/detect` endpoint; Python performs inference and returns JSON metadata. This avoids direct ESP32 pulls and aligns with the gateway/uploader architecture.
- **Auto-ESP32 mode** (optional): If you explicitly enable `USE_ESP32 = True` and `AUTO_CONNECT_ESP32 = True` in `config.py`, Python will attempt to open the ESP32 MJPEG stream at startup.

To switch to uploader mode, set `AUTO_CONNECT_ESP32 = False` in your `config.py` or export it as an environment variable before starting `app.py`.

## Troubleshooting

**Camera not working?**
- Close other apps using webcam
- Try different camera index: `WebcamCapture(camera_index=1)`

**Import errors?**
- Ensure all dependencies installed: `pip install -r requirements.txt`
- Check Python version: 3.8+
