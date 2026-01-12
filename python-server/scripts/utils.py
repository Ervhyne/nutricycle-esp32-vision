# Utility Functions for NutriCycle

"""
Common utility functions used across scripts.
"""

def ensure_dir(path):
    """Ensure a directory exists."""
    import os
    os.makedirs(path, exist_ok=True)
    return path
