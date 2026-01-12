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


# ESP32 stream capture support has been removed.
# The architecture now treats Node as the gateway and Python as a strict detection API.
# Devices should POST frames to `/detect` (forwarded by Node) and Python will return detections.
