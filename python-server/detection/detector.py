"""
NutriCycle Object Detector
Handles webcam capture and YOLOv8 inference for waste classification
"""

import cv2
import numpy as np
from ultralytics import YOLO
from datetime import datetime
import json
import os
import time

class WasteDetector:
    def __init__(self, model_path='yolov8n.pt', conf_threshold=0.5):
        """
        Initialize the waste detector
        
        Args:
            model_path: Path to YOLOv8 model (uses pretrained by default)
            conf_threshold: Confidence threshold for detections
        """
        self.conf_threshold = conf_threshold
        self.model = None
        self.model_path = model_path
        
        # Detection statistics
        self.detection_counts = {
            'leafy_vegetables': 0,
            'plastic': 0,
            'metal': 0,
            'paper': 0,
            'unknown': 0
        }
        
        # Load model
        self.load_model()
        
    def load_model(self):
        """Load YOLOv8 model"""
        try:
            # Check if custom trained model exists
            if os.path.exists(self.model_path):
                print(f"Loading custom model: {self.model_path}")
                self.model = YOLO(self.model_path)
            else:
                print("Loading pretrained YOLOv8n model...")
                self.model = YOLO('yolov8n.pt')
                print("Note: Using pretrained model. Train custom model for waste classification.")
        except Exception as e:
            print(f"Error loading model: {e}")
            raise
    
    def map_class_to_waste_type(self, class_name):
        """
        Map YOLO class names to waste categories
        
        For pretrained model, we'll map common classes.
        For custom trained model, classes should match directly.
        """
        class_name = class_name.lower()
        
        # Mapping for pretrained COCO model (temporary)
        veggie_keywords = ['broccoli', 'carrot', 'apple', 'orange', 'banana']
        plastic_keywords = ['bottle', 'cup']
        metal_keywords = ['fork', 'knife', 'spoon']
        paper_keywords = ['book', 'paper']
        
        if any(keyword in class_name for keyword in veggie_keywords):
            return 'leafy_vegetables'
        elif any(keyword in class_name for keyword in plastic_keywords):
            return 'plastic'
        elif any(keyword in class_name for keyword in metal_keywords):
            return 'metal'
        elif any(keyword in class_name for keyword in paper_keywords):
            return 'paper'
        else:
            return 'unknown'
    
    def detect(self, frame, filter_non_vegetables=True, draw=True):
        """
        Run detection on a frame
        
        Args:
            frame: OpenCV image (BGR format)
            filter_non_vegetables: If True, only show non-vegetable items (contamination tracking)
            draw: If True, returns an annotated frame; if False, skips drawing and returns raw frame copy
            
        Returns:
            annotated_frame: Frame with bounding boxes (or raw frame copy when draw=False)
            detections: List of detection dictionaries
        """
        if self.model is None:
            return frame, []
        
        # Run inference with reduced image size for faster CPU processing
        results = self.model(frame, conf=self.conf_threshold, verbose=False, imgsz=256)
        
        # Process results
        all_detections = []
        annotated_frame = frame.copy()
        
        for result in results:
            boxes = result.boxes
            
            for box in boxes:
                # Extract box coordinates
                x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()
                confidence = float(box.conf[0])
                class_id = int(box.cls[0])
                class_name = self.model.names[class_id]
                
                # Map to waste type
                waste_type = self.map_class_to_waste_type(class_name)
                
                # Update counts
                self.detection_counts[waste_type] += 1
                
                # Create detection record
                detection = {
                    'type': waste_type,
                    'original_class': class_name,
                    'confidence': confidence,
                    'bbox': [int(x1), int(y1), int(x2), int(y2)],
                    'timestamp': datetime.now().isoformat()
                }
                all_detections.append(detection)
        
        # Filter detections based on mode
        if filter_non_vegetables:
            # Only show non-vegetable items (contamination monitoring)
            filtered_detections = [d for d in all_detections if d['type'] != 'leafy_vegetables']
        else:
            # Show all detections
            filtered_detections = all_detections
        
        # Only draw bounding boxes if requested (frontend will handle drawing in new architecture)
        if draw:
            for detection in filtered_detections:
                x1, y1, x2, y2 = detection['bbox']
                waste_type = detection['type']
                confidence = detection['confidence']
                class_name = detection['original_class']
                
                # Draw bounding box
                color = self.get_color_for_type(waste_type)
                cv2.rectangle(annotated_frame, 
                             (int(x1), int(y1)), 
                             (int(x2), int(y2)), 
                             color, 3)  # Thicker box for visibility
                
                # Draw label with confidence percentage
                label = f"{class_name} {confidence:.0%}"
                label_size, _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 2)
                
                # Background for label
                cv2.rectangle(annotated_frame,
                            (int(x1), int(y1) - label_size[1] - 12),
                            (int(x1) + label_size[0] + 10, int(y1)),
                            color, -1)
                
                # Label text
                cv2.putText(annotated_frame, label,
                          (int(x1) + 5, int(y1) - 5),
                          cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        
        return annotated_frame, filtered_detections
    
    def get_color_for_type(self, waste_type):
        """Get BGR color for each waste type"""
        colors = {
            'leafy_vegetables': (0, 255, 0),    # Green
            'plastic': (0, 0, 255),             # Red
            'metal': (255, 165, 0),             # Orange
            'paper': (255, 255, 0),             # Yellow
            'unknown': (128, 128, 128)          # Gray
        }
        return colors.get(waste_type, (255, 255, 255))
    
    def get_statistics(self):
        """Get current detection statistics"""
        return self.detection_counts.copy()
    
    def reset_statistics(self):
        """Reset detection counts"""
        for key in self.detection_counts:
            self.detection_counts[key] = 0


class WebcamCapture:
    def __init__(self, camera_index=0):
        """
        Initialize webcam capture
        
        Args:
            camera_index: Camera device index (0 for default webcam)
        """
        self.camera_index = camera_index
        self.cap = None
        self.is_open = False
        
    def start(self):
        """Start camera capture"""
        self.cap = cv2.VideoCapture(self.camera_index)
        
        if not self.cap.isOpened():
            raise Exception(f"Cannot open camera {self.camera_index}")
        
        # Set camera properties for better performance
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        self.cap.set(cv2.CAP_PROP_FPS, 30)
        
        self.is_open = True
        print(f"Camera {self.camera_index} started successfully")
        
    def read(self):
        """
        Read a frame from camera
        
        Returns:
            success: Boolean indicating if frame was read
            frame: OpenCV image (BGR format)
        """
        if not self.is_open:
            return False, None
        
        success, frame = self.cap.read()
        return success, frame
    
    def stop(self):
        """Stop camera capture"""
        if self.cap is not None:
            self.cap.release()
            self.is_open = False
            print("Camera stopped")
    
    def __enter__(self):
        """Context manager entry"""
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.stop()


class ESP32StreamCapture:
    def __init__(self, stream_url="http://192.168.1.17/stream"):
        """
        Initialize ESP32-CAM stream capture with auto-reconnection
        
        Args:
            stream_url: URL of ESP32-CAM MJPEG stream
        """
        self.stream_url = stream_url
        self.esp32_base_url = stream_url.rsplit('/', 1)[0]  # Extract base URL for control
        self.cap = None
        self.is_open = False
        self.reconnect_attempts = 0
        self.max_reconnect_attempts = 5
        
        # Performance monitoring
        self.fps = 0
        self.latency = 0
        self.last_frame_time = time.time()
        self.frame_times = []
        
        # Quality management
        self.current_quality = "UXGA"  # Start with highest quality
        self.quality_levels = ["UXGA", "SVGA", "VGA", "QVGA"]
        self.quality_values = {"UXGA": 13, "SVGA": 9, "VGA": 8, "QVGA": 6}
        self.latency_threshold = 2.0  # 2 seconds
        
    def start(self):
        """Start ESP32 stream capture"""
        try:
            print(f"Connecting to ESP32-CAM at {self.stream_url}...")
            self.cap = cv2.VideoCapture(self.stream_url)
            
            if not self.cap.isOpened():
                raise Exception(f"Cannot open ESP32 stream at {self.stream_url}")
            
            self.is_open = True
            self.reconnect_attempts = 0
            print(f"ESP32-CAM connected successfully")
            
        except Exception as e:
            print(f"Error connecting to ESP32: {e}")
            self.is_open = False
            raise
    
    def read(self):
        """
        Read a frame from ESP32 stream with auto-reconnection
        
        Returns:
            success: Boolean indicating if frame was read
            frame: OpenCV image (BGR format)
        """
        if not self.is_open or self.cap is None:
            # Attempt reconnection
            if self._reconnect():
                return self.read()
            return False, None
        
        success, frame = self.cap.read()
        
        if not success:
            print("Frame read failed, attempting reconnection...")
            if self._reconnect():
                return self.read()
            return False, None
        
        # Update performance metrics
        self._update_metrics()
        
        # Quality management disabled - causes timeouts on slow connections
        # self._manage_quality()
        
        return True, frame
    
    def _reconnect(self):
        """Attempt to reconnect to ESP32 stream"""
        if self.reconnect_attempts >= self.max_reconnect_attempts:
            print(f"Max reconnection attempts ({self.max_reconnect_attempts}) reached")
            return False
        
        self.reconnect_attempts += 1
        print(f"Reconnection attempt {self.reconnect_attempts}/{self.max_reconnect_attempts}...")
        
        # Close existing connection
        if self.cap is not None:
            self.cap.release()
        
        time.sleep(2)  # Wait before reconnecting
        
        try:
            self.cap = cv2.VideoCapture(self.stream_url)
            if self.cap.isOpened():
                self.is_open = True
                self.reconnect_attempts = 0
                print("Reconnection successful!")
                return True
        except Exception as e:
            print(f"Reconnection failed: {e}")
        
        return False
    
    def _update_metrics(self):
        """Update FPS and latency metrics"""
        current_time = time.time()
        frame_time = current_time - self.last_frame_time
        self.last_frame_time = current_time
        
        # Track last 30 frame times for average
        self.frame_times.append(frame_time)
        if len(self.frame_times) > 30:
            self.frame_times.pop(0)
        
        # Calculate FPS
        avg_frame_time = sum(self.frame_times) / len(self.frame_times)
        self.fps = 1.0 / avg_frame_time if avg_frame_time > 0 else 0
        
        # Latency is the frame processing time
        self.latency = avg_frame_time
    
    def _manage_quality(self):
        """Automatically adjust stream quality based on latency"""
        try:
            if self.latency > self.latency_threshold:
                # Reduce quality if lagging
                current_index = self.quality_levels.index(self.current_quality)
                if current_index < len(self.quality_levels) - 1:
                    new_quality = self.quality_levels[current_index + 1]
                    self._set_quality(new_quality)
                    print(f"⚠️ High latency detected ({self.latency:.2f}s). Reducing quality to {new_quality}")
            
            elif self.latency < self.latency_threshold * 0.5:
                # Restore quality if latency is good
                current_index = self.quality_levels.index(self.current_quality)
                if current_index > 0:
                    new_quality = self.quality_levels[current_index - 1]
                    self._set_quality(new_quality)
                    print(f"✓ Latency normalized ({self.latency:.2f}s). Restoring quality to {new_quality}")
        except Exception as e:
            print(f"Quality management error: {e}")
    
    def _set_quality(self, quality_level):
        """Send control command to ESP32 to change resolution"""
        try:
            import requests
            quality_val = self.quality_values[quality_level]
            control_url = f"{self.esp32_base_url}/control?var=framesize&val={quality_val}"
            requests.get(control_url, timeout=2)
            self.current_quality = quality_level
        except Exception as e:
            print(f"Failed to set ESP32 quality: {e}")
    
    def get_status(self):
        """Get current stream status"""
        return {
            'connected': self.is_open,
            'fps': round(self.fps, 1),
            'latency': round(self.latency, 3),
            'quality': self.current_quality,
            'reconnect_attempts': self.reconnect_attempts
        }
    
    def stop(self):
        """Stop ESP32 stream capture"""
        if self.cap is not None:
            self.cap.release()
            self.is_open = False
            print("ESP32 stream stopped")
    
    def __enter__(self):
        """Context manager entry"""
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.stop()
