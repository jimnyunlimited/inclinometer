/**
 * TRAILSENSE V4 - Aircraft Artificial Horizon
 * Waveshare ESP32-S3-Touch-AMOLED-1.43
 */

#include <Arduino.h>
#include "lcd_bsp.h"
#include "FT3168.h"
#include "qmi8658c.h"
#include "i2c_bsp.h"

// --- UI Objects ---
lv_obj_t * horizon_cont; 
lv_obj_t * pitch_label;
lv_obj_t * roll_label;

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

// Helper function to draw lines on the pitch ladder
void create_ladder_line(lv_obj_t * parent, int width, int y_offset) {
    lv_obj_t * line = lv_obj_create(parent);
    lv_obj_set_size(line, width, 4);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, y_offset);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0); 
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);           
    lv_obj_set_style_min_height(line, 0, 0);        
    lv_obj_set_style_radius(line, 2, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE); // Don't steal clicks
}

void createUI() {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0); 
    
    // CRITICAL FIX: Disable scrolling on the main screen so the horizon doesn't fly away!
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // 1. MASSIVE HORIZON CONTAINER
    horizon_cont = lv_obj_create(screen);
    lv_obj_set_size(horizon_cont, 1200, 1200);
    lv_obj_set_pos(horizon_cont, -367, -367); 
    lv_obj_set_style_bg_opa(horizon_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(horizon_cont, 0, 0);
    lv_obj_set_style_pad_all(horizon_cont, 0, 0);
    lv_obj_clear_flag(horizon_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(horizon_cont, LV_OBJ_FLAG_CLICKABLE); // Don't steal clicks

    // 2. SKY (Top Half)
    lv_obj_t * sky = lv_obj_create(horizon_cont);
    lv_obj_set_size(sky, 1200, 600);
    lv_obj_align(sky, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(sky, lv_color_hex(0x2B82CB), 0); 
    lv_obj_set_style_bg_opa(sky, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sky, 0, 0);
    lv_obj_set_style_pad_all(sky, 0, 0);
    lv_obj_set_style_radius(sky, 0, 0);
    lv_obj_clear_flag(sky, LV_OBJ_FLAG_CLICKABLE);

    // 3. GROUND (Bottom Half)
    lv_obj_t * ground = lv_obj_create(horizon_cont);
    lv_obj_set_size(ground, 1200, 600);
    lv_obj_align(ground, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ground, lv_color_hex(0x8B4513), 0); 
    lv_obj_set_style_bg_opa(ground, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ground, 0, 0);
    lv_obj_set_style_pad_all(ground, 0, 0);
    lv_obj_set_style_radius(ground, 0, 0);
    lv_obj_clear_flag(ground, LV_OBJ_FLAG_CLICKABLE);

    // 4. PITCH LADDER
    create_ladder_line(horizon_cont, 1200, 0);   
    create_ladder_line(horizon_cont, 100, -60);  
    create_ladder_line(horizon_cont, 140, -120); 
    create_ladder_line(horizon_cont, 100, 60);   
    create_ladder_line(horizon_cont, 140, 120);  

    // 5. FIXED AIRCRAFT CROSSHAIR
    lv_obj_t * center_dot = lv_obj_create(screen);
    lv_obj_set_size(center_dot, 12, 12);
    lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_hex(0xFFFF00), 0); 
    lv_obj_set_style_bg_opa(center_dot, LV_OPA_COVER, 0); 
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_set_style_pad_all(center_dot, 0, 0);
    lv_obj_set_style_min_height(center_dot, 0, 0);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(center_dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * left_wing = lv_obj_create(screen);
    lv_obj_set_size(left_wing, 80, 6);
    lv_obj_align(left_wing, LV_ALIGN_CENTER, -60, 0);
    lv_obj_set_style_bg_color(left_wing, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_bg_opa(left_wing, LV_OPA_COVER, 0); 
    lv_obj_set_style_border_width(left_wing, 0, 0);
    lv_obj_set_style_pad_all(left_wing, 0, 0);
    lv_obj_set_style_min_height(left_wing, 0, 0);
    lv_obj_clear_flag(left_wing, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * right_wing = lv_obj_create(screen);
    lv_obj_set_size(right_wing, 80, 6);
    lv_obj_align(right_wing, LV_ALIGN_CENTER, 60, 0);
    lv_obj_set_style_bg_color(right_wing, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_bg_opa(right_wing, LV_OPA_COVER, 0); 
    lv_obj_set_style_border_width(right_wing, 0, 0);
    lv_obj_set_style_pad_all(right_wing, 0, 0);
    lv_obj_set_style_min_height(right_wing, 0, 0);
    lv_obj_clear_flag(right_wing, LV_OBJ_FLAG_CLICKABLE);

    // 6. TEXT LABELS
    pitch_label = lv_label_create(screen);
    lv_obj_set_width(pitch_label, 300);
    lv_obj_set_style_text_align(pitch_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pitch_label, LV_ALIGN_TOP_MID, 0, 60); 
    lv_obj_set_style_text_color(pitch_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(pitch_label, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_bg_color(pitch_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pitch_label, LV_OPA_50, 0); 
    lv_obj_clear_flag(pitch_label, LV_OBJ_FLAG_CLICKABLE);
    
    roll_label = lv_label_create(screen);
    lv_obj_set_width(roll_label, 300);
    lv_obj_set_style_text_align(roll_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(roll_label, LV_ALIGN_BOTTOM_MID, 0, -60); 
    lv_obj_set_style_text_color(roll_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(roll_label, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_bg_color(roll_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(roll_label, LV_OPA_50, 0);
    lv_obj_clear_flag(roll_label, LV_OBJ_FLAG_CLICKABLE);

    // 7. CRITICAL FIX: INVISIBLE TOUCH OVERLAY
    // Because this is created last, it sits on top of EVERYTHING.
    // It guarantees that tapping anywhere on the screen will trigger the zeroing function.
    lv_obj_t * touch_overlay = lv_obj_create(screen);
    lv_obj_set_size(touch_overlay, 466, 466);
    lv_obj_align(touch_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(touch_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_overlay, 0, 0);
    lv_obj_clear_flag(touch_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(touch_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(touch_overlay, screen_click_cb, LV_EVENT_CLICKED, NULL);

    Serial.println("✓ UI created");
}

void updateAttitude() {
    float accel[3];
    float gyro[3];

    qmi8658_read_xyz(accel, gyro);

    unsigned long current_time = millis();
    float dt = (current_time - last_time) / 1000.0;
    if (dt > 0.1) dt = 0.01;
    last_time = current_time;

    float accel_roll = atan2(accel[1], accel[2]) * 180.0 / PI;
    float accel_pitch = atan2(-accel[0], sqrt(accel[1] * accel[1] + accel[2] * accel[2])) * 180.0 / PI;

    roll += gyro[0] * dt;
    pitch += gyro[1] * dt;

    roll = ALPHA * roll + (1.0 - ALPHA) * accel_roll;
    pitch = ALPHA * pitch + (1.0 - ALPHA) * accel_pitch;
}

void updateUI() {
    char buf[32];

    float display_pitch = pitch - pitch_offset;
    float display_roll = roll - roll_offset;

    // 1. Update Text
    snprintf(buf, sizeof(buf), "PITCH: %.1f", display_pitch);
    lv_label_set_text(pitch_label, buf);

    snprintf(buf, sizeof(buf), "ROLL: %.1f", display_roll);
    lv_label_set_text(roll_label, buf);

    // 2. Move the Horizon UP/DOWN based on Pitch
    int pitch_shift = (int)(display_pitch * 6.0);
    lv_obj_set_pos(horizon_cont, -367, -367 + pitch_shift);

    // 3. Rotate the Horizon based on Roll
    lv_obj_set_style_transform_angle(horizon_cont, (int)(-display_roll * 10.0), 0);

    // 4. Force full screen redraw to prevent AMOLED garbling
    lv_obj_invalidate(lv_scr_act()); 
}