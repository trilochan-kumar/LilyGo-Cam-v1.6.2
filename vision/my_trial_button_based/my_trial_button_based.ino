/*
  When the button (pin 15, internal pull‑up) is pressed, the board:
    1. Captures an image.
    2. Sends it to Gemini Vision with the prompt "Describe the image in 5-10 words."
    3. Prints the concise description.
*/

#define CAMERA_MODEL_TTGO_T_CAMERA_V162  // TTGO T-Camera V1.6.2 board
#include "select_pins.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Arduino.h>

//––––– WiFi and API Credentials –––––
char wifi_ssid[] = "SSID";
char wifi_pass[] = "Password";
String gemini_Key = "Gemini_API_Key";

//––––– Button Pin –––––
const int buttonPin = 15;

//––––– Custom Base64 Encoding Function –––––
String base64Encode(const uint8_t* data, size_t len) {
  const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String ret = "";
  int i = 0;
  int j = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];

  while (len--) {
    char_array_3[i++] = *(data++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      for (i = 0; i < 4; i++) {
        ret += base64_chars[char_array_4[i]];
      }
      i = 0;
    }
  }
  if (i > 0) {
    for (j = i; j < 3; j++) {
      char_array_3[j] = 0;
    }
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;
    for (j = 0; j < i + 1; j++) {
      ret += base64_chars[char_array_4[j]];
    }
    while (i++ < 3) {
      ret += '=';
    }
  }
  return ret;
}

//––––– WiFi Initialization –––––
void initWiFi() {
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_pass);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed.");
  }
}

//––––– Gemini Vision API Call –––––
String SendImageToGeminiVision(String key, String prompt) {
  WiFiClientSecure client;
  client.setInsecure();
  const char* host = "generativelanguage.googleapis.com";

  if (!client.connect(host, 443)) {
    Serial.println("Connection to Gemini Vision failed.");
    return "Connection failed";
  }
  
  // Capture an image from the camera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed.");
    return "Camera capture failed";
  }

  // Encode the image to Base64 using our custom function
  String imageBase64 = base64Encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  
  // Prepare JSON payload with prompt and image data
  String payload = "{\"contents\": [{\"parts\": ["
                   "{\"text\": \"" + prompt + "\"},"
                   "{\"inline_data\": {\"mime_type\": \"image/jpeg\", \"data\": \"" + imageBase64 + "\"}}"
                   "]}]}";
  
  // Build the HTTP POST request
  String request = String("POST /v1beta/models/gemini-1.5-flash-latest:generateContent?key=") + key + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "Content-Type: application/json; charset=utf-8\r\n" +
                   "Content-Length: " + String(payload.length()) + "\r\n" +
                   "Connection: close\r\n\r\n" +
                   payload;
  client.print(request);

  // Wait for a response (with a timeout)
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("Client Timeout!");
      client.stop();
      return "Timeout";
    }
    delay(100);
  }
  
  // Read the full response
  String response = "";
  while(client.available()){
    response += client.readString();
  }
  client.stop();
  
  // Extract the JSON part (skip HTTP headers)
  int jsonStart = response.indexOf("{");
  if (jsonStart != -1) {
    String jsonResponse = response.substring(jsonStart);
    
    // Parse the JSON response
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonResponse);
    if (!error) {
      String description = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
      return description;
    } else {
      return "JSON parse error";
    }
  }
  
  return "No JSON response";
}

//––––– Setup –––––
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize button (with internal pull‑up)
  pinMode(buttonPin, INPUT_PULLUP);
  
  // Connect to WiFi
  initWiFi();
  
  // Configure the camera for TTGO T-Camera V1.6.2
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  
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
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Use CIF resolution for Gemini Vision compatibility
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_CIF;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }
  
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed. Restarting...");
    ESP.restart();
  }
  Serial.println("Camera init succeeded");
}

//––––– Main Loop –––––
void loop() {
  // Check if the button is pressed (active LOW)
  if (digitalRead(buttonPin) == LOW) {
    delay(50); // Basic debounce
    if (digitalRead(buttonPin) == LOW) {
      Serial.println("Button pressed. Capturing image...");
      
      // Use a single prompt asking for a short description
      String prompt = "Describe the image in 5-10 words.";
      String description = SendImageToGeminiVision(gemini_Key, prompt);
      Serial.println("Description: " + description);
      
      // Wait until the button is released before accepting a new press
      while (digitalRead(buttonPin) == LOW) {
        delay(10);
      }
      delay(200); // Additional debounce delay
    }
  }
  delay(10);
}
