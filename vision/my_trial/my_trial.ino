// =======================================================
// IMPORTANT: Define your board model BEFORE any includes!
// In this example, we are using TTGO T-CAMERA V1.6.2.
// If you are using a different board, change the macro accordingly.
#define CAMERA_MODEL_TTGO_T_CAMERA_V162

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Base64.h"         // Make sure you have an appropriate Base64 library installed.
#include "esp_camera.h"
#include "select_pins.h"      // This file uses the macro defined above to select your board

// --- WiFi and Gemini Vision credentials ---
#define WIFI_SSID    "2o25"
#define WIFI_PASS    "19o82oo2"
String gemini_Key = "AIzaSyA7o-ItgG5ZCnYWIocnhMPJMuaWrBwp6fg";  // Replace with your actual API key

// --- Forward declarations ---
bool setupCamera();
String SendStillToGeminiVision(String key, String prompt);
String SendMessageToGemini(String key, String behaviorInstruction, String previousResponse);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // --- Connect to WiFi ---
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP().toString());
  
  // --- Initialize the camera ---
  if (!setupCamera()) {
    Serial.println("Camera initialization failed. Restarting...");
    delay(1000);
    ESP.restart();
  }
  Serial.println("Camera initialized successfully.");
  
  delay(2000); // Allow the sensor to settle
  
  // --- First Gemini Vision request: capture image and ask for analysis ---
  String analysisPrompt = "Please analyze the image content";
  String analysisResponse = SendStillToGeminiVision(gemini_Key, analysisPrompt);
  Serial.println("\nGemini Vision Analysis Response:");
  Serial.println(analysisResponse);

  // --- Second Gemini Vision request: ask for behavior decision based on the analysis ---
  String behaviorInstruction = "Please follow these guidelines: (1) If people are present, return '1'. (2) If not, return '0'. (3) If it cannot be determined, return '-1'. Do not include any extra explanation.";
  String behaviorResponse = SendMessageToGemini(gemini_Key, behaviorInstruction, analysisResponse);
  Serial.println("\nGemini Vision Behavior Response:");
  Serial.println(behaviorResponse);
}

void loop() {
  // In this example the Gemini Vision process runs once in setup().
  // You can add code here to re-run the capture/analysis process periodically or on a button press.
  delay(60000);  // Wait one minute (or adjust as needed)
}

// =======================================================
// Gemini Vision function: capture image, base64-encode it, and send the image with a prompt.
String SendStillToGeminiVision(String key, String prompt) {
  WiFiClientSecure client;
  client.setInsecure();  // For testing only—disables certificate validation
  const char* host = "generativelanguage.googleapis.com";

  Serial.println("\n[SendStillToGeminiVision] Connecting to " + String(host) + "...");
  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    return "Connection failed";
  }
  Serial.println("Connected.");

  // --- Capture an image from the camera ---
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }

  // --- Base64-encode the JPEG image ---
  int encodedLen = base64_enc_len(fb->len);
  char *encodedImg = (char *)malloc(encodedLen + 1);
  if (!encodedImg) {
    Serial.println("Memory allocation for base64 encoding failed");
    esp_camera_fb_return(fb);
    return "Memory allocation failed";
  }
  base64_encode(encodedImg, (char *)fb->buf, fb->len);
  String imageData = String(encodedImg);
  free(encodedImg);
  esp_camera_fb_return(fb);

  // --- Construct the JSON payload ---
  String payload = "{\"contents\": [{\"parts\": [{ \"text\": \"" + prompt + "\"}, {\"inline_data\": {\"mime_type\": \"image/jpeg\", \"data\": \"" + imageData + "\"}}]}]}";
  
  // --- Build the HTTP POST request ---
  String request = "POST /v1beta/models/gemini-1.5-flash-latest:generateContent?key=" + key + " HTTP/1.1\r\n";
  request += "Host: " + String(host) + "\r\n";
  request += "Content-Type: application/json; charset=utf-8\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += payload;

  client.print(request);
  Serial.println("\n[SendStillToGeminiVision] Request sent:");
  Serial.println(request);

  // --- Read the HTTP response ---
  String response = "";
  long timeout = millis() + 10000;
  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }
  client.stop();
  Serial.println("\n[SendStillToGeminiVision] Full response:");
  Serial.println(response);

  // --- Extract JSON from the HTTP response (skip headers) ---
  int jsonStart = response.indexOf("{");
  String jsonResponse = (jsonStart != -1) ? response.substring(jsonStart) : "";
  
  // --- Parse JSON using ArduinoJson ---
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return "JSON parse error";
  }
  
  String result = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
  if (result == "null" || result == "") {
    result = doc["error"]["message"].as<String>();
  }
  
  return result;
}

// =======================================================
// Gemini Vision function: send a message (behavior instruction) and the previous reply.
String SendMessageToGemini(String key, String behaviorInstruction, String previousResponse) {
  WiFiClientSecure client;
  client.setInsecure();
  const char* host = "generativelanguage.googleapis.com";

  Serial.println("\n[SendMessageToGemini] Connecting to " + String(host) + "...");
  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    return "Connection failed";
  }
  Serial.println("Connected.");

  // --- Construct the JSON payload ---
  String payload = "{\"contents\": [{\"parts\": [{\"text\": \"" + behaviorInstruction + "\"}, {\"text\": \"" + previousResponse + "\"}]}]}";
  
  // --- Build the HTTP POST request ---
  String request = "POST /v1beta/models/gemini-1.5-flash-latest:generateContent?key=" + key + " HTTP/1.1\r\n";
  request += "Host: " + String(host) + "\r\n";
  request += "Content-Type: application/json; charset=utf-8\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += payload;

  client.print(request);
  Serial.println("\n[SendMessageToGemini] Request sent:");
  Serial.println(request);

  // --- Read the HTTP response ---
  String response = "";
  long timeout = millis() + 10000;
  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      char c = client.read();
      response += c;
    }
  }
  client.stop();
  Serial.println("\n[SendMessageToGemini] Full response:");
  Serial.println(response);

  // --- Extract JSON from the HTTP response ---
  int jsonStart = response.indexOf("{");
  String jsonResponse = (jsonStart != -1) ? response.substring(jsonStart) : "";
  
  // --- Parse JSON using ArduinoJson ---
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return "JSON parse error";
  }
  
  String result = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
  if (result == "null" || result == "") {
    result = doc["error"]["message"].as<String>();
  }
  
  return result;
}

// =======================================================
// Camera initialization function
// This function uses the pin definitions provided by "select_pins.h"
bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  sensor_t *s = esp_camera_sensor_get();
  // Adjust sensor settings if needed (for example, for OV3660)
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  // Optionally change the frame size further:
  s->set_framesize(s, FRAMESIZE_CIF);
  
  return true;
}
