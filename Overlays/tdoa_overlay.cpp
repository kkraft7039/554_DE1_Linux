#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>  // for std::max

int main() {
    cv::VideoCapture cap(0);  // /dev/video0
    if (!cap.isOpened()) {
        std::cerr << "Error: could not open camera\n";
        return 1;
    }

    cap.set(CV_CAP_PROP_FRAME_WIDTH, 800);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 600);

    cv::Mat frame;

    // Fade strength for each quadrant:
    // 1 = top-right, 2 = top-left, 3 = bottom-right, 4 = bottom-left
    float quadFade[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: failed to read frame\n";
            break;
        }

        /*
        Get sound location from TDOA on the PL and show corresponding quadrant on the overlay.

        tdoaQuadrant = 0; // Unknown/No Sound
        tdoaQuadrant = 1; // Top-Right
        tdoaQuadrant = 2; // Top-Left
        tdoaQuadrant = 3; // Bottom-Right
        tdoaQuadrant = 4; // Bottom-Left
        */

        // Example test pulse:
        // Replace this with your real PL/TDOA output
        int tdoaQuadrant = 0;

        // Demo trigger from keyboard for testing
        char key = (char)cv::waitKey(1);
        if (key == '1') tdoaQuadrant = 1;
        if (key == '2') tdoaQuadrant = 2;
        if (key == '3') tdoaQuadrant = 3;
        if (key == '4') tdoaQuadrant = 4;
        if (key == 27 || key == 'q') break;

        // If a pulse is detected, reset that quadrant fade to full
        if (tdoaQuadrant >= 1 && tdoaQuadrant <= 4) {
            quadFade[tdoaQuadrant] = 1.0f;
        }

        // Create overlay
        cv::Mat overlay;
        frame.copyTo(overlay);

        int w = frame.cols;
        int h = frame.rows;

        // Define quadrant rectangles
        cv::Rect rects[5];
        rects[1] = cv::Rect(w/2,    0,      w/2,    h/2); // Top-Right
        rects[2] = cv::Rect(0,      0,      w/2,    h/2); // Top-Left
        rects[3] = cv::Rect(w/2,    h/2,    w/2,    h/2); // Bottom-Right
        rects[4] = cv::Rect(0,      h/2,    w/2,    h/2); // Bottom-Left

        // Draw only the strongest currently active quadrant
        int activeQuad = 0;
        float maxFade = 0.0f;
        for (int i = 1; i <= 4; i++) {
            if (quadFade[i] > maxFade) {
                maxFade = quadFade[i];
                activeQuad = i;
            }
        }

        if (activeQuad != 0 && maxFade > 0.01f) {
            // Filled green rectangle on overlay
            cv::rectangle(
                overlay,
                rects[activeQuad],
                CV_RGB(0, 255, 0),
                CV_FILLED
            );

            // Blend based on fade amount
            double alpha = 0.35 * maxFade;
            cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0.0, frame);
        }

        // Fade out slowly
        for (int i = 1; i <= 4; i++) {
            quadFade[i] -= 0.02f;   // smaller = slower fade
            if (quadFade[i] < 0.0f) quadFade[i] = 0.0f;
        }

        cv::imshow("Camera Overlay", frame);
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}