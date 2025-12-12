# NutriCycle Detection System - Setup Guide

## Quick Start

### 1. Install Dependencies
```bash
cd backend
pip install -r requirements.txt
```

### 2. Run the Backend Server
```bash
python app.py
```

The server will start on `http://localhost:5000`

### 3. Open the Frontend
Simply open `frontend/index.html` in your web browser, or use:
```bash
# Using Python's built-in HTTP server
cd frontend
python -m http.server 8080
```
Then open `http://localhost:8080`

### 4. Start Detection
- Click **"Start Detection"** button in the web interface
- Your webcam will activate and detection will begin
- View real-time results and statistics

## System Requirements

- Python 3.8+
- Webcam (built-in or USB)
- 4GB RAM minimum
- Windows/Mac/Linux

## Features

✅ Real-time object detection using YOLOv8
✅ Webcam video streaming
✅ Detection categories: Leafy Vegetables, Plastic, Metal, Paper
✅ Live statistics dashboard
✅ Detection logging (saved in `backend/logs/`)
✅ CORS enabled for frontend communication

## Model Information

### Current Model
- **YOLOv8n** (pretrained on COCO dataset)
- Temporary mapping of common objects to waste categories
- Works out of the box for testing

### Training Custom Model
To improve accuracy, train a custom model with your own dataset:

1. **Collect Dataset**
   ```bash
   python scripts/collect_dataset.py
   ```

2. **Label Images** (use Roboflow, CVAT, or LabelImg)
   - Export in YOLOv8 format

3. **Train Model**
   ```bash
   python scripts/train_yolo.py
   ```

4. **Replace Model**
   - Place trained model at `backend/models/best.pt`
   - Detector will automatically use it

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Server status |
| `/video_feed` | GET | MJPEG video stream |
| `/start_detection` | POST | Start detection |
| `/stop_detection` | POST | Stop detection |
| `/statistics` | GET | Get detection counts |
| `/reset_statistics` | POST | Reset counters |

## Troubleshooting

### Camera Not Found
- Check if another application is using the webcam
- Try changing camera index in `app.py`: `WebcamCapture(camera_index=1)`

### Connection Refused
- Ensure Flask server is running
- Check firewall settings
- Verify port 5000 is not in use

### Poor Detection Accuracy
- Train a custom model with your specific waste types
- Adjust confidence threshold in detector initialization
- Improve lighting conditions

## ESP32 Integration (Future)

When ESP32-CAM is ready:
1. Replace `WebcamCapture` with ESP32 stream URL
2. Update detector to read from network stream
3. No frontend changes needed!

## File Structure
```
backend/
  ├── app.py                 # Flask server
  ├── detection/
  │   ├── detector.py       # YOLOv8 detector
  │   └── logger.py         # Detection logging
  ├── models/               # Place trained models here
  │   └── best.pt          # Custom trained model
  └── logs/                # Detection logs (auto-created)

frontend/
  ├── index.html           # Main dashboard
  └── js/
      └── main.js          # Frontend logic
```

## Next Steps

1. ✅ Test with webcam
2. 📊 Collect your own dataset images
3. 🎯 Train custom YOLOv8 model
4. 🔧 Prepare ESP32 integration code
5. 🚀 Deploy complete system

---

**Need Help?** Check the documentation in `docs/` folder
