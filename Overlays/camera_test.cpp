#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: could not open camera\n";
        return 1;
    }

    // Force MJPG first
    cap.set(CV_CAP_PROP_FOURCC, CV_FOURCC('M','J','P','G'));

    // Request 640x480
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(CV_CAP_PROP_FPS, 30);

    // Read back what OpenCV thinks it got
    double actualW = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    double actualH = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    double actualFPS = cap.get(CV_CAP_PROP_FPS);

    std::cout << "Requested: 640x480 @ 30 fps\n";
    std::cout << "Actual: " << actualW << "x" << actualH
              << " @ " << actualFPS << " fps\n";

    cv::Mat frame;
    int frameCount = 0;

    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: failed to read frame\n";
            break;
        }

        if (frameCount % 30 == 0) {
            std::cout << "Frame size from Mat: "
                      << frame.cols << "x" << frame.rows << "\n";
        }
        frameCount++;

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
