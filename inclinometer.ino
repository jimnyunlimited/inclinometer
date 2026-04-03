/**
 * TRAILSENSE V4 - Aircraft Artificial Horizon
 * Waveshare ESP32-S3-Touch-AMOLED-1.43
 * (Pure Math Rotation - Inverted Roll Fix)
 */

#include <Arduino.h>
#include "lcd_bsp.h"
#include "FT3168.h"
#include "qmi8658c.h"
#include "i2c_bsp.h"
#include <math.h>

// --- UI Objects ---
lv_obj_t * ground_line;
lv_point_t ground_points[2];

#define LADDER_COUNT 13
lv_obj_t * ladder_lines[LADDER_COUNT];
lv_point_t ladder_points[LADDER_COUNT][2];

lv_obj_t * ladder_labels_l[LADDER_COUNT];
lv_obj_t * ladder_labels_r[LADDER_COUNT];

#define ROLL_MARKER_COUNT 19
lv_obj_t * roll_markers[ROLL_MARKER_COUNT];
lv_point_t roll_marker_points[ROLL_MARKER_COUNT][3]; // 3 points to support chevrons

lv_obj_t * roll_pointer_line;
lv_point_t roll_pointer_points[3]; // 3 points for the upward chevron

// Base coordinates for the pitch ladder lines (x1, x2, y, degree)
// 10 degrees = 60 pixels of Y movement
struct LadderLine {
    float x1, x2, y;
    int degree;
};

const LadderLine ladder_base[LADDER_COUNT] = {
    {-100, 100, 0,    0},    // Horizon
    {-50,  50,  -60,  10},   // Sky +10
    {-70,  70,  -120, 20},   // Sky +20
    {-50,  50,  -180, 30},   // Sky +30
    {-70,  70,  -240, 40},   // Sky +40
    {-50,  50,  -300, 50},   // Sky +50
    {-70,  70,  -360, 60},   // Sky +60
    {-50,  50,  60,  -10},   // Ground -10
    {-70,  70,  120, -20},   // Ground -20
    {-50,  50,  180, -30},   // Ground -30
    {-70,  70,  240, -40},   // Ground -40
    {-50,  50,  300, -50},   // Ground -50
    {-70,  70,  360, -60}    // Ground -60
};

// Points for the classic "W" aircraft symbol
// Screen center is 233, 233
lv_point_t aircraft_w_points[5] = {
    {233 - 120, 233},       // Left wing tip
    {233 - 40,  233},       // Left inner dip start
    {233,       233 + 30},  // Center bottom dip
    {233 + 40,  233},       // Right inner dip start
    {233 + 120, 233}        // Right wing tip
};

// --- Attitude values ---
float pitch = 0.0;
float roll = 0.0;
float pitch_offset = 0.0;
float roll_offset = 0.0;

// --- IMU Settings ---
#define ALPHA 0.85 
unsigned long last_time = 0;
bool imu_ready = false;

// --- Touch Event Callback (Safe Zeroing) ---
static void screen_click_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        pitch_offset = pitch;
        roll_offset = roll;
        Serial.println("Sensors Zeroed via LVGL!");
    }
}

// --- Function prototypes ---
void setupIMU();
void setupDisplay();
void createUI();
void updateAttitude();
void updateUI();

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   TRAILSENSE V4 - Artificial Horizon   ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    I2C_master_Init();
    setupIMU();
    Touch_Init();
    setupDisplay();
    createUI();

    last_time = millis();
    Serial.println("✓ Setup complete! Tap screen to zero sensors.\n");
}

void setupIMU() {
    Serial.println("Initializing QMI8658 IMU...");
    if (!qmi8658_init()) {
        Serial.println("⚠ QMI8658 initialization failed!");
        imu_ready = false;
        return;
    }
    qmi8658_config_reg(0);  // Normal power mode
    qmi8658_enableSensors(QMI8658_ACCGYR_ENABLE);

    Serial.println("  Warming up sensors...");
    for (int i = 0; i < 50; i++) {
        float acc[3], gyro[3];
        qmi8658_read_xyz(acc, gyro);
        delay(20);
    }
    imu_ready = true;
    Serial.println("✓ IMU ready");
}

void loop() {
    static unsigned long last_update = 0;

    // Update at ~50Hz for smooth animation
    if (millis() - last_update >= 20) {
        if (imu_ready) {
            updateAttitude();
            updateUI();
        }
        last_update = millis();
    }

    lv_timer_handler();
    delay(5);
}

void setupDisplay() {
    lcd_lvgl_Init();
}

