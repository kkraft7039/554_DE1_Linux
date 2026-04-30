#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <stdio.h>

// Hardware interface
#define HW_REGS_BASE      0xFF200000
#define HW_REGS_SPAN      0x00200000
#define LED_PIO_OFFSET    0x00010040   // PS -> PL
#define DIPSW_PIO_OFFSET  0x00010080   // PL -> PS

// Localization constants
#define SPEED_OF_SOUND_M_S 343.0
#define CLOCK_FREQ_HZ      50000000.0
#define ARRAY_SPACING_M    0.2345

// Mic array coordinates (Meters)
// Assuming M0=Top-Left, M1=Top-Right, M2=Bottom-Right, M3=Bottom-Left
const double MIC_X[4] = {0.11725,  -0.11725,  -0.11725, 0.11725};
const double MIC_Y[4] = {-0.11725,  -0.11725, 0.11725, 0.11725};

struct Point3D {
    double X;
    double Y;
    double Z;
    bool valid;
};

// Helper function to calculate the determinant of a 3x3 matrix
double determinant3x3(double m[3][3]) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
           m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

Point3D calculate_bounded_tdoa(uint16_t hit_cycles[4]) {
    Point3D best_point = {0.0, 0.0, 0.0, false};
    
    // 1. Find the reference mic (0 cycles)
    int ref_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (hit_cycles[i] == 0) {
            ref_idx = i;
            break;
        }
    }
    if (ref_idx == -1) return best_point;

    // 2. Convert clock cycles to measured distance differences
    double measured_dd[4] = {0};
    for (int i = 0; i < 4; i++) {
        measured_dd[i] = (hit_cycles[i] / CLOCK_FREQ_HZ) * SPEED_OF_SOUND_M_S;
    }

    // 3. Define the Search Space (2-meter cube in front of the camera)
    // Camera is at (0,0,0) looking down positive Z.
    double x_min = -1.0, x_max = 1.0;
    double y_min = -1.0, y_max = 1.0;
    double z_min =  0.1, z_max = 2.0; // Don't search inside the physical mics (Z=0)
    double step_size = 0.05; // 5 cm resolution

    double min_error = DBL_MAX;

    // 4. Grid Search
    for (double x = x_min; x <= x_max; x += step_size) {
        for (double y = y_min; y <= y_max; y += step_size) {
            for (double z = z_min; z <= z_max; z += step_size) {
                
                double current_error = 0.0;
                
                // Distance from this hypothetical point to the reference mic
                double expected_d_ref = std::sqrt(std::pow(x - MIC_X[ref_idx], 2) + 
                                                  std::pow(y - MIC_Y[ref_idx], 2) + 
                                                  std::pow(z, 2));

                // Check how well this point matches the other 3 mics
                for (int i = 0; i < 4; i++) {
                    if (i == ref_idx) continue;

                    // Distance from point to this mic
                    double expected_d_mic = std::sqrt(std::pow(x - MIC_X[i], 2) + 
                                                      std::pow(y - MIC_Y[i], 2) + 
                                                      std::pow(z, 2));

                    // What should the delay have been if the sound was exactly here?
                    double expected_dd = expected_d_mic - expected_d_ref;

                    // Add squared error between what we expect and what the FPGA measured
                    current_error += std::pow(expected_dd - measured_dd[i], 2);
                }

                // If this is the lowest error we've seen, save the point
                if (current_error < min_error) {
                    min_error = current_error;
                    best_point.X = x;
                    best_point.Y = y;
                    best_point.Z = z;
                    best_point.valid = true;
                }
            }
        }
    }

    // Optional: Reject the point entirely if even the "best" point has massive error
    // (Meaning the sound was likely a random echo or physical bump to the desk)
    if (min_error > 0.1) { // 10cm squared error threshold
        best_point.valid = false;
    }

    return best_point;
}

// Point3D calculate_tdoa_position(uint16_t hit_cycles[4]) {
//     Point3D result = {0.0, 0.0, 0.0, false};

//     // Step 1: Find the reference microphone (the one with 0 cycles)
//     int ref_idx = -1;
//     for (int i = 0; i < 4; i++) {
//         if (hit_cycles[i] == 0) {
//             ref_idx = i;
//             break;
//         }
//     }

