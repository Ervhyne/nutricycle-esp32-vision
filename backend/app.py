"""
NutriCycle Backend - Detection Pipeline
Flask backend for real-time waste detection with webcam
"""

from flask import Flask, Response, jsonify, render_template
from flask_cors import CORS
import cv2
import numpy as np
import threading
import time
from datetime import datetime
from detection.detector import WasteDetector, WebcamCapture, ESP32StreamCapture
from detection.logger import DetectionLogger
try:
    from config import *
except ImportError:
    # Default configuration if config.py doesn't exist
    USE_ESP32 = True
    ESP32_STREAM_URL = "http://192.168.1.17/stream"
    MODEL_PATH = 'backend/models/best.pt'
    CONFIDENCE_THRESHOLD = 0.5
    FILTER_NON_VEGETABLES = True
    MAX_BATCH_HISTORY = 10
    HOST = '0.0.0.0'
    PORT = 5000
    DEBUG = False

app = Flask(__name__)
CORS(app)  # Enable CORS for frontend communication

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
batch_history = []  # Store last N batches

def initialize_system():
    """Initialize detector, stream capture, and logger"""
    global detector, stream_capture, logger
    
    print("Initializing NutriCycle Detection System...")
    print(f"Configuration: ESP32={USE_ESP32}, URL={ESP32_STREAM_URL if USE_ESP32 else 'N/A'}")
    
    # Initialize detector with pretrained model (will use custom model when available)
    detector = WasteDetector(model_path=MODEL_PATH, conf_threshold=CONFIDENCE_THRESHOLD)
    
    # Initialize stream capture (ESP32 or Webcam)
    if USE_ESP32:
        print(f"Using ESP32-CAM stream at {ESP32_STREAM_URL}")
        stream_capture = ESP32StreamCapture(stream_url=ESP32_STREAM_URL)
        try:
            stream_capture.start()
        except Exception as e:
            print(f"Failed to connect to ESP32: {e}")
            print("Falling back to webcam...")
            stream_capture = WebcamCapture(camera_index=0)
            stream_capture.start()
    else:
        print("Using webcam")
        stream_capture = WebcamCapture(camera_index=0)
        stream_capture.start()
    
    # Initialize logger
    logger = DetectionLogger(log_dir='logs')
    
    print("System initialized successfully!")

def detection_loop():
    """Main detection loop running in background thread"""
    global output_frame, lock, detection_active, frame_count, detector, stream_capture, logger
    global batch_active, batch_detections
    
    error_frame = None  # Cache error frame
    skip_frames = 14  # Process every 15th frame for better FPS on older CPUs
    frame_counter = 0
    last_annotated_frame = None
    
    while detection_active:
        # Read frame from stream
        success, frame = stream_capture.read()
        
        if not success:
            print("Failed to read frame from stream")
            
            # Create error frame with reconnection message
            if error_frame is None:
                error_frame = create_error_frame("ESP32 Connection Lost", "Reconnecting...")
            
            with lock:
                output_frame = error_frame.copy()
            
            time.sleep(1)
            continue
        
        # Clear error frame on successful read
        error_frame = None
        
        # Only run detection every Nth frame to improve FPS
        if frame_counter % (skip_frames + 1) == 0:
            # Reset statistics before each detection to show current frame only (not cumulative)
            detector.reset_statistics()
            
            # Resize frame for faster inference (smaller = faster on i5-3470)
            inference_frame = cv2.resize(frame, (320, 320))
            
            # Run detection (filter based on configuration)
            annotated_inference, detections = detector.detect(inference_frame, filter_non_vegetables=FILTER_NON_VEGETABLES)
            
            # Scale detections back to original frame size
            annotated_frame = cv2.resize(annotated_inference, (frame.shape[1], frame.shape[0]))
            last_annotated_frame = annotated_frame
            
            # Log detections if any found
            if detections:
                logger.log_detection(detections, frame_id=frame_count)
                
                # Add to batch if active
                if batch_active:
                    batch_detections.extend(detections)
        else:
            # Use last annotated frame or raw frame
            annotated_frame = last_annotated_frame if last_annotated_frame is not None else frame
        
        # Update output frame
        with lock:
            output_frame = annotated_frame.copy()
        
        frame_count += 1
        frame_counter += 1
        
        # Small delay to prevent CPU overload
        time.sleep(0.05)  # ~20 FPS target


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

