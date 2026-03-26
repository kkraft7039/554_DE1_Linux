#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: could not open camera\n";
        return 1;
    }

    // Request 640x480
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);

    // Read back what OpenCV thinks it got
    std::cout << "Width: " << cap.get(CV_CAP_PROP_FRAME_WIDTH) << "\n";
    std::cout << "Height: " << cap.get(CV_CAP_PROP_FRAME_HEIGHT) << "\n";

    cv::Mat frame;
    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: failed to read frame\n";
            break;
        }

        cv::imshow("Camera Feed", frame);

        char key = (char)cv::waitKey(1);
        if (key == 27 || key == 'q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