void createUI() {
    lv_obj_t * screen = lv_scr_act();
    
    // 1. SKY (The screen background itself is the sky)
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x2B82CB), 0); 
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0); 
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // 2. GROUND (A massive line with a thickness of 1200 pixels)
    ground_line = lv_line_create(screen);
    lv_obj_set_style_line_width(ground_line, 1200, 0);
    lv_obj_set_style_line_color(ground_line, lv_color_hex(0x8B4513), 0);
    lv_obj_set_style_line_rounded(ground_line, false, 0);
    lv_obj_clear_flag(ground_line, LV_OBJ_FLAG_CLICKABLE);

    // 3. PITCH LADDER & LABELS
    for(int i=0; i<LADDER_COUNT; i++) {
        ladder_lines[i] = lv_line_create(screen);
        lv_obj_set_style_line_width(ladder_lines[i], 3, 0);
        lv_obj_set_style_line_color(ladder_lines[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_line_rounded(ladder_lines[i], true, 0);
        lv_obj_clear_flag(ladder_lines[i], LV_OBJ_FLAG_CLICKABLE);

        if (ladder_base[i].degree != 0) {
            ladder_labels_l[i] = lv_label_create(screen);
            lv_label_set_text_fmt(ladder_labels_l[i], "%d", abs(ladder_base[i].degree));
            lv_obj_set_style_text_color(ladder_labels_l[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ladder_labels_l[i], &lv_font_montserrat_24, 0);
            lv_obj_clear_flag(ladder_labels_l[i], LV_OBJ_FLAG_CLICKABLE);

            ladder_labels_r[i] = lv_label_create(screen);
            lv_label_set_text_fmt(ladder_labels_r[i], "%d", abs(ladder_base[i].degree));
            lv_obj_set_style_text_color(ladder_labels_r[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ladder_labels_r[i], &lv_font_montserrat_24, 0);
            lv_obj_clear_flag(ladder_labels_r[i], LV_OBJ_FLAG_CLICKABLE);
        } else {
            ladder_labels_l[i] = NULL;
            ladder_labels_r[i] = NULL;
        }
    }

    // 4. ROLL RING (Static Black Ring around the edge)
    lv_obj_t * roll_ring = lv_obj_create(screen);
    lv_obj_set_size(roll_ring, 466, 466);
    lv_obj_align(roll_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(roll_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(roll_ring, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(roll_ring, 24, 0); // 24px thick border
    lv_obj_set_style_radius(roll_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(roll_ring, LV_OBJ_FLAG_CLICKABLE);

    // 5. ROLL MARKERS (Dots for 30/60/90, Lines for 10/20/40/50/70/80, Chevron for 0)
    for(int i = 0; i < ROLL_MARKER_COUNT; i++) {
        int angle_deg = -90 + (i * 10);
        
        if (angle_deg != 0 && abs(angle_deg) % 30 == 0) {
            // Create Solid Dots for 30, 60, 90
            roll_markers[i] = lv_obj_create(screen);
            lv_obj_set_size(roll_markers[i], 24, 24); // Exactly 24px to fit the ring edge-to-edge
            lv_obj_set_style_bg_color(roll_markers[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(roll_markers[i], LV_OPA_COVER, 0);
            lv_obj_set_style_radius(roll_markers[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(roll_markers[i], 0, 0);
            lv_obj_clear_flag(roll_markers[i], LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Create Lines (and the 0 degree chevron)
            roll_markers[i] = lv_line_create(screen);
            lv_obj_set_style_line_color(roll_markers[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_line_rounded(roll_markers[i], true, 0);
            lv_obj_clear_flag(roll_markers[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // 6. ROLL POINTER (Fixed Upward Chevron, colored yellow)
    roll_pointer_line = lv_line_create(screen);
    lv_obj_set_style_line_width(roll_pointer_line, 4, 0);
    lv_obj_set_style_line_color(roll_pointer_line, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_line_rounded(roll_pointer_line, true, 0);
    lv_obj_clear_flag(roll_pointer_line, LV_OBJ_FLAG_CLICKABLE);
    
    // Fixed at 12 o'clock, pointing UP (tip is at smaller Y)
    roll_pointer_points[0].x = 233 - 14;
    roll_pointer_points[0].y = 233 - 185;
    roll_pointer_points[1].x = 233;
    roll_pointer_points[1].y = 233 - 205; // Tip pointing UP
    roll_pointer_points[2].x = 233 + 14;
    roll_pointer_points[2].y = 233 - 185;
    lv_line_set_points(roll_pointer_line, roll_pointer_points, 3);

    // 7. FIXED AIRCRAFT CROSSHAIR (Classic "W" Style)
    lv_obj_t * aircraft_w = lv_line_create(screen);
    lv_line_set_points(aircraft_w, aircraft_w_points, 5);
    lv_obj_set_style_line_width(aircraft_w, 6, 0);
    lv_obj_set_style_line_color(aircraft_w, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_line_rounded(aircraft_w, true, 0);
    lv_obj_clear_flag(aircraft_w, LV_OBJ_FLAG_CLICKABLE);

    // Center dot for precise zero-pitch reference
    lv_obj_t * center_dot = lv_obj_create(screen);
    lv_obj_set_size(center_dot, 10, 10);
    lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_hex(0xFFFF00), 0); 
    lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0); 
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(center_dot, LV_OBJ_FLAG_CLICKABLE);

    // 8. INVISIBLE TOUCH OVERLAY (For zeroing)
    lv_obj_t * touch_overlay = lv_obj_create(screen);
    lv_obj_set_size(touch_overlay, 466, 466);
    lv_obj_align(touch_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(touch_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_overlay, 0, 0);
    lv_obj_clear_flag(touch_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_overlay, screen_click_cb, LV_EVENT_CLICKED, NULL);

    Serial.println("✓ UI created (Pure Math Mode)");
}

void updateAttitude() {
    float accel[3] = {0.0f, 0.0f, 0.0f};
    float gyro[3] = {0.0f, 0.0f, 0.0f};

    qmi8658_read_xyz(accel, gyro);

    unsigned long current_time = millis();
    float dt = (current_time - last_time) / 1000.0;
    if (dt > 0.1 || dt <= 0.0) dt = 0.01;
    last_time = current_time;

    // Gyro Deadband: Ignore tiny vibrations to prevent drift while stationary
    if (abs(gyro[0]) < 1.0) gyro[0] = 0.0;
    if (abs(gyro[1]) < 1.0) gyro[1] = 0.0;

    // --- CRITICAL FIX: ISOLATED & INVERTED ROLL MATH ---
    // Pitch uses standard atan2(X, Z) for full 360 rotation.
    float accel_pitch = atan2(-accel[0], accel[2]) * 180.0 / PI;
    
    // Roll uses the FULL gravity vector sqrt(X^2 + Z^2). 
    // INVERTED: Added negative sign to accel[1] to flip the roll axis
    float accel_roll = atan2(-accel[1], sqrt(accel[0]*accel[0] + accel[2]*accel[2])) * 180.0 / PI;

    pitch += gyro[1] * dt;
    roll -= gyro[0] * dt; // INVERTED: Subtract gyro instead of add

    // Handle 360-degree wrap-around for the complementary filter
    if (pitch - accel_pitch > 180.0) pitch -= 360.0;
    else if (pitch - accel_pitch < -180.0) pitch += 360.0;

    if (roll - accel_roll > 180.0) roll -= 360.0;
    else if (roll - accel_roll < -180.0) roll += 360.0;

    pitch = ALPHA * pitch + (1.0 - ALPHA) * accel_pitch;
    roll = ALPHA * roll + (1.0 - ALPHA) * accel_roll;

    // Keep internal values strictly within -180 to +180
    while (pitch > 180.0) pitch -= 360.0;
    while (pitch < -180.0) pitch += 360.0;
    while (roll > 180.0) roll -= 360.0;
    while (roll < -180.0) roll += 360.0;

    if (isnan(pitch) || isinf(pitch)) pitch = 0.0;
    if (isnan(roll) || isinf(roll)) roll = 0.0;
}

void updateUI() {
    float display_pitch = (pitch - pitch_offset);
    float display_roll = (roll - roll_offset);

    // Normalize display pitch and roll
    while (display_pitch > 180.0) display_pitch -= 360.0;
    while (display_pitch < -180.0) display_pitch += 360.0;
    while (display_roll > 180.0) display_roll -= 360.0;
    while (display_roll < -180.0) display_roll += 360.0;

    // --- HARD STOPS ---
    if (display_pitch > 60.0) display_pitch = 60.0;
    if (display_pitch < -60.0) display_pitch = -60.0;
    
    if (display_roll > 90.0) display_roll = 90.0;
    if (display_roll < -90.0) display_roll = -90.0;

    int pitch_shift = (int)(display_pitch * 6.0);
    
    // Clamp pitch shift so we don't run out of ground/sky
    if (pitch_shift > 400) pitch_shift = 400;
    if (pitch_shift < -400) pitch_shift = -400;

    // Convert roll to radians for math
    float angle_rad = -display_roll * PI / 180.0;
    float cos_a = cos(angle_rad);
    float sin_a = sin(angle_rad);

    int CX = 233; // Center of 466x466 screen
    int CY = 233;

    // 1. Update Ground Line (Rolls with horizon)
    float gx1 = -1000.0;
    float gy1 = 600.0 + pitch_shift;
    float gx2 = 1000.0;
    float gy2 = 600.0 + pitch_shift;

    ground_points[0].x = CX + (int)(gx1 * cos_a - gy1 * sin_a);
    ground_points[0].y = CY + (int)(gx1 * sin_a + gy1 * cos_a);
    ground_points[1].x = CX + (int)(gx2 * cos_a - gy2 * sin_a);
    ground_points[1].y = CY + (int)(gx2 * sin_a + gy2 * cos_a);

    lv_line_set_points(ground_line, ground_points, 2);

    // 2. Update Pitch Ladder & Labels (Fixed to aircraft, NO ROLL)
    for(int i = 0; i < LADDER_COUNT; i++) {
        float lx1 = ladder_base[i].x1;
        float lx2 = ladder_base[i].x2;
        float ly = ladder_base[i].y + pitch_shift;

        // Update Line (No cos_a / sin_a applied)
        ladder_points[i][0].x = CX + (int)lx1;
        ladder_points[i][0].y = CY + (int)ly;
        ladder_points[i][1].x = CX + (int)lx2;
        ladder_points[i][1].y = CY + (int)ly;
        lv_line_set_points(ladder_lines[i], ladder_points[i], 2);

        // Update Labels
        if (ladder_base[i].degree != 0) {
            float llx = lx1 - 30;
            lv_obj_set_pos(ladder_labels_l[i], CX + (int)llx - 15, CY + (int)ly - 12);

            float rlx = lx2 + 30;
            lv_obj_set_pos(ladder_labels_r[i], CX + (int)rlx - 5, CY + (int)ly - 12);
        }
    }

    // 3. Update Roll Scale (Rotating markers, ALWAYS VISIBLE)
    for(int i = 0; i < ROLL_MARKER_COUNT; i++) {
        int angle_deg = -90 + (i * 10); // -90 to +90 relative to sky
        
        // Rotate the scale in the opposite direction of the roll
        float marker_angle_rad = (angle_deg - 90 - display_roll) * PI / 180.0;
        
        if (angle_deg != 0 && abs(angle_deg) % 30 == 0) {
            // Update Solid Dot Position (30, 60, 90)
            // Center of the 24px thick ring is at radius 221 (233 - 12)
            int r = 221; 
            int cx = CX + (int)(r * cos(marker_angle_rad));
            int cy = CY + (int)(r * sin(marker_angle_rad));
            // Offset by -12 to perfectly center the 24x24 object
            lv_obj_set_pos(roll_markers[i], cx - 12, cy - 12); 
        } 
        else if (angle_deg == 0) {
            // Large downward chevron for 0 degrees (Fits edge-to-edge from 233 to 209)
            lv_obj_set_style_line_width(roll_markers[i], 4, 0);
            float delta = 0.06; // Width of the chevron
            roll_marker_points[i][0].x = CX + (int)(233 * cos(marker_angle_rad - delta));
            roll_marker_points[i][0].y = CY + (int)(233 * sin(marker_angle_rad - delta));
            roll_marker_points[i][1].x = CX + (int)(209 * cos(marker_angle_rad)); // Tip pointing down
            roll_marker_points[i][1].y = CY + (int)(209 * sin(marker_angle_rad));
            roll_marker_points[i][2].x = CX + (int)(233 * cos(marker_angle_rad + delta));
            roll_marker_points[i][2].y = CY + (int)(233 * sin(marker_angle_rad + delta));
            lv_line_set_points(roll_markers[i], roll_marker_points[i], 3);
        } 
        else {
            // Standard straight line for 10, 20, 40, 50, 70, 80
            lv_obj_set_style_line_width(roll_markers[i], 3, 0);
            int inner_r = 217;
            roll_marker_points[i][0].x = CX + (int)(233 * cos(marker_angle_rad));
            roll_marker_points[i][0].y = CY + (int)(233 * sin(marker_angle_rad));
            roll_marker_points[i][1].x = CX + (int)(inner_r * cos(marker_angle_rad));
            roll_marker_points[i][1].y = CY + (int)(inner_r * sin(marker_angle_rad));
            lv_line_set_points(roll_markers[i], roll_marker_points[i], 2);
        }
    }
}