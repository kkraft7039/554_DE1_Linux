#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// Hardware interface
#define HW_REGS_BASE      0xFF200000
#define HW_REGS_SPAN      0x00200000
#define LED_PIO_OFFSET    0x00010040   // PS -> PL
#define DIPSW_PIO_OFFSET  0x00010080   // PL -> PS

struct HwInterface {
    int fd;
    void *virtual_base;
    volatile uint32_t *led_pio;
    volatile uint32_t *dipsw_pio;
    uint32_t current_req_clk;

    HwInterface()
        : fd(-1), virtual_base(MAP_FAILED), led_pio(NULL), dipsw_pio(NULL), current_req_clk(2) {}
};

static uint8_t get_next_byte(volatile uint32_t *led_pio,
                             volatile uint32_t *dipsw_pio,
                             uint32_t *current_req)
{
    *current_req ^= 0x01;
    *led_pio = *current_req;

    uint32_t dipsw_val;
    do {
        dipsw_val = *dipsw_pio;
    } while (((dipsw_val >> 1) & 0x01) != (*current_req & 0x01));

    do {
        dipsw_val = *dipsw_pio;
    } while ((dipsw_val & 0x01) == 0);

    usleep(1);
    dipsw_val = *dipsw_pio;
    return (uint8_t)((dipsw_val >> 2) & 0xFF);
}

static bool init_hardware(HwInterface &hw)
{
    hw.fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (hw.fd < 0) {
        std::perror("open /dev/mem");
        return false;
    }

    hw.virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
                           MAP_SHARED, hw.fd, HW_REGS_BASE);
    if (hw.virtual_base == MAP_FAILED) {
        std::perror("mmap");
        close(hw.fd);
        hw.fd = -1;
        return false;
    }

    hw.led_pio   = (volatile uint32_t *)((uint8_t *)hw.virtual_base + LED_PIO_OFFSET);
    hw.dipsw_pio = (volatile uint32_t *)((uint8_t *)hw.virtual_base + DIPSW_PIO_OFFSET);

    hw.current_req_clk = 2;
    *hw.led_pio = hw.current_req_clk;
    return true;
}

static void close_hardware(HwInterface &hw)
{
    if (hw.led_pio) {
        hw.current_req_clk &= 0x01;
        *hw.led_pio = hw.current_req_clk;
    }

    if (hw.virtual_base != MAP_FAILED) {
        munmap(hw.virtual_base, HW_REGS_SPAN);
        hw.virtual_base = MAP_FAILED;
    }
    if (hw.fd >= 0) {
        close(hw.fd);
        hw.fd = -1;
    }

    hw.led_pio = NULL;
    hw.dipsw_pio = NULL;
}

static uint8_t read_quadrant(HwInterface &hw)
{
    if (!hw.led_pio || !hw.dipsw_pio) return 0;

    uint8_t quad = get_next_byte(hw.led_pio, hw.dipsw_pio, &hw.current_req_clk);

    if (quad > 4) return 0;
    return quad;
}

int main() {
    HwInterface hw;
    bool hw_ok = init_hardware(hw);
    if (!hw_ok) {
        std::cerr << "Warning: hardware interface could not be opened. Keyboard test mode only.\n";
    }

    cv::VideoCapture cap(0);  // /dev/video0
    if (!cap.isOpened()) {
        std::cerr << "Error: could not open camera\n";
        close_hardware(hw);
        return 1;
    }

    cap.set(CV_CAP_PROP_FRAME_WIDTH, 800);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 600);

    cv::namedWindow("Camera Overlay", CV_WINDOW_NORMAL);
    cv::setWindowProperty("Camera Overlay", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);

    cv::Mat frame;
    cv::Mat displayFrame;

    // Fade strength for each quadrant:
    // 1 = top-right, 2 = top-left, 3 = bottom-right, 4 = bottom-left
    float quadFade[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    while (true) {
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "Error: failed to read frame\n";
            break;
        }

        /*
        Get sound location from PL/TDOA hardware and show corresponding quadrant on the overlay.

        tdoaQuadrant = 0; // Unknown/No Sound
        tdoaQuadrant = 1; // Top-Right
        tdoaQuadrant = 2; // Top-Left
        tdoaQuadrant = 3; // Bottom-Right
        tdoaQuadrant = 4; // Bottom-Left
        */
        int tdoaQuadrant = 0;

        // Read live hardware first.
        if (hw_ok) {
            tdoaQuadrant = (int)read_quadrant(hw);
        }

        // Keyboard fallback for testing.
        char key = (char)cv::waitKey(1);
        if (key == '1') tdoaQuadrant = 1;
        if (key == '2') tdoaQuadrant = 2;
        if (key == '3') tdoaQuadrant = 3;
        if (key == '4') tdoaQuadrant = 4;
        if (key == 27 || key == 'q') break;

        // If a pulse is detected, reset that quadrant fade to full.
        if (tdoaQuadrant >= 1 && tdoaQuadrant <= 4) {
            quadFade[tdoaQuadrant] = 1.0f;
        }

        // Create overlay.
        cv::Mat overlay;
        frame.copyTo(overlay);

        int w = frame.cols;
        int h = frame.rows;

        // Define quadrant rectangles.
        cv::Rect rects[5];
        rects[1] = cv::Rect(w/2, 0,   w - w/2, h/2);     // Top-Right
        rects[2] = cv::Rect(0,   0,   w/2,     h/2);     // Top-Left
        rects[3] = cv::Rect(w/2, h/2, w - w/2, h - h/2); // Bottom-Right
        rects[4] = cv::Rect(0,   h/2, w/2,     h - h/2); // Bottom-Left

        // Draw only the strongest currently active quadrant.
        int activeQuad = 0;
        float maxFade = 0.0f;
        for (int i = 1; i <= 4; i++) {
            if (quadFade[i] > maxFade) {
                maxFade = quadFade[i];
                activeQuad = i;
            }
        }

        if (activeQuad != 0 && maxFade > 0.01f) {
            cv::rectangle(overlay, rects[activeQuad], CV_RGB(0, 255, 0), CV_FILLED);

            double alpha = 0.35 * maxFade;
            cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0.0, frame);
        }

        // Fade out slowly.
        for (int i = 1; i <= 4; i++) {
            quadFade[i] -= 0.02f;
            if (quadFade[i] < 0.0f) quadFade[i] = 0.0f;
        }

        cv::resize(frame, displayFrame, cv::Size(1600, 1200));
        cv::imshow("Camera Overlay", displayFrame);
    }

    cap.release();
    close_hardware(hw);
    cv::destroyAllWindows();
    return 0;
}
