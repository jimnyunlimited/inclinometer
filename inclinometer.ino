/**
 * TRAILSENSE V4 - Minimalist Text Readout
 * Waveshare ESP32-S3-Touch-AMOLED-1.43
 *
 * Features:
 * - Ultra-simple text readouts for Pitch and Roll
 * - No complex graphics or LVGL containers
 * - Tap screen to zero calibration
 */

#include <Arduino.h>
#include "lcd_bsp.h"
#include "FT3168.h"
#include "qmi8658c.h"
#include "i2c_bsp.h"

// UI objects
lv_obj_t * pitch_label;
lv_obj_t * roll_label;

// Attitude values
float pitch = 0.0;
float roll = 0.0;

// Calibration offsets
float pitch_offset = 0.0;
float roll_offset = 0.0;

// Complementary filter
#define ALPHA 0.96
unsigned long last_time = 0;
bool imu_ready = false;

// Touch tracking
bool touch_active = false;

// Function prototypes
void setupIMU();
void setupDisplay();
void createUI();
void updateAttitude();
void updateUI();
void handleTouch();

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   TRAILSENSE V4 - Minimalist Text      ║");
    Serial.println("║   Waveshare ESP32-S3-AMOLED-1.43       ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    // 1. Turn on the I2C bus FIRST
    I2C_master_Init();
    Serial.println("✓ I2C Bus initialized");

    // 2. Initialize IMU BEFORE the display/LVGL starts!
    setupIMU();

    // 3. Initialize Touch
    Touch_Init();
    Serial.println("✓ Touch initialized");

    // 4. Initialize display (and LVGL) LAST
    setupDisplay();

    // 5. Create UI
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

    Serial.println("✓ QMI8658 Found!");

    qmi8658_config_reg(0);  // Normal power mode
    qmi8658_enableSensors(QMI8658_ACCGYR_ENABLE);

    // Warm-up period
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

    // Update at ~50Hz
    if (millis() - last_update >= 20) {
        if (imu_ready) {
            updateAttitude();
            updateUI();
        }
        handleTouch();
        last_update = millis();
    }

    lv_timer_handler();
    delay(5);
}

void setupDisplay() {
    Serial.println("Initializing AMOLED Display...");
    lcd_lvgl_Init();
    Serial.println("✓ Display initialized");
}

void createUI() {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0); 

    // --- Pitch Label (Top Center) ---
    pitch_label = lv_label_create(screen);
    lv_obj_set_width(pitch_label, 400); // WIDE enough to stop word-wrapping/dancing
    lv_obj_set_style_text_align(pitch_label, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_align(pitch_label, LV_ALIGN_CENTER, 0, -50);
    lv_obj_set_style_text_color(pitch_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(pitch_label, &lv_font_montserrat_48, 0); 
    // FIX GARBLED TEXT: Give the label its own solid black background
    lv_obj_set_style_bg_color(pitch_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pitch_label, LV_OPA_COVER, 0);
    
    // --- Roll Label (Bottom Center) ---
    roll_label = lv_label_create(screen);
    lv_obj_set_width(roll_label, 400); // WIDE enough to stop word-wrapping/dancing
    lv_obj_set_style_text_align(roll_label, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_align(roll_label, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_text_color(roll_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(roll_label, &lv_font_montserrat_48, 0);
    // FIX GARBLED TEXT: Give the label its own solid black background
    lv_obj_set_style_bg_color(roll_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(roll_label, LV_OPA_COVER, 0);

    // --- Hint Label ---
    lv_obj_t * hint = lv_label_create(screen);
    lv_obj_set_width(hint, 400);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(hint, "TAP TO ZERO");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_20, 0);
    // FIX GARBLED TEXT: Give the label its own solid black background
    lv_obj_set_style_bg_color(hint, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hint, LV_OPA_COVER, 0);

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

    // Calculate angles from accelerometer
    float accel_roll = atan2(accel[1], accel[2]) * 180.0 / PI;
    float accel_pitch = atan2(-accel[0], sqrt(accel[1] * accel[1] + accel[2] * accel[2])) * 180.0 / PI;

    // Integrate gyroscope
    roll += gyro[0] * dt;
    pitch += gyro[1] * dt;

    // Complementary filter
    roll = ALPHA * roll + (1.0 - ALPHA) * accel_roll;
    pitch = ALPHA * pitch + (1.0 - ALPHA) * accel_pitch;
}

void updateUI() {
    char buf[32];

    // Apply calibration offset for display
    float display_pitch = pitch - pitch_offset;
    float display_roll = roll - roll_offset;

    // Update text labels
    snprintf(buf, sizeof(buf), "PITCH: %.1f", display_pitch);
    lv_label_set_text(pitch_label, buf);

    snprintf(buf, sizeof(buf), "ROLL: %.1f", display_roll);
    lv_label_set_text(roll_label, buf);

    lv_obj_invalidate(lv_scr_act());
}

void handleTouch() {
    uint16_t touch_x = 0, touch_y = 0;

    if (getTouch(&touch_x, &touch_y)) {
        if (!touch_active) {
            touch_active = true;
            
            // Zero the display on tap
            pitch_offset = pitch;
            roll_offset = roll;
            Serial.println("Sensors Zeroed! Touch detected at X:" + String(touch_x) + " Y:" + String(touch_y));
        }
    } else {
        touch_active = false;
    }
}