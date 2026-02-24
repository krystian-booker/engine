import os
import re
import subprocess
import cv2
import numpy as np

def update_z(z_val):
    with open('samples/render_test/src/main.cpp', 'r') as f:
        content = f.read()
    content = re.sub(r'Vec3\{0\.0f, 7\.5f, [\d\.]+f\}', f'Vec3{{0.0f, 7.5f, {z_val}f}}', content)
    with open('samples/render_test/src/main.cpp', 'w') as f:
        f.write(content)

def evaluate():
    subprocess.run(['cmake', '--build', 'out/build/Qt-Debug', '--target', 'render_test'], capture_output=True)
    subprocess.run([r'out\build\Qt-Debug\bin\render_test.exe', '--screenshot=test_output.png', '--screenshot-frame=60'], capture_output=True)
    
    img1 = cv2.imread('test_output.png')
    img2 = cv2.imread('samples/render_test/golden/golden.png')
    if img1 is None or img2 is None: return 0.0
    
    diff = cv2.absdiff(img1, img2)
    _, diff_mask = cv2.threshold(cv2.cvtColor(diff, cv2.COLOR_BGR2GRAY), 5, 255, cv2.THRESH_BINARY)
    return 100.0 * (1.0 - (np.count_nonzero(diff_mask) / (img1.shape[0] * img1.shape[1])))

best_z = 21.0
best_score = 0.0

for z in np.arange(17.0, 24.0, 0.5):
    update_z(z)
    score = evaluate()
    print(f'Z={z:.1f} -> {score:.3f}%')
    if score > best_score:
        best_score = score
        best_z = z

# Revert to best Z
update_z(best_z)
evaluate()
print(f'Best Z: {best_z} with score {best_score}%')
