"""
NutriCycle Backend Configuration
"""

# ESP32-CAM Configuration
USE_ESP32 = True  # Set to True when ESP32-CAM is connected, False to use webcam
ESP32_STREAM_URL = "http://192.168.1.17:81/stream"  # ESP32-CAM stream URL

# Detection Configuration
MODEL_PATH = 'backend/models/best.pt'
CONFIDENCE_THRESHOLD = 0.5
FILTER_NON_VEGETABLES = True  # Only show non-vegetable items (contamination tracking)

# Batch Configuration
MAX_BATCH_HISTORY = 10  # Number of batch records to keep in memory

# Server Configuration
HOST = '0.0.0.0'
PORT = 5000
DEBUG = False
