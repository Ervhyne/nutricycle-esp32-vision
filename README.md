# NutriCycle ESP32 Vision

A real-time food waste detection and classification system using YOLOv8 object detection.

## 🚀 Quick Start (Webcam Mode - Working Now!)

### 1. Install Dependencies
```bash
python setup.py
```

### 2. Start the System
**Windows:** Double-click `start.ps1`

**Manual:**
```bash
cd backend
python app.py
```

### 3. Open Dashboard
Open `frontend/index.html` in your browser and click "Start Detection"

📖 **Full instructions:** See [GET_STARTED.md](GET_STARTED.md)

---

## ✨ Features

✅ **Real-time object detection** with YOLOv8
✅ **Webcam support** for desktop testing
✅ **Live video streaming** dashboard
✅ **Detection categories:** Leafy Vegetables, Plastic, Metal, Paper
✅ **Statistics tracking** and logging
✅ **ESP32-CAM ready** architecture

---

## 🎯 Current Status

| Component | Status | Notes |
|-----------|--------|-------|
| Backend Detection | ✅ Working | Flask + YOLOv8 + OpenCV |
| Webcam Capture | ✅ Working | Using desktop webcam |
| Frontend Dashboard | ✅ Working | HTML + JavaScript |
| Detection Logging | ✅ Working | JSON logs in `backend/logs/` |
| ESP32 Code | ✅ Ready | Prepared in `esp32/` folder |
| ESP32 Hardware | ⏳ Pending | Waiting for replacement |
| Custom Model | 📝 Next Step | Need to train with dataset |

---

## 📁 Project Structure

```
nutricycle-esp32-vision/
│
├── backend/                    # Detection server
│   ├── app.py                  # Flask API server
│   ├── detection/
│   │   ├── detector.py        # YOLOv8 detector
│   │   └── logger.py          # Detection logger
│   ├── models/                # Place trained models here
│   └── logs/                  # Auto-generated logs
│
├── frontend/                  # Web dashboard
│   ├── index.html            # Main interface
│   └── js/main.js            # Frontend logic
│
├── esp32/                     # ESP32-CAM firmware
│   ├── src/main.cpp          # Camera stream code
│   └── platformio.ini        # PlatformIO config
│
├── scripts/                   # Training utilities
│   ├── train_yolo.py         # Model training
│   ├── collect_dataset.py    # Dataset collection
│   └── utils.py              # Helper functions
│
├── docs/                      # Documentation
│   ├── NutriCycle_AI_Model_Detector_Plan.md
│   └── system-architecture.md
│
├── start.ps1                  # Windows startup script
├── setup.py                   # Installation script
├── GET_STARTED.md            # User guide
└── QUICKSTART.md             # Detailed setup guide
```

---

## 🎥 How It Works

1. **Video Capture** - Webcam (or ESP32-CAM) streams video
2. **Detection** - YOLOv8 identifies objects in each frame
3. **Classification** - Objects mapped to waste categories
4. **Visualization** - Bounding boxes drawn on video feed
5. **Streaming** - MJPEG stream sent to frontend
6. **Statistics** - Real-time counts and logging

---

## 🔧 API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Server status |
| `/video_feed` | GET | MJPEG video stream |
| `/start_detection` | POST | Start detection |
| `/stop_detection` | POST | Stop detection |
| `/statistics` | GET | Get detection counts |
| `/reset_statistics` | POST | Reset counters |

---

## 📊 Training Custom Model

The system currently uses a pretrained YOLOv8n model. To improve accuracy:

### 1. Collect Dataset
```bash
python scripts/collect_dataset.py
```

### 2. Label Images
Use Roboflow, CVAT, or LabelImg with these classes:
- `leafy_vegetables`
- `plastic`
- `metal`
- `paper`

### 3. Train Model
```bash
python scripts/train_yolo.py
```

### 4. Deploy Model
Place `best.pt` in `backend/models/` and restart server.

---

## 🔌 ESP32-CAM Integration (Future)

When you receive the new ESP32-CAM:

1. **Flash firmware** from `esp32/` folder
2. **Get stream URL** (e.g., `http://192.168.1.100/stream`)
3. **Update detector** in `backend/detection/detector.py`:
   ```python
   # Replace WebcamCapture with ESP32 stream
   cap = cv2.VideoCapture('http://ESP32_IP/stream')
   ```

No frontend changes needed - the architecture is already prepared!

---

## 📸 Detection Categories

| Category | Color | Description |
|----------|-------|-------------|
| 🌿 Leafy Vegetables | Green | Vegetable waste (compostable) |
| 🔴 Plastic | Red | Non-biodegradable plastic waste |
| 🟠 Metal | Orange | Metal waste for recycling |
| 🟡 Paper | Yellow | Paper waste for recycling |
| ⚪ Unknown | Gray | Unclassified objects |

---

## 🛠️ Requirements

- **Python:** 3.8+
- **Webcam:** Built-in or USB
- **RAM:** 4GB minimum
- **OS:** Windows, macOS, or Linux

---

## 📝 Logs

Detection events are automatically logged:
- **Location:** `backend/logs/`
- **Format:** `detections_YYYYMMDD_HHMMSS.json`
- **Contains:** Timestamps, object types, confidence scores, bounding boxes

---

## 🐛 Troubleshooting

**Camera not working?**
- Close other apps using the webcam
- Try different camera index in `app.py`

**Connection errors?**
- Ensure Flask server is running on port 5000
- Check firewall settings

**Poor accuracy?**
- Train custom model with your specific objects
- Improve lighting conditions
- Adjust confidence threshold

See [GET_STARTED.md](GET_STARTED.md) for detailed troubleshooting.

---

## 📚 Documentation

- [GET_STARTED.md](GET_STARTED.md) - User guide and tips
- [QUICKSTART.md](QUICKSTART.md) - Detailed setup guide
- [docs/NutriCycle_AI_Model_Detector_Plan.md](docs/NutriCycle_AI_Model_Detector_Plan.md) - AI model plan
- [backend/README.md](backend/README.md) - Backend details

---

## 🎯 Roadmap

- [x] Backend detection service
- [x] Webcam integration
- [x] Frontend dashboard
- [x] Detection logging
- [x] ESP32 code preparation
- [ ] Collect custom dataset
- [ ] Train custom YOLOv8 model
- [ ] ESP32-CAM hardware integration
- [ ] Hardware diverter control
- [ ] Cloud analytics dashboard

---

## 📄 License

MIT License