//     if (ref_idx == -1) {
//         std::cerr << "Error: No reference mic found. One cycle count must be 0." << std::endl;
//         return result;
//     }

//     double x0 = MIC_X[ref_idx];
//     double y0 = MIC_Y[ref_idx];

//     // Step 2: Set up the Linear System (A * v = B)
//     double A[3][3];
//     double B[3];
    
//     int row = 0;
//     for (int i = 0; i < 4; i++) {
//         if (i == ref_idx) continue; // Skip the reference mic

//         // Convert delay from clock cycles to meters
//         double delta_t = hit_cycles[i] / CLOCK_FREQ_HZ;
//         double delta_d = delta_t * SPEED_OF_SOUND_M_S;

//         double xi = MIC_X[i];
//         double yi = MIC_Y[i];

//         // Populate Matrix A
//         A[row][0] = 2.0 * (xi - x0);
//         A[row][1] = 2.0 * (yi - y0);
//         A[row][2] = 2.0 * delta_d;

//         // Populate Vector B
//         B[row] = (xi * xi) - (x0 * x0) + 
//                  (yi * yi) - (y0 * y0) - 
//                  (delta_d * delta_d);
//         row++;
//     }

//     // Step 3: Solve the 3x3 system using Cramer's Rule
//     double det_A = determinant3x3(A);

//     if (std::abs(det_A) < 1e-9) {
//         std::cerr << "Error: Matrix is singular. Cannot resolve location." << std::endl;
//         return result;
//     }

//     double A_X[3][3], A_Y[3][3], A_D[3][3];
//     for (int r = 0; r < 3; r++) {
//         for (int c = 0; c < 3; c++) {
//             A_X[r][c] = (c == 0) ? B[r] : A[r][c];
//             A_Y[r][c] = (c == 1) ? B[r] : A[r][c];
//             A_D[r][c] = (c == 2) ? B[r] : A[r][c];
//         }
//     }

//     // Solve for X, Y, and the 3D distance to reference mic (D0)
//     result.X = determinant3x3(A_X) / det_A;
//     result.Y = determinant3x3(A_Y) / det_A;
//     double D0 = determinant3x3(A_D) / det_A;

//     // Step 4: Calculate Z (Depth)
//     // Pythagoras: D0^2 = (X - x0)^2 + (Y - y0)^2 + Z^2
//     double z_squared = (D0 * D0) - 
//                        std::pow(result.X - x0, 2) - 
//                        std::pow(result.Y - y0, 2);

//     // Guard against floating point inaccuracies or impossible physical timings 
//     // producing a negative square root (noise)
//     std::cout << "Z^2: " << z_squared << std::endl;
//     if (z_squared < 0) {
//         result.Z = 0.0;
//     } else {
//         result.Z = std::sqrt(z_squared);
//     }

//     result.valid = true;
//     return result;
// }

struct SoundLocation {
    double x_proj; // -1.0 (left)   to 1.0 (right)
    double y_proj; // -1.0 (bottom) to 1.0 (top)
};

struct HwInterface {
    int fd;
    void *virtual_base;
    volatile uint32_t *led_pio;
    volatile uint32_t *dipsw_pio;
    uint32_t current_req_clk;

    HwInterface()
        : fd(-1), virtual_base(MAP_FAILED), led_pio(NULL), dipsw_pio(NULL), current_req_clk(2) {}
};

// Same byte reader as fast_pcm_file_write.c
static bool get_next_byte_unblocking(HwInterface &hw,
                          uint8_t &byte_out)
{
    static bool waiting = false;
    uint32_t dipsw_val = *hw.dipsw_pio;

    // Phase 1: request byte
    if (!waiting) {
        hw.current_req_clk ^= 0x01;
        *hw.led_pio = hw.current_req_clk;
        waiting = true;
        return false;
    }

    // Phase 2: wait for PL ack to match request bit
    if (((dipsw_val >> 1) & 0x01) != (hw.current_req_clk & 0x01)) {
        return false;
    }

    // Phase 3: wait for valid/data-ready bit
    if ((dipsw_val & 0x01) == 0) {
        return false;
    }

    byte_out = (uint8_t)((dipsw_val >> 2) & 0xFF);

    // Ready for next byte
    waiting = false;
    return true;
}

