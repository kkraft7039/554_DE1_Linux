import cv2
import numpy as np

cap = cv2.VideoCapture(0)

x = 0
while True:
    ret, frame = cap.read()
    if not ret:
        break

    h, w = frame.shape[:2]

    heat = np.zeros((h, w), dtype=np.uint8)
    x = (x + 8) % w
    cv2.circle(heat, (x, h // 2), 60, 255, -1)

    heat_color = cv2.applyColorMap(heat, cv2.COLORMAP_JET)
    frame = cv2.addWeighted(frame, 0.7, heat_color, 0.3, 0)

    cv2.imshow("heatmap_test", frame)
    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()
