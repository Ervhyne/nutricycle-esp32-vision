"""
Detection Logger
Handles logging of all detection events
"""

import json
import os
from datetime import datetime
from pathlib import Path

class DetectionLogger:
    def __init__(self, log_dir='logs'):
        """
        Initialize detection logger
        
        Args:
            log_dir: Directory to store log files
        """
        self.log_dir = Path(log_dir)
        self.log_dir.mkdir(exist_ok=True)
        
        # Create session log file
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        self.log_file = self.log_dir / f'detections_{timestamp}.json'
        
        # Initialize log structure
        self.session_data = {
            'session_start': datetime.now().isoformat(),
            'detections': []
        }
        
        print(f"Logging to: {self.log_file}")
    
    def log_detection(self, detections, frame_id=None):
        """
        Log detection results
        
        Args:
            detections: List of detection dictionaries
            frame_id: Optional frame identifier
        """
        if not detections:
            return
        
        log_entry = {
            'frame_id': frame_id,
            'timestamp': datetime.now().isoformat(),
            'count': len(detections),
            'objects': detections
        }
        
        self.session_data['detections'].append(log_entry)
    
    def save(self):
        """Save current session data to file"""
        try:
            with open(self.log_file, 'w') as f:
                json.dump(self.session_data, f, indent=2)
        except Exception as e:
            print(f"Error saving log: {e}")
    
    def get_summary(self):
        """Get summary of current session"""
        total_detections = len(self.session_data['detections'])
        
        # Count by type
        type_counts = {}
        for entry in self.session_data['detections']:
            for obj in entry['objects']:
                obj_type = obj['type']
                type_counts[obj_type] = type_counts.get(obj_type, 0) + 1
        
        return {
            'total_frames': total_detections,
            'type_counts': type_counts,
            'session_start': self.session_data['session_start']
        }
