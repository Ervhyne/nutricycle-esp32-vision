"""
NutriCycle Detection System
Visual Test and Demo Guide
"""

# Quick Test Checklist

print("="*60)
print("🌱 NutriCycle Detection System - Quick Test")
print("="*60)
print()

# Step 1: Installation
print("STEP 1: Installation Check")
print("-" * 60)
print("Run: python setup.py")
print("Expected: All dependencies installed")
print()

# Step 2: Start Backend
print("STEP 2: Start Backend Server")
print("-" * 60)
print("Run: cd backend && python app.py")
print("Expected: Server starts on http://localhost:5000")
print("Look for: 'NutriCycle Detection Server Starting'")
print()

# Step 3: Open Frontend
print("STEP 3: Open Frontend")
print("-" * 60)
print("Open: frontend/index.html in browser")
print("Expected: Dashboard with 'Start Detection' button")
print()

# Step 4: Test Detection
print("STEP 4: Test Detection")
print("-" * 60)
print("1. Click 'Start Detection'")
print("2. Allow webcam access if prompted")
print("3. Show objects to camera:")
print()
print("   Test Objects (with pretrained model):")
print("   ✓ Any fruit/vegetable → Leafy Vegetables (Green box)")
print("   ✓ Plastic bottle/cup → Plastic (Red box)")
print("   ✓ Fork/spoon/knife → Metal (Orange box)")
print("   ✓ Book/paper → Paper (Yellow box)")
print()
print("Expected: Bounding boxes appear on video feed")
print("Expected: Statistics update in right panel")
print()

# Step 5: Verify Features
print("STEP 5: Verify Features")
print("-" * 60)
print("✓ Video feed shows live camera")
print("✓ Objects have colored bounding boxes")
print("✓ Labels show object type and confidence")
print("✓ Statistics update in real-time")
print("✓ Status badge shows 'Active' (green)")
print()

# Step 6: Check Logs
print("STEP 6: Check Logs")
print("-" * 60)
print("Location: backend/logs/")
print("Expected: detections_YYYYMMDD_HHMMSS.json file created")
print("Contents: JSON with all detection events")
print()

# Troubleshooting
print("="*60)
print("🔧 TROUBLESHOOTING")
print("="*60)
print()
print("Problem: Camera not detected")
print("Solution: Close other apps using webcam (Zoom, Teams, etc.)")
print()
print("Problem: No detections appearing")
print("Solution: Try common objects (bottle, fruit, book)")
print("          Ensure good lighting")
print("          Object should be clearly visible")
print()
print("Problem: Low accuracy")
print("Solution: This is expected with pretrained model")
print("          Train custom model for better results")
print()

# Next Steps
print("="*60)
print("🎯 NEXT STEPS")
print("="*60)
print()
print("1. Collect dataset of your actual waste items")
print("2. Label images using Roboflow or CVAT")
print("3. Train custom YOLOv8 model")
print("4. Replace backend/models/best.pt with trained model")
print("5. Integrate ESP32-CAM when hardware arrives")
print()

print("="*60)
print("📖 For detailed help, see:")
print("   - GET_STARTED.md")
print("   - QUICKSTART.md")
print("   - backend/README.md")
print("="*60)
