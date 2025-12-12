# 🚀 Getting Started - NutriCycle Detection System

## What's Ready Now

✅ **Webcam-based detection system**
✅ **Real-time YOLOv8 object detection**
✅ **Live video streaming dashboard**
✅ **Detection statistics and logging**
✅ **ESP32 integration ready architecture**

---

## Installation (First Time Only)

### Option 1: Automatic Setup
```powershell
python setup.py
```

### Option 2: Manual Setup
```powershell
cd backend
pip install -r requirements.txt
```

---

## Running the System

### Windows (Easy Way)
Double-click `start.ps1` in Windows Explorer

### Manual Start
```powershell
# Terminal 1 - Backend Server
cd backend
python app.py

# Terminal 2 (Optional) - Frontend Server
cd frontend
python -m http.server 8080
```

Then open `frontend/index.html` in your browser (or `http://localhost:8080`)

---

## Using the Dashboard

1. **Open the webpage** - You'll see the NutriCycle dashboard
2. **Click "Start Detection"** - Your webcam will activate
3. **Show objects** - Point camera at vegetables, plastic, metal, or paper
4. **View results** - See bounding boxes and statistics update in real-time
5. **Click "Stop Detection"** - Stop when done

---

## Current Model Behavior

The system currently uses **YOLOv8n pretrained model** (COCO dataset):
- Maps common objects to waste categories
- Works for testing but has limited accuracy
- Examples: broccoli → leafy vegetables, bottle → plastic

### Detected Items (Temporary Mapping)
| Show This | Detected As |
|-----------|-------------|
| Broccoli, carrots, fruits | Leafy Vegetables |
| Bottles, cups | Plastic |
| Forks, knives, spoons | Metal |
| Books, papers | Paper |
| Other objects | Unknown |

---

## Next Steps to Improve Accuracy

### 1. Collect Your Own Dataset
Use your ESP32-CAM (when ready) or webcam:
```powershell
python scripts/collect_dataset.py
```

### 2. Label Images
- Use [Roboflow](https://roboflow.com), CVAT, or LabelImg
- Label as: `leafy_vegetables`, `plastic`, `metal`, `paper`
- Export in YOLOv8 format

### 3. Train Custom Model
```powershell
python scripts/train_yolo.py
```

### 4. Use Your Model
- Place trained `best.pt` in `backend/models/`
- Restart server - it will auto-load your model!

---

## ESP32-CAM Integration (When Ready)

When you get a new ESP32-CAM:

1. **Upload ESP32 code** from `esp32/` folder
2. **Update detector** to read ESP32 stream:
   ```python
   # In backend/detection/detector.py
   # Replace WebcamCapture with:
   cap = cv2.VideoCapture('http://ESP32_IP/stream')
   ```
3. **No frontend changes needed!**

The architecture is already prepared for this switch.

---

## File Structure

```
nutricycle-esp32-vision/
├── backend/
│   ├── app.py                    # Flask server ⚙️
│   ├── detection/
│   │   ├── detector.py          # YOLOv8 detection 🎯
│   │   └── logger.py            # Event logging 📝
│   ├── models/                   # Place trained models here
│   └── logs/                     # Detection logs (auto-created)
│
├── frontend/
│   ├── index.html               # Dashboard 🖥️
│   └── js/main.js               # Frontend logic
│
├── esp32/
│   └── src/main.cpp             # ESP32 code (for future) 📷
│
├── scripts/
│   ├── collect_dataset.py       # Dataset collection
│   └── train_yolo.py            # Model training
│
├── start.ps1                    # Windows startup script
├── setup.py                     # Installation script
└── QUICKSTART.md               # Detailed guide
```

---

## Troubleshooting

### Backend won't start
- Check Python 3.8+ installed: `python --version`
- Install dependencies: `pip install -r backend/requirements.txt`

### Webcam not detected
- Close other apps using webcam (Zoom, Teams, etc.)
- Try different camera: Edit `app.py`, change `camera_index=0` to `1`

### Frontend can't connect
- Ensure Flask server running on port 5000
- Check CORS is enabled (already configured)
- Try opening developer console (F12) for errors

### Poor detection quality
- Expected with pretrained model
- Train custom model with your specific objects
- Improve lighting conditions
- Adjust confidence threshold in detector

---

## Tips for Best Results

🎯 **Good lighting** - Bright, even lighting works best
📐 **Object positioning** - Place objects clearly in view
🔄 **Multiple angles** - Show objects from different sides
📊 **Check statistics** - Monitor detection counts in real-time
💾 **Save logs** - Logs auto-saved when stopping detection

---

## Support

- 📖 Read `QUICKSTART.md` for detailed setup
- 📁 Check `docs/` folder for architecture details
- 🔧 Modify code as needed - it's designed to be flexible

---

**You're all set!** 🎉

Start the backend server and open the frontend to begin detecting waste objects!
