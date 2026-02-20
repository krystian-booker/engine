import sys
from PIL import Image
import numpy as np

def compare_images(img1_path, img2_path):
    try:
        img1 = Image.open(img1_path).convert('RGB')
        img2 = Image.open(img2_path).convert('RGB')
    except Exception as e:
        print(f"Error loading images: {e}")
        return

    if img1.size != img2.size:
        print(f"Sizes differ: {img1.size} vs {img2.size}. Resizing img1 to match img2.")
        img1 = img1.resize(img2.size)

    arr1 = np.array(img1).astype(int)
    arr2 = np.array(img2).astype(int)

    diff = np.abs(arr1 - arr2)
    # allow a margin of error of 5 pixel values
    close_pixels = np.all(diff <= 5, axis=2)
    
    match_pixels = np.sum(close_pixels)
    total_pixels = close_pixels.size
    
    match_percentage = (match_pixels / total_pixels) * 100.0
    print(f"Match: {match_percentage:.2f}%")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python compare.py <image1> <image2>")
        sys.exit(1)
    compare_images(sys.argv[1], sys.argv[2])
