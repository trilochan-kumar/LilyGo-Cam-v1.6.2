/*
  Minimal code to capture a frame from the TTGO T-Camera V1.6.2.
  This sketch mimics the official code by:
    - Defining the board model.
    - Including the official pin mapping from "select_pins.h".
    - Checking for PSRAM and selecting the frame size, JPEG quality,
      frame buffer count, and buffer location accordingly.
  
  When PSRAM is available, it uses UXGA resolution with two frame buffers
  allocated in PSRAM. Otherwise, it falls back to QVGA.
*/

#define CAMERA_MODEL_TTGO_T_CAMERA_V162  // Define the board model
#include "select_pins.h"                 // Include the pin mapping

#include "esp_camera.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow time for the serial monitor to open

  // Set up camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  
  // Use the pin definitions provided by select_pins.h
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // Use JPEG to keep the frame size compact

  // Use PSRAM if available for larger resolution and additional frame buffers.
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_UXGA;  // 1600x1200 resolution
    config.jpeg_quality = 10;              // Lower number = higher quality
    config.fb_count     = 2;               // Two frame buffers
    config.fb_location  = CAMERA_FB_IN_PSRAM; // Allocate frame buffers in PSRAM
  } else {
    // Fallback: use a smaller resolution that requires less memory.
    config.frame_size   = FRAMESIZE_QVGA;    // 320x240 resolution
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;  // Allocate in internal DRAM
  }

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    while (true) { delay(1000); }  // Halt if initialization fails
  }
  Serial.println("Camera init succeeded");
}

void loop() {
  // Capture a frame from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    return;
  }
  
  // Print the size of the captured JPEG frame (in bytes)
  Serial.printf("Captured frame: %u bytes\n", fb->len);
  
  // (Optional: Process fb->buf as needed—for example, send over WiFi or save to SD card.)
  
  // Return the frame buffer so it can be reused
  esp_camera_fb_return(fb);

  // Wait for 5 seconds before capturing the next frame
  delay(5000);
}