// Same byte reader as fast_pcm_file_write.c
static bool get_next_byte_blocking(HwInterface &hw,
                          uint8_t &byte_out)
{
    uint32_t dipsw_val = *hw.dipsw_pio;

    // Phase 1: request byte
    hw.current_req_clk ^= 0x01;
    *hw.led_pio = hw.current_req_clk;

    // Phase 2: wait for PL ack to match request bit
    do { 
        dipsw_val = *hw.dipsw_pio;
    } while (((dipsw_val >> 1) & 0x01) != (hw.current_req_clk & 0x01));

    // Phase 3: wait for valid/data-ready bit
    do {
        dipsw_val = *hw.dipsw_pio; 
    } while ((dipsw_val & 0x01) == 0);

    usleep(1);
    byte_out = (uint8_t)((dipsw_val >> 2) & 0xFF);

    // Ready for next byte
    return true;
}

static bool init_hardware(HwInterface &hw)
{
    hw.fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (hw.fd < 0) {
        perror("open /dev/mem");
        return false;
    }

    hw.virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
                           MAP_SHARED, hw.fd, HW_REGS_BASE);
    if (hw.virtual_base == MAP_FAILED) {
        perror("mmap");
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
}

// Reads 4x16-bit mic delay/timestamp values using the same byte handshake.
// Assumes the PL is presenting:
//   mic3 LSB, mic3 MSB, mic2 LSB, mic2 MSB, mic1 LSB, mic1 MSB, mic0 LSB, mic0 MSB
static bool read_mic_delays(HwInterface &hw, uint16_t mic_delay[4])
{
    static uint8_t bytes[8];

    uint8_t byte;
    if (!get_next_byte_unblocking(hw, byte)) {
        return false;
    }

    bytes[0] = byte;

    for (int i = 1; i < 8; ++i) {
        if (!get_next_byte_blocking(hw, byte)) {
            return false;
        }
        bytes[i] = byte;
    }

    // PL sends: mic3 LSB, mic3 MSB, mic2 LSB, mic2 MSB, ...
    int k = 0;
    for (int i = 3; i >= 0; --i) {
        uint8_t lsb = bytes[k++];
        uint8_t msb = bytes[k++];
        mic_delay[i] = (uint16_t)((msb << 8) | lsb);
    }

    for (int i = 0; i < 4; ++i) {
        std::cout << i+1 << ": " << static_cast<int>(mic_delay[i]) << " | ";
    }
    std::cout << std::endl;

    return true;
}

// t1 = Mic 1 (Bottom-Right)
// t2 = Mic 2 (Bottom-Left)
// t3 = Mic 3 (Top-Left)
// t4 = Mic 4 (Top-Right)
static SoundLocation calculate_sound_origin(uint16_t t1, uint16_t t2,
                                            uint16_t t3, uint16_t t4)
{
    SoundLocation loc;

    double sec_per_cycle = 1.0 / CLOCK_FREQ_HZ;
    double t_BR = t1 * sec_per_cycle;
    double t_BL = t2 * sec_per_cycle;
    double t_TL = t3 * sec_per_cycle;
    double t_TR = t4 * sec_per_cycle;

    double t_left   = (t_TL + t_BL) / 2.0;
    double t_right  = (t_TR + t_BR) / 2.0;
    double t_top    = (t_TL + t_TR) / 2.0;
    double t_bottom = (t_BL + t_BR) / 2.0;

    double delta_t_x = t_left - t_right;
    double delta_t_y = t_bottom - t_top;

    double distance_x = delta_t_x * SPEED_OF_SOUND_M_S;
    double distance_y = delta_t_y * SPEED_OF_SOUND_M_S;

    loc.x_proj = distance_x / ARRAY_SPACING_M;
    loc.y_proj = distance_y / ARRAY_SPACING_M;

    if (loc.x_proj > 1.0)  loc.x_proj = 1.0;
    if (loc.x_proj < -1.0) loc.x_proj = -1.0;
    if (loc.y_proj > 1.0)  loc.y_proj = 1.0;
    if (loc.y_proj < -1.0) loc.y_proj = -1.0;

    loc.x_proj = -loc.x_proj; // Flip X so positive is right and negative is left.

    return loc;
}

