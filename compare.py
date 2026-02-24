import cv2
import numpy as np
import sys
import os

def compare_images(img1_path, img2_path):
    if not os.path.exists(img1_path):
        print(f"Error: {img1_path} not found")
        return 0.0
    if not os.path.exists(img2_path):
        print(f"Error: {img2_path} not found")
        return 0.0

    img1 = cv2.imread(img1_path)
    img2 = cv2.imread(img2_path)

    if img1 is None or img2 is None:
        print("Error: Could not read one or both images")
        return 0.0

    if img1.shape != img2.shape:
        print(f"Images have different shapes: {img1.shape} vs {img2.shape}")
        # Resize to match for a rough comparison, but ideally they should be same
        img2 = cv2.resize(img2, (img1.shape[1], img1.shape[0]))

    diff = cv2.absdiff(img1, img2)
    # create a mask of differences > small threshold
    threshold = 5
    _, diff_mask = cv2.threshold(cv2.cvtColor(diff, cv2.COLOR_BGR2GRAY), threshold, 255, cv2.THRESH_BINARY)
    
    diff_pixels = np.count_nonzero(diff_mask)
    total_pixels = img1.shape[0] * img1.shape[1]
    
    match_percentage = 100.0 * (1.0 - (diff_pixels / total_pixels))
    
    # Save diff image for manual inspect
    cv2.imwrite("diff_output.png", diff)
    
    print(f"Match: {match_percentage:.2f}%")
    return match_percentage

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python compare.py <img1> <img2>")
        sys.exit(1)
    compare_images(sys.argv[1], sys.argv[2])
