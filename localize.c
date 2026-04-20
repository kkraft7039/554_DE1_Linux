#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// --- Physical Constants ---
#define SPEED_OF_SOUND_M_S 343.0
#define CLOCK_FREQ_HZ      50000000.0 // 50 MHz
#define ARRAY_SPACING_M    0.2345     // 23.45 cm

typedef struct {
    double x_proj; // -1.0 (Left) to 1.0 (Right)
    double y_proj; // -1.0 (Bottom) to 1.0 (Top)
} SoundLocation;

// t1 = Mic 1 (Bottom-Right)
// t2 = Mic 2 (Bottom-Left)
// t3 = Mic 3 (Top-Left)
// t4 = Mic 4 (Top-Right)
SoundLocation calculate_sound_origin(uint16_t t1, uint16_t t2, uint16_t t3, uint16_t t4) {
    SoundLocation loc;
    
    // 1. Convert clock cycles to absolute seconds
    double sec_per_cycle = 1.0 / CLOCK_FREQ_HZ;
    double t_BR = t1 * sec_per_cycle; // Bottom-Right
    double t_BL = t2 * sec_per_cycle; // Bottom-Left
    double t_TL = t3 * sec_per_cycle; // Top-Left
    double t_TR = t4 * sec_per_cycle; // Top-Right

    // 2. Average the pairs to isolate X and Y directions
    double t_left   = (t_TL + t_BL) / 2.0;
    double t_right  = (t_TR + t_BR) / 2.0;
    double t_top    = (t_TL + t_TR) / 2.0;
    double t_bottom = (t_BL + t_BR) / 2.0;

    // 3. Find the Time Differences
    // If sound is from Right, Right mics hit first (smaller T), Left hits later.
    // So (t_left - t_right) yields a POSITIVE number for Right-side sources.
    double delta_t_x = t_left - t_right;
    
    // If sound is from Top, Top mics hit first (smaller T), Bottom hits later.
    // So (t_bottom - t_top) yields a POSITIVE number for Top-side sources.
    double delta_t_y = t_bottom - t_top;

    // 4. Convert time differences to distance (meters)
    double distance_x = delta_t_x * SPEED_OF_SOUND_M_S;
    double distance_y = delta_t_y * SPEED_OF_SOUND_M_S;

    // 5. Normalize against the physical distance between mics
    loc.x_proj = distance_x / ARRAY_SPACING_M;
    loc.y_proj = distance_y / ARRAY_SPACING_M;

    // Constrain to unit bounds (-1.0 to 1.0)
    if (loc.x_proj > 1.0) loc.x_proj = 1.0;
    if (loc.x_proj < -1.0) loc.x_proj = -1.0;
    if (loc.y_proj > 1.0) loc.y_proj = 1.0;
    if (loc.y_proj < -1.0) loc.y_proj = -1.0;

    return loc;
}

int main() {
    // Test Case: Sound coming from the Top-Left
    // Mic 3 (Top-Left) should hit first (0)
    // Mic 1 (Bottom-Right) should hit last
    uint32_t t1 = 26000; // Mic 1 (Bottom-Right)
    uint32_t t2 = 13000; // Mic 2 (Bottom-Left)
    uint32_t t3 = 0;     // Mic 3 (Top-Left)
    uint32_t t4 = 13000; // Mic 4 (Top-Right)

    SoundLocation sound_loc = calculate_sound_origin(t1, t2, t3, t4);

    printf("Cartesian Camera Coordinates:\n");
    printf("X: %5.2f (Negative = Left,  Positive = Right)\n", sound_loc.x_proj);
    printf("Y: %5.2f (Negative = Down,  Positive = Up)\n", sound_loc.y_proj);

    // --- Screen/Camera Mapping ---
    // Note: Standard image mapping puts (0,0) at the Top-Left of the screen!
    // So we must invert the Y projection when mapping to pixel rows.
    int screen_width = 1920;
    int screen_height = 1080;
    
    int pixel_x = (int)((sound_loc.x_proj + 1.0) / 2.0 * screen_width);
    int pixel_y = (int)((-sound_loc.y_proj + 1.0) / 2.0 * screen_height); // Y inverted for screen
    
    printf("\nDraw dot at Image Pixel: (X: %d, Y: %d)\n", pixel_x, pixel_y);

    return 0;
}