static cv::Point sound_to_frame_pixel(const SoundLocation &loc, int frame_width, int frame_height)
{
    int x = (int)(((loc.x_proj + 1.0) * 0.5) * (frame_width  - 1));
    int y = (int)(((loc.y_proj + 1.0) * 0.5) * (frame_height - 1));

    x = std::max(0, std::min(frame_width  - 1, x));
    y = std::max(0, std::min(frame_height - 1, y));

    return cv::Point(x, y);
}

static cv::Point loc3d_to_frame_pixel(const Point3D &loc, int frame_width, int frame_height)
{
    int x = (int)(((loc.X + 1.0) * 0.5) * (frame_width  - 1));
    int y = (int)(((-loc.Y + 1.0) * 0.5) * (frame_height - 1));

    x = std::max(0, std::min(frame_width  - 1, x));
    y = std::max(0, std::min(frame_height - 1, y));

    return cv::Point(x, y);
}

// Circular heatmap with slight random variation each time it is drawn.
// 'strength' controls brightness, and the overlay decays between frames.
static void draw_heatmap_blob(cv::Mat &heatmap, const cv::Point &center, double strength)
{
    const int base_radius = 55;
    const int radius_jitter = (std::rand() % 11) - 5;   // [-5, +5]

    const int radius = std::max(20, base_radius + radius_jitter);
    const int cx = center.x;
    const int cy = center.y;

    int x0 = std::max(0, cx - radius);
    int x1 = std::min(heatmap.cols - 1, cx + radius);
    int y0 = std::max(0, cy - radius);
    int y1 = std::min(heatmap.rows - 1, cy + radius);

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            double dx = (double)(x - cx);
            double dy = (double)(y - cy);
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) continue;

            double norm = 1.0 - (dist / (double)radius);
            double falloff = norm * norm; // soft edge

            cv::Vec3b &pix = heatmap.at<cv::Vec3b>(y, x);

            int blue_add  = (int)(20.0  * falloff * strength);
            int green_add = (int)(120.0 * falloff * strength);
            int red_add   = (int)(255.0 * falloff * strength);

            pix[0] = (uchar)std::min(255, (int)pix[0] + blue_add);
            pix[1] = (uchar)std::min(255, (int)pix[1] + green_add);
            pix[2] = (uchar)std::min(255, (int)pix[2] + red_add);
        }
    }
}

