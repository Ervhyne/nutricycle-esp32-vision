
# NutriCycle AI Model Detector Plan

## PHASE 1 — Define Detection Requirements
- Detect non-vegetable waste (plastic, metal, paper, unknown).
- Classify vegetable vs non-vegetable with ≥80% confidence.
- Real-time detection from ESP32-CAM.
- Support bounding boxes and dashboards.
- Log all detections per batch.

### Target Classes
- Leafy Vegetables  
- Plastic  
- Metal  
- Paper  
- Unknown  

---

## PHASE 2 — Dataset Strategy

### Sources
- ESP32-CAM captured images.
- Web-scraped waste and vegetable images.
- Real NutriCycle machine waste samples.

### Dataset Size Goals
| Class | Minimum | Goal |
|-------|---------|------|
| Leafy Vegetables | 200–400 | 800+ |
| Plastic | 200–300 | 600+ |
| Metal | 150–300 | 500+ |
| Paper | 150–300 | 500+ |

### Augmentations
- Brightness  
- Rotation  
- Flips  
- Noise  
- Blur  

---

## PHASE 3 — Model Selection

### Recommended
- **YOLOv8n (nano model)**

### Alternatives
- YOLOv8s  
- YOLOv11n  

### Reasons
- Fast CPU inference  
- Lightweight  
- Accurate bounding boxes  

---

## PHASE 4 — Training Pipeline

### Steps
1. Prepare dataset in Roboflow.  
2. Label bounding boxes.  
3. Export YOLOv8 format.  
4. Train the model:

```bash
yolo detect train data=data.yaml model=yolov8n.pt epochs=50 imgsz=640
```

### Outputs
- `best.pt` model  
- mAP charts  
- Confusion matrix  

---

## PHASE 5 — Testing & Validation

### Test Scenarios
- Single object  
- Multiple objects  
- Low/bright lighting  
- Transparent plastics  
- Conveyor simulation  

### Metrics
- mAP@50 ≥ 0.80  
- Precision ≥ 0.85  
- Recall ≥ 0.75  
- Latency <150 ms  

---

## PHASE 6 — Deployment (Desktop)

### Pipeline
- ESP32-CAM streams video.  
- Desktop Python YOLO inference.  
- Flask server streams annotated MJPEG.  
- Dashboard displays live output.  

---

## PHASE 7 — Dashboard Monitoring

### UI Shows
- Live annotated video  
- Object labels and confidence  
- Class counts  
- Batch logs  

### Backend
- Flask API  
- `/video_feed` for MJPEG stream  

---

## PHASE 8 — Logging & Analytics

### Log Details
- Timestamp  
- Object type  
- Confidence  
- Frame ID  
- Batch ID  

### Storage Options
- JSON logs  
- SQLite  
- Firestore  

---

## PHASE 9 — Optimization

### Improve Accuracy
- More dataset images  
- Balance classes  
- Increase epochs  
- Try YOLOv8s  

### Improve Speed
- Lower `imgsz` to 416  
- Use YOLOv8n  
- Optimize preprocessing  

---

## PHASE 10 — Future Expansion

- Add hardware diverter control.  
- ESP32 receives divert commands.  
- Add MQTT or HTTP communication.  
- Integrate with NutriCycle backend dashboard.  
- Add analytics and batch reporting.  

---

## Summary
This plan enables:
- Training  
- Deployment  
- Real-time inference  
- Dashboard monitoring  
- Logging  
- Future hardware integration  
