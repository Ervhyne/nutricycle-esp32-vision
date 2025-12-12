# ESP32-CAM Integration Setup Guide

## Quick Start

### 1. Configure ESP32 IP Address

Edit `backend/config.py` and update the ESP32 stream URL:
```python
ESP32_STREAM_URL = "http://192.168.1.17/stream"  # Change to your ESP32's IP
```

Or to use webcam instead:
```python
USE_ESP32 = False
```

### 2. Install Additional Dependencies

The ESP32 integration requires the `requests` library for adaptive quality control:
```bash
pip install requests
```

### 3. Start the System

**Windows:**
```powershell
.\start.ps1
```

**Manual:**
```bash
cd backend
python app.py
```

### 4. Open Dashboard

Open `frontend/index.html` in your browser or navigate to `http://localhost:5000`

---

## Features

### Contamination Monitoring
- **Non-Vegetable Detection Only**: System filters and displays only plastic, metal, paper, and unknown items
- **Real-Time Alerts**: Visual banner appears when contamination is detected
- **CCTV-Style Tracking**: Bounding boxes with confidence percentages on all detected contaminants

### ESP32-CAM Features
- **Auto-Reconnection**: Automatically reconnects if ESP32 connection drops
- **Adaptive Quality**: Reduces stream resolution when latency exceeds 2 seconds
- **Network Monitoring**: Real-time FPS, latency, and connection status display

### Batch Tracking
- **Start/End Batch**: Manual batch control via API or frontend buttons
- **Batch Summary**: Automatic generation of metadata (duration, item counts, types)
- **History Panel**: Displays last 10 batch records with detailed information

---

## API Endpoints

### Detection Control
- `POST /start_detection` - Start detection
- `POST /stop_detection` - Stop detection
- `GET /video_feed` - MJPEG video stream
- `GET /statistics` - Get detection statistics

### Batch Management
- `POST /start_batch` - Start new batch tracking
- `POST /end_batch` - End current batch and generate summary
- `GET /batch_history` - Get last 10 batch records

### Monitoring
- `GET /stream_status` - Get ESP32 connection status, FPS, latency, quality
- `POST /reset_statistics` - Reset detection counters

---

## Configuration Options

### `backend/config.py`

```python
# ESP32-CAM Configuration
USE_ESP32 = True  # Set to False to use webcam
ESP32_STREAM_URL = "http://192.168.1.17/stream"

# Detection Configuration
MODEL_PATH = 'backend/models/best.pt'
CONFIDENCE_THRESHOLD = 0.5
FILTER_NON_VEGETABLES = True  # Only show contaminants

# Batch Configuration
MAX_BATCH_HISTORY = 10

# Server Configuration
HOST = '0.0.0.0'
PORT = 5000
DEBUG = False
```

---

## Troubleshooting

### ESP32 Won't Connect
1. Verify ESP32 IP address: `ping 192.168.1.17`
2. Test stream in browser: `http://192.168.1.17/stream`
3. Check firewall settings
4. System will auto-fallback to webcam if ESP32 unavailable

### High Latency Warning
- System automatically reduces quality when latency > 2s
- Check WiFi signal strength
- Move ESP32 closer to router
- Reduce network traffic

### No Detections Showing
- Verify `FILTER_NON_VEGETABLES = True` in config
- System only shows plastic, metal, paper, unknown items
- Vegetables are intentionally filtered out

### Batch Not Starting
- Ensure detection is active first
- Check console for error messages
- Only one batch can be active at a time

---

## ESP32-CAM Stream Endpoints

The ESP32-CAM provides these endpoints:

- `http://192.168.1.17/` - Camera web interface
- `http://192.168.1.17/stream` - MJPEG stream (main)
- `http://192.168.1.17/capture` - Single JPEG capture
- `http://192.168.1.17/status` - Camera sensor status
- `http://192.168.1.17/control?var=framesize&val=X` - Resolution control
  - UXGA=13, SVGA=9, VGA=8, QVGA=6

---

## Frontend Features

### Alert System
- Red banner appears when contamination detected
- Auto-hides when feed is clean
- Pulses for visibility

### Network Status Display
- Connection state (Connected/Disconnected)
- Real-time FPS counter
- Latency monitoring
- Current stream quality level
- Warning color when quality degraded

### Batch Controls
- **Start Batch** button - Begins contamination tracking
- **End Batch** button - Finalizes batch and generates summary
- Buttons enable/disable based on batch state

### Batch History Table
- Shows last 10 batches
- Columns: Batch ID, Start Time, Duration, Items Detected, Types
- Updates automatically every 5 seconds
- Active batch indicator with current item count

---

## Development Notes

### File Structure
```
backend/
├── app.py              # Main Flask application
├── config.py           # Configuration (NEW)
└── detection/
    ├── detector.py     # WasteDetector + ESP32StreamCapture (UPDATED)
    └── logger.py       # Detection logging

frontend/
├── index.html          # Main dashboard (UPDATED)
└── js/
    └── main.js         # Frontend logic (UPDATED)
```

### Key Changes from Webcam Version
1. **Backend**: `WebcamCapture` → `ESP32StreamCapture` with auto-reconnect
2. **Detection**: Filter mode added (non-vegetables only)
3. **Batch System**: API-triggered batch tracking with summaries
4. **Frontend**: Alert banner, network status, batch controls, history panel
5. **Configuration**: Centralized config file for easy customization

---

## Testing

### Test ESP32 Connection
```bash
curl http://192.168.1.17/status
```

### Test Detection API
```bash
# Start detection
curl -X POST http://localhost:5000/start_detection

# Get stream status
curl http://localhost:5000/stream_status

# Start batch
curl -X POST http://localhost:5000/start_batch

# End batch
curl -X POST http://localhost:5000/end_batch

# Get history
curl http://localhost:5000/batch_history
```

---

## Performance Tips

1. **Optimal WiFi**: Keep ESP32 on 2.4GHz with strong signal
2. **Frame Rate**: System targets ~30 FPS, auto-adjusts if laggy
3. **CPU Usage**: Detection runs in separate thread, minimal UI impact
4. **Memory**: Batch history limited to 10 records to prevent memory growth

---

## Next Steps

1. Train custom YOLOv8 model with your specific items
2. Deploy model to `backend/models/best.pt`
3. Update class mappings in `detector.py` if needed
4. Consider adding audio alerts for contamination events
5. Implement database storage for long-term batch history