int main()
{
//    std::srand((unsigned int)std::time(NULL));

    HwInterface hw;
    bool hw_ok = init_hardware(hw);
    if (!hw_ok) {
        std::cerr << "Warning: hardware interface could not be opened. Running camera only.\n";
    }

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: could not open camera\n";
        close_hardware(hw);
        return 1;
    }

    cap.set(CV_CAP_PROP_FRAME_WIDTH, 800);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 600);

 //   cv::namedWindow("Camera Overlay", CV_WINDOW_NORMAL);
    // cv::setWindowProperty("Camera Overlay", CV_WND_PROP_FULLSCREEN, CV_WINDOW_FULLSCREEN);

    cv::Mat frame;
    cv::Mat heatmap;
    cv::Mat displayFrame;

    // Keep the last valid location so the blob persists even if a read is missed.
    cv::Point last_center(400, 300);
    // 1. Updated Intrinsics (Correct Resolution)
    cv::Mat mtx = (cv::Mat_<double>(3,3) << 
        555.48738527,   0.0,            437.60226682,
        0.0,            556.2852091,    343.09571369,
        0.0,            0.0,            1.0
    );
    
    cv::Mat dist = (cv::Mat_<double>(1,5) << 
        -0.35276978, 0.1837128, 0.00041309, 0.00080041, -0.0580492
    );

    // 2. Extrinsics (Hardware setup: Camera and Mic on the same plane)
    // Rotate 180 degrees (PI) on X to align Audio 'Up' (+Y) with Camera 'Down' (+Y)
    cv::Mat rvec = (cv::Mat_<double>(3,1) << 3.1415926535, 0.0, 0.0); 
    
    // Translation: X=0 (centered), Y=Offset (meters), Z=0 (same plane)
    cv::Mat tvec = (cv::Mat_<double>(3,1) << 0.0, 0.0, 0.0);

    std::vector<cv::Point3f> objectPoints(1);
    std::vector<cv::Point2f> imagePoints(1);

    time_t start_time, end_time;
    
    while (true) {
        cap.grab();
        if (!cap.retrieve(frame) || frame.empty()) {
            std::cerr << "Error: failed to read frame\n";
            break;
        }

        if (heatmap.empty() || heatmap.size() != frame.size()) {
            heatmap = cv::Mat::zeros(frame.size(), CV_8UC3);
            last_center = cv::Point(frame.cols / 2, frame.rows / 2);
        }

        start_time = time(NULL);
        // Fade old heatmap slightly every frame.
        heatmap.convertTo(heatmap, -1, 0.92, 0.0);
        end_time = time(NULL);

        cout << "Heatmap decay time: " << difftime(end_time, start_time) << " seconds" << std::endl;

        uint16_t mic_delay[4] = {0, 0, 0, 0};

        start_time = time(NULL);
        bool got_delays = hw_ok && read_mic_delays(hw, mic_delay);
        end_time = time(NULL);

        cout << "PL read time: " << difftime(end_time, start_time) << " seconds" << std::endl;

        if (got_delays) {
            // SoundLocation loc = calculate_sound_origin(mic_delay[0], mic_delay[1], mic_delay[2], mic_delay[3]);
            // Point3D loc3d = calculate_tdoa_position(mic_delay);
            start_time = time(NULL);
            Point3D loc3d = calculate_bounded_tdoa(mic_delay);
            end_time = time(NULL);

            cout << "3D Position Calculation Time: " << difftime(end_time, start_time) << " seconds" << std::endl;

            if (loc3d.valid) {
                std::cout << "Sound Source Localized!" << std::endl;
                std::cout << "X: " << loc3d.X << " meters" << std::endl;
                std::cout << "Y: " << -loc3d.Y << " meters" << std::endl;
                std::cout << "Z: " << loc3d.Z << " meters" << std::endl;
            }
            
            // last_center = sound_to_frame_pixel(loc, frame.cols, frame.rows);
            // last_center = loc3d_to_frame_pixel(loc3d, frame.cols, frame.rows);
            
            objectPoints[0] = cv::Point3f(loc3d.X, -loc3d.Y, loc3d.Z);
            
            // The Projection
            start_time = time(NULL);
            cv::projectPoints(objectPoints, rvec, tvec, mtx, dist, imagePoints);
            end_time = time(NULL);

            cout << "Projection Time: " << difftime(end_time, start_time) << " seconds" << std::endl;

            std::cout << "Projected 2D Point: (" << imagePoints[0].x << ", " << imagePoints[0].y << ")\n";
            
            last_center = cv::Point(imagePoints[0].x, imagePoints[0].y);

            start_time = time(NULL);
            // Stronger blob when there is more directional separation.
            // double magnitude = std::sqrt(loc.x_proj * loc.x_proj + loc.y_proj * loc.y_proj);
            double magnitude = std::sqrt(loc3d.X * loc3d.X + loc3d.Y * loc3d.Y + loc3d.Z * loc3d.Z); 
            double strength = std::max(0.35, std::min(1.0, 0.45 + 0.55 * magnitude));
            draw_heatmap_blob(heatmap, last_center, strength);
            end_time = time(NULL);

            cout << "Heatmap draw time: " << difftime(end_time, start_time) << " seconds" << std::endl;
        }

        start_time = time(NULL);
        cv::addWeighted(frame, 1.0, heatmap, 0.55, 0.0, displayFrame);
        cv::imshow("Camera Overlay", displayFrame);
        end_time = time(NULL);

        cout << "Display time: " << difftime(end_time, start_time) << " seconds" << std::endl;

        char key = (char)cv::waitKey(1);
        if (key == 27 || key == 'q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    close_hardware(hw);
    return 0;
}