def generate_frames():
    """Generator function for video streaming"""
    global output_frame, lock
    
    while True:
        with lock:
            if output_frame is None:
                continue
            
            # Encode frame as JPEG
            ret, buffer = cv2.imencode('.jpg', output_frame)
            
            if not ret:
                continue
            
            frame_bytes = buffer.tobytes()
        
        # Yield frame in multipart format
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/')
def index():
    """API status endpoint"""
    return jsonify({
        "status": "NutriCycle Backend Running",
        "detection_active": detection_active,
        "frame_count": frame_count
    })

@app.route('/video_feed')
def video_feed():
    """Video streaming endpoint"""
    return Response(generate_frames(),
                   mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/start_detection', methods=['POST'])
def start_detection():
    """Start detection process"""
    global detection_active
    
    if detection_active:
        return jsonify({"status": "already_running"})
    
    # Reset statistics when starting to avoid cumulative counting
    if detector:
        detector.reset_statistics()
    
    detection_active = True
    
    # Start detection thread
    detection_thread = threading.Thread(target=detection_loop, daemon=True)
    detection_thread.start()
    
    return jsonify({"status": "started"})

@app.route('/stop_detection', methods=['POST'])
def stop_detection():
    """Stop detection process"""
    global detection_active
    
    detection_active = False
    
    # Save logs
    if logger:
        logger.save()
    
    return jsonify({"status": "stopped"})

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

@app.route('/end_batch', methods=['POST'])
def end_batch():
    """End current batch and generate summary"""
    global batch_active, batch_start_time, batch_detections, batch_history
    
    if not batch_active:
        return jsonify({"status": "no_active_batch"})
    
    batch_end_time = datetime.now()
    duration = (batch_end_time - batch_start_time).total_seconds()
    
    # Generate batch summary
    contamination_types = {}
    max_confidence = 0
    total_items = len(batch_detections)
    
    for detection in batch_detections:
        waste_type = detection['type']
        contamination_types[waste_type] = contamination_types.get(waste_type, 0) + 1
        max_confidence = max(max_confidence, detection['confidence'])
    
    batch_summary = {
        'batch_id': len(batch_history) + 1,
        'start_time': batch_start_time.isoformat(),
        'end_time': batch_end_time.isoformat(),
        'duration': round(duration, 2),
        'total_items': total_items,
        'contamination_types': contamination_types,
        'max_confidence': round(max_confidence, 3)
    }
    
    # Add to history (keep last N based on config)
    batch_history.append(batch_summary)
    if len(batch_history) > MAX_BATCH_HISTORY:
        batch_history.pop(0)
    
    # Reset batch state
    batch_active = False
    batch_start_time = None
    batch_detections = []
    
    print(f"Batch ended. Duration: {duration:.2f}s, Items: {total_items}")
    
    return jsonify({
        "status": "ended",
        "summary": batch_summary
    })

@app.route('/batch_history')
def get_batch_history():
    """Get batch history (last 10 batches)"""
    return jsonify({
        "batches": batch_history,
        "active_batch": batch_active,
        "current_batch_items": len(batch_detections) if batch_active else 0
    })

@app.route('/stream_status')
def get_stream_status():
    """Get current stream status and metrics"""
    if stream_capture is None:
        return jsonify({"error": "Stream not initialized"})
    
    # Get status from stream capture
    if hasattr(stream_capture, 'get_status'):
        status = stream_capture.get_status()
    else:
        # Webcam doesn't have status, provide basic info
        status = {
            'connected': stream_capture.is_open,
            'fps': 30,
            'latency': 0,
            'quality': 'N/A',
            'reconnect_attempts': 0
        }
    
    # Add contamination count
    contamination_count = sum([
        detector.detection_counts.get('plastic', 0),
        detector.detection_counts.get('metal', 0),
        detector.detection_counts.get('paper', 0),
        detector.detection_counts.get('unknown', 0)
    ]) if detector else 0
    
    status['contamination_detected'] = contamination_count > 0
    status['contamination_count'] = contamination_count
    
    return jsonify(status)

def cleanup():
    """Cleanup resources on shutdown"""
    global detection_active, stream_capture, logger
    
    print("Shutting down...")
    detection_active = False
    
    if stream_capture:
        stream_capture.stop()
    
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
