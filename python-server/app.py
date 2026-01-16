"""
NutriCycle Backend - Detection Pipeline
Flask backend for real-time waste detection with webcam
"""

from flask import Flask, Response, jsonify, request
from flask_cors import CORS
import cv2
import numpy as np
import threading
import time
from datetime import datetime
from detection.detector import WasteDetector
from detection.logger import DetectionLogger

# Config (with sensible defaults if config.py missing)
try:
    from config import *
except Exception:
    # Default to uploader-only/detection mode (Node gateway handles uploads & streams)
    USE_ESP32 = False
    ESP32_STREAM_URL = None
    MODEL_PATH = 'backend/models/best.pt'
    CONFIDENCE_THRESHOLD = 0.5
    FILTER_NON_VEGETABLES = True
    MAX_BATCH_HISTORY = 10
    HOST = '127.0.0.1'
    PORT = 5000
    DEBUG = False
    # If False, do not fall back to local webcam when ESP32 is unavailable
    FALLBACK_TO_WEBCAM = False
    # If False, do not attempt to auto-connect to ESP32 at startup; rely on uploads instead
    AUTO_CONNECT_ESP32 = False

# Ensure required config vars exist even if config.py omitted some keys
USE_ESP32 = globals().get('USE_ESP32', False)
ESP32_STREAM_URL = globals().get('ESP32_STREAM_URL', None)
MODEL_PATH = globals().get('MODEL_PATH', 'backend/models/best.pt')
CONFIDENCE_THRESHOLD = globals().get('CONFIDENCE_THRESHOLD', 0.25)
FILTER_NON_VEGETABLES = globals().get('FILTER_NON_VEGETABLES', True)
MAX_BATCH_HISTORY = globals().get('MAX_BATCH_HISTORY', 10)
HOST = globals().get('HOST', '127.0.0.1')
PORT = globals().get('PORT', 5000)
DEBUG = globals().get('DEBUG', False)
FALLBACK_TO_WEBCAM = globals().get('FALLBACK_TO_WEBCAM', False)
AUTO_CONNECT_ESP32 = globals().get('AUTO_CONNECT_ESP32', False)

# Create Flask app early so decorators below work
app = Flask(__name__)
CORS(app)

# Removed detection loop and direct stream capture: Python now runs as a detection API only.
# Devices should POST frames to `/detect` (recommended path): Node gateway will forward uploads to this endpoint.

@app.route('/detect', methods=['POST'])
def detect_image():
    """Detect objects from an uploaded image and return JSON metadata.
    Accepts multipart/form-data with `image` file or raw image bytes in the body.
    This handler now includes diagnostics and error handling to help trace 500/no-response cases.
    """
    if detector is None:
        return jsonify({"error": "Detector not initialized"}), 500

    start_time = time.time()
    try:
        # Read image from multipart/form-data (`image`) or raw bytes
        if 'image' in request.files:
            file = request.files['image']
            data = file.read()
        else:
            data = request.get_data()

        # Quick diagnostics
        try:
            req_size = len(data) if data is not None else 0
            client_ip = request.remote_addr or request.headers.get('X-Forwarded-For', 'unknown')
            print(f"[detect] request from {client_ip}, {req_size} bytes")
        except Exception as _:
            pass

        arr = np.frombuffer(data, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)

        if img is None:
            return jsonify({"error": "invalid_image"}), 400

        # Resize to model input size for faster inference
        inference_frame = cv2.resize(img, (320, 320))
        _unused, detections = detector.detect(inference_frame, filter_non_vegetables=FILTER_NON_VEGETABLES, draw=False)

        # Scale bounding boxes back to original image size
        width_scale = img.shape[1] / 320.0
        height_scale = img.shape[0] / 320.0
        scaled = []
        for d in detections:
            x1, y1, x2, y2 = d['bbox']
            sx1 = int(x1 * width_scale)
            sx2 = int(x2 * width_scale)
            sy1 = int(y1 * height_scale)
            sy2 = int(y2 * height_scale)
            d_copy = d.copy()
            d_copy['bbox'] = [sx1, sy1, sx2, sy2]
            scaled.append(d_copy)

        elapsed_ms = int((time.time() - start_time) * 1000)
        print(f"[detect] processed in {elapsed_ms} ms, detections={len(scaled)}, image={img.shape[1]}x{img.shape[0]}")

        return jsonify({
            "detections": scaled,
            "image_width": img.shape[1],
            "image_height": img.shape[0]
        })
    except Exception as e:
        # Log full exception for debugging
        import traceback
        traceback.print_exc()
        elapsed_ms = int((time.time() - start_time) * 1000)
        print(f"[detect] error after {elapsed_ms} ms: {e}")
        return jsonify({"error": "internal_error", "details": str(e)}), 500

