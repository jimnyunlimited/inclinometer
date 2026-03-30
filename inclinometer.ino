/**
 * TRAILSENSE V4 - Aircraft Artificial Horizon
 * Waveshare ESP32-S3-Touch-AMOLED-1.43
 * (Pure Math Rotation - 60 Degree Hard Stop)
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
#define ALPHA 0.96
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
        // Create Line
        ladder_lines[i] = lv_line_create(screen);
        lv_obj_set_style_line_width(ladder_lines[i], 3, 0);
        lv_obj_set_style_line_color(ladder_lines[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_line_rounded(ladder_lines[i], true, 0);
        lv_obj_clear_flag(ladder_lines[i], LV_OBJ_FLAG_CLICKABLE);

        // Create Labels (Skip the 0 degree horizon line)
        if (ladder_base[i].degree != 0) {
            // Left Label
            ladder_labels_l[i] = lv_label_create(screen);
            lv_label_set_text_fmt(ladder_labels_l[i], "%d", abs(ladder_base[i].degree));
            lv_obj_set_style_text_color(ladder_labels_l[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ladder_labels_l[i], &lv_font_montserrat_24, 0); // Double size font
            lv_obj_clear_flag(ladder_labels_l[i], LV_OBJ_FLAG_CLICKABLE);

            // Right Label
            ladder_labels_r[i] = lv_label_create(screen);
            lv_label_set_text_fmt(ladder_labels_r[i], "%d", abs(ladder_base[i].degree));
            lv_obj_set_style_text_color(ladder_labels_r[i], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(ladder_labels_r[i], &lv_font_montserrat_24, 0); // Double size font
            lv_obj_clear_flag(ladder_labels_r[i], LV_OBJ_FLAG_CLICKABLE);
        } else {
            ladder_labels_l[i] = NULL;
            ladder_labels_r[i] = NULL;
        }
    }

    // 4. FIXED AIRCRAFT CROSSHAIR (Classic "W" Style)
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

    // 5. INVISIBLE TOUCH OVERLAY (For zeroing)
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

    // Full 360-degree pitch calculation
    float accel_pitch = atan2(-accel[0], accel[2]) * 180.0 / PI;
    float accel_roll = atan2(accel[1], accel[2]) * 180.0 / PI;

    pitch += gyro[1] * dt;
    roll += gyro[0] * dt;

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
    
    // DISABLE ROLL: Hardcoded to 0.0 to stop the spinning
    float display_roll = 0.0; // (Normally: roll - roll_offset)

    // Normalize display pitch to handle zeroing near the 180 degree boundary
    while (display_pitch > 180.0) display_pitch -= 360.0;
    while (display_pitch < -180.0) display_pitch += 360.0;

    // --- CRITICAL FIX: HARD STOP AT +/- 60 DEGREES ---
    // This physically prevents the UI from drawing past the 60 degree markers
    if (display_pitch > 60.0) display_pitch = 60.0;
    if (display_pitch < -60.0) display_pitch = -60.0;

    int pitch_shift = (int)(display_pitch * 6.0);

    // Convert roll to radians for math
    float angle_rad = -display_roll * PI / 180.0;
    float cos_a = cos(angle_rad);
    float sin_a = sin(angle_rad);

    int CX = 233; // Center of 466x466 screen
    int CY = 233;

    // 1. Update Ground Line
    float gx1 = -1000.0;
    float gy1 = 600.0 + pitch_shift;
    float gx2 = 1000.0;
    float gy2 = 600.0 + pitch_shift;

    ground_points[0].x = CX + (int)(gx1 * cos_a - gy1 * sin_a);
    ground_points[0].y = CY + (int)(gx1 * sin_a + gy1 * cos_a);
    ground_points[1].x = CX + (int)(gx2 * cos_a - gy2 * sin_a);
    ground_points[1].y = CY + (int)(gx2 * sin_a + gy2 * cos_a);

    lv_line_set_points(ground_line, ground_points, 2);

    // 2. Update Pitch Ladder & Labels
    for(int i = 0; i < LADDER_COUNT; i++) {
        float lx1 = ladder_base[i].x1;
        float lx2 = ladder_base[i].x2;
        float ly = ladder_base[i].y + pitch_shift;

        // Update Line
        ladder_points[i][0].x = CX + (int)(lx1 * cos_a - ly * sin_a);
        ladder_points[i][0].y = CY + (int)(lx1 * sin_a + ly * cos_a);
        ladder_points[i][1].x = CX + (int)(lx2 * cos_a - ly * sin_a);
        ladder_points[i][1].y = CY + (int)(lx2 * sin_a + ly * cos_a);
        lv_line_set_points(ladder_lines[i], ladder_points[i], 2);

        // Update Labels
        if (ladder_base[i].degree != 0) {
            // Left label (offset by -30 pixels to the left of the line)
            float llx = lx1 - 30;
            int slx = CX + (int)(llx * cos_a - ly * sin_a);
            int sly = CY + (int)(llx * sin_a + ly * cos_a);
            lv_obj_set_pos(ladder_labels_l[i], slx - 15, sly - 12); // -12 centers the larger font vertically

            // Right label (offset by +30 pixels to the right of the line)
            float rlx = lx2 + 30;
            int srx = CX + (int)(rlx * cos_a - ly * sin_a);
            int sry = CY + (int)(rlx * sin_a + ly * cos_a);
            lv_obj_set_pos(ladder_labels_r[i], srx - 5, sry - 12); // -12 centers the larger font vertically
        }
    }
}