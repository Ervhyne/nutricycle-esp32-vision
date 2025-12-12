"""
Quick Setup Script for NutriCycle Detection System
Checks dependencies and provides setup guidance
"""

import sys
import subprocess
import os

def check_python_version():
    """Check if Python version is compatible"""
    version = sys.version_info
    if version.major < 3 or (version.major == 3 and version.minor < 8):
        print("❌ Python 3.8+ required")
        print(f"   Current version: {version.major}.{version.minor}.{version.micro}")
        return False
    print(f"✅ Python {version.major}.{version.minor}.{version.micro}")
    return True

def check_pip():
    """Check if pip is available"""
    try:
        subprocess.run([sys.executable, "-m", "pip", "--version"], 
                      capture_output=True, check=True)
        print("✅ pip is available")
        return True
    except:
        print("❌ pip not found")
        return False

def install_dependencies():
    """Install required packages"""
    print("\n📦 Installing dependencies...")
    requirements_file = os.path.join("backend", "requirements.txt")
    
    if not os.path.exists(requirements_file):
        print(f"❌ {requirements_file} not found")
        return False
    
    try:
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", requirements_file],
                      check=True)
        print("✅ Dependencies installed successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Error installing dependencies: {e}")
        return False

def check_webcam():
    """Check if webcam is accessible"""
    try:
        import cv2
        cap = cv2.VideoCapture(0)
        if cap.isOpened():
            print("✅ Webcam detected")
            cap.release()
            return True
        else:
            print("⚠️  Webcam not detected (may still work)")
            return True
    except ImportError:
        print("⚠️  OpenCV not installed yet")
        return True

def create_directories():
    """Create necessary directories"""
    dirs = [
        "backend/models",
        "backend/logs"
    ]
    
    for directory in dirs:
        os.makedirs(directory, exist_ok=True)
    
    print("✅ Directories created")
    return True

def main():
    print("="*60)
    print("🌱 NutriCycle Detection System - Setup")
    print("="*60 + "\n")
    
    # Check prerequisites
    print("Checking prerequisites...\n")
    
    if not check_python_version():
        return
    
    if not check_pip():
        return
    
    # Ask user if they want to install dependencies
    print("\n" + "-"*60)
    response = input("Install dependencies? (y/n): ").lower().strip()
    
    if response == 'y':
        if not install_dependencies():
            return
        
        # Check webcam after installation
        print("\n" + "-"*60)
        print("Checking hardware...\n")
        check_webcam()
    
    # Create directories
    print("\n" + "-"*60)
    print("Setting up directories...\n")
    create_directories()
    
    # Success message
    print("\n" + "="*60)
    print("✅ Setup complete!")
    print("="*60)
    print("\nNext steps:")
    print("1. Run backend server:")
    print("   cd backend")
    print("   python app.py")
    print("\n2. Open frontend:")
    print("   Open frontend/index.html in your browser")
    print("\n3. Click 'Start Detection' to begin")
    print("\nFor more details, see QUICKSTART.md")
    print("="*60)

if __name__ == "__main__":
    main()