# (Flask app already created earlier)  -- duplicate creation removed to preserve registered routes

# Global variables
detector = None
stream_capture = None  # Changed from webcam to support both webcam and ESP32
logger = None
output_frame = None
lock = threading.Lock()
detection_active = False
frame_count = 0

# Batch tracking variables
batch_active = False
batch_start_time = None
batch_detections = []
# batch_history removed

def initialize_system():
    """Initialize detector, stream capture, and logger"""
    global detector, stream_capture, logger
    
    print("Initializing NutriCycle Detection System...")
    print(f"Configuration: ESP32={USE_ESP32}, URL={ESP32_STREAM_URL if USE_ESP32 else 'N/A'}")
    
    # Initialize detector with pretrained model (will use custom model when available)
    detector = WasteDetector(model_path=MODEL_PATH, conf_threshold=CONFIDENCE_THRESHOLD)
    
    # Initialize only detection model & logger — Python runs as a detection API (uploader-only)
    logger = DetectionLogger(log_dir='logs')
    print("System initialized successfully in uploader-only detection mode.")

# detection loop implementation moved earlier to avoid server-side drawing. See top of file for updated `detection_loop()`.


def create_error_frame(title, message):
    """Create an error frame with message"""
    frame = np.zeros((480, 640, 3), dtype=np.uint8)
    
    # Add title
    cv2.putText(frame, title, (120, 200), 
                cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 3)
    
    # Add message
    cv2.putText(frame, message, (200, 280), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
    
    return frame

# Removed MJPEG streaming generator — Python acts as a detection API only. Use POST /detect to submit frames.

@app.route('/')
def index():
    """API status endpoint"""
    return jsonify({
        "status": "NutriCycle Backend Running",
        "mode": "uploader-only",
        "note": "POST images to /detect (gateway should forward uploads)"
    })

# /video_feed removed — Python server is detection API only. Use POST /detect for uploads.

# /start_detection removed — Python runs as a detection API. Use POST /detect for inference.

# /stop_detection removed — not applicable in uploader-only mode.

@app.route('/statistics')
def get_statistics():
    """Get current detection statistics"""
    if detector is None:
        return jsonify({"error": "Detector not initialized"})
    
    stats = detector.get_statistics()
    
    if logger:
        summary = logger.get_summary()
        stats['session_info'] = summary
    
    return jsonify(stats)

@app.route('/reset_statistics', methods=['POST'])
def reset_statistics():
    """Reset detection statistics"""
    if detector:
        detector.reset_statistics()
    
    return jsonify({"status": "reset"})

@app.route('/start_batch', methods=['POST'])
def start_batch():
    """Start a new contamination detection batch"""
    global batch_active, batch_start_time, batch_detections
    
    if batch_active:
        return jsonify({"status": "already_active", "start_time": batch_start_time.isoformat()})
    
    batch_active = True
    batch_start_time = datetime.now()
    batch_detections = []
    
    print(f"Batch started at {batch_start_time}")
    
    return jsonify({
        "status": "started",
        "start_time": batch_start_time.isoformat()
    })

# batch endpoints removed

# batch_history endpoint removed

@app.route('/stream_status')
def get_stream_status():
    """Return a simple status for uploader-only deployments."""
    status = {
        'mode': 'uploader-only',
        'note': 'POST frames to /detect (gateway should forward uploads if in place)',
        'contamination_count': sum([
            detector.detection_counts.get('plastic', 0),
            detector.detection_counts.get('metal', 0),
            detector.detection_counts.get('paper', 0),
            detector.detection_counts.get('unknown', 0)
        ]) if detector else 0
    }
    status['contamination_detected'] = status['contamination_count'] > 0
    return jsonify(status)

def cleanup():
    """Cleanup resources on shutdown"""
    global logger
    print("Shutting down...")
    # Save logs
    if logger:
        logger.save()
    print("Cleanup complete")

if __name__ == '__main__':
    try:
        initialize_system()
        print("\n" + "="*50)
        print("NutriCycle Detection Server Starting")
        print(f"Open http://localhost:{PORT} in your browser")
        print("="*50 + "\n")
        
        app.run(debug=DEBUG, host=HOST, port=PORT, threaded=True)
    except KeyboardInterrupt:
        cleanup()
    except Exception as e:
        print(f"Error: {e}")
        cleanup()
