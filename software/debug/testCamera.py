import cv2

cap = cv2.VideoCapture(0)  # 0: default webcam

if not cap.isOpened():
    raise RuntimeError("Cannot open webcam (index 0)")

while True:
    ret, frame = cap.read()
    if not ret:
        break

    cv2.imshow("webcam", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):  # press q to quit
        break

cap.release()
cv2.destroyAllWindows()
