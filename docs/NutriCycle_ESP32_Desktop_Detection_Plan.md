
# NutriCycle ESP32-CAM + Desktop Detection Plan

## PHASE 1 — ESP32-CAM SETUP
- Flash **CameraWebServer** example.
- Configure WiFi credentials.
- Stream URL example:  
  `http://<esp32-ip>:81/stream`

### Hardware Required
- ESP32-CAM  
- FTDI programmer  
- Stable lighting  

---

## PHASE 2 — Desktop YOLO Detection Pipeline
### Install Dependencies
```bash
pip install ultralytics opencv-python flask
```

### Python Detection Script
- Reads ESP32 stream  
- Runs YOLOv8  
- Displays annotated frames  

---

## PHASE 3 — Simple Dashboard (Flask)
- Flask backend streams annotated MJPEG frames.
- HTML frontend shows live detection feed.

### Endpoints
- `/` → Dashboard  
- `/video_feed` → Annotated MJPEG stream  

---

## PHASE 4 — Training Plan
Use **Roboflow** for dataset creation:

Classes:
- Leafy vegetables  
- Plastic  
- Metal  
- Paper  

Export dataset in **YOLOv8 format**.

### Training Command
```bash
yolo detect train data=data.yaml model=yolov8n.pt epochs=50
```

---

## PHASE 5 — Testing

### Test Cases
- Plastic detection  
- Leafy detection  
- Mixed objects  
- Bright/low lighting  
- Continuous streaming stress test  

### Performance Goals
- **10–20 FPS**  
- **<150 ms latency**  

---

## PHASE 6 — Implementation Steps
1. Flash ESP32-CAM  
2. Test video stream  
3. Run YOLO on desktop  
4. Add Flask dashboard  
5. Collect training data  
6. Train the YOLO model  
7. Replace model in backend  
8. Validate accuracy  

---

## PHASE 7 — Future Expansion
- Add servo diverter control via ESP32  
- Add MQTT or HTTP control commands  
- Integrate with NutriCycle backend dashboard  
- Add batch logs and error reporting  

