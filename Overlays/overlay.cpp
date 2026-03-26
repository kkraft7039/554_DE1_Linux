#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::VideoCapture cap(0);  // /dev/video0
    if (!cap.isOpened()) {
        std::cerr << "Error: could not open camera\n";
        return 1;
    }

    cap.set(CV_CAP_PROP_FRAME_WIDTH, 800);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 600);

    int x = 0;
    int y = 100;
    int vx = 4;
    int vy = 2;

    const int boxW = 100;
    const int boxH = 80;

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: failed to read frame\n";
            break;
        }

        int w = frame.cols;
        int h = frame.rows;

        // Update position
        x += vx;
        y += vy;

        // Bounce logic
        if (x < 0 || x + boxW > w) {
            vx = -vx;
            x += vx;
        }
        if (y < 0 || y + boxH > h) {
            vy = -vy;
            y += vy;
        }

        // Create overlay
        cv::Mat overlay;
        frame.copyTo(overlay);  // safer for OpenCV 2.x than clone()

        cv::rectangle(
            overlay,
            cv::Rect(x, y, boxW, boxH),
            cv::Scalar(0, 255, 0),
            -1
        );

        // Blend
        double alpha = 0.35;
        cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0.0, frame);

        // Text
        cv::putText(
            frame,
            "Test Overlay",
            cv::Point(20, 40),
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            cv::Scalar(255, 255, 255),
            2
        );

        cv::imshow("Camera Overlay", frame);

        char key = (char)cv::waitKey(1);
        if (key == 27 || key == 'q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
