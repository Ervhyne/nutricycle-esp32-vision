# YOLO Training Script for NutriCycle

"""
Script to train YOLO model on food waste dataset.
"""

from ultralytics import YOLO

def train_model():
    # Load a model
    model = YOLO('yolov8n.pt')  # load a pretrained model
    
    # Train the model
    # model.train(data='dataset.yaml', epochs=100, imgsz=640)
    print("Training script ready. Configure dataset.yaml and uncomment training line.")

if __name__ == '__main__':
    train_model()
