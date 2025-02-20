/*
  Combined Minimal Code for TTGO T-Camera V1.6.2
  ------------------------------------------------
  1. Initializes WiFi and the camera (using select_pins.h)
  2. In loop():
     - Captures a frame and sends it to Gemini Vision API with the prompt
       "Please analyze the image content"
     - Receives and prints the Gemini response
     - Sends a follow-up message (asking for a behavior analysis)
     - Prints that response
     - Repeats after a delay
*/

#define CAMERA_MODEL_TTGO_T_CAMERA_V162  // Board model for TTGO T-Camera V1.6.2
#include "select_pins.h"

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Base64.h"
#include <ArduinoJson.h>
#include <Arduino.h>

//––– WiFi and Gemini Credentials –––
char wifi_ssid[] = "SSID";
char wifi_pass[] = "Password";
String gemini_Key = "Gemini_API_Key";

//––– WiFi Initialization Function –––
void initWiFi() {
  for (int i = 0; i < 2; i++) {
    WiFi.begin(wifi_ssid, wifi_pass);
    delay(1000);
    Serial.println("");
    Serial.print("Connecting to ");
    Serial.println(wifi_ssid);

    long int StartTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if ((StartTime + 5000) < millis()) break;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("STA IP address:");
      Serial.println(WiFi.localIP());
      Serial.println("");
      break;
    }
  }
}

//––– Gemini Vision Function –––
String SendStillToGeminiVision(String key, String message) {
  WiFiClientSecure client_tcp;
  client_tcp.setInsecure();
  const char* myDomain = "generativelanguage.googleapis.com";
  Serial.println("Connect to " + String(myDomain));
  String getResponse = "", Feedback = "";
  if (client_tcp.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    // Capture a frame from the camera:
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      ESP.restart();
      return "Camera capture failed";
    }
    
    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];
    String imageFile = "";
    // Encode the image to base64 (implementation left unchanged)
    for (int i = 0; i < fb->len; i++) {
      base64_encode(output, (input++), 3);
      if (i % 3 == 0) imageFile += String(output);
    }
    
    String Data = "{\"contents\": [{\"parts\": [{ \"text\": \"" + message +
                  "\"}, {\"inline_data\": {\"mime_type\": \"image/jpeg\", \"data\": \"" +
                  imageFile + "\"}}]}]}";
    
    client_tcp.println("POST /v1beta/models/gemini-1.5-flash-latest:generateContent?key=" + key + " HTTP/1.1");
    client_tcp.println("Connection: close");
    client_tcp.println("Host: " + String(myDomain));
    client_tcp.println("Content-Type: application/json; charset=utf-8");
    client_tcp.println("Content-Length: " + String(Data.length()));
    client_tcp.println("Connection: close");
    client_tcp.println();
    
    for (int Index = 0; Index < Data.length(); Index += 1024) {
      client_tcp.print(Data.substring(Index, Index + 1024));
    }
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;
    long startTime = millis();
    boolean state = false;
    boolean markState = false;
    while ((startTime + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (client_tcp.available()) {
        char c = client_tcp.read();
        if (String(c) == "{") markState = true;
        if (state == true && markState == true) Feedback += String(c);
        if (c == '\n') {
          if (getResponse.length() == 0) state = true;
          getResponse = "";
        } else if (c != '\r') {
          getResponse += String(c);
        }
        startTime = millis();
      }
      if (Feedback.length() > 0) break;
    }
    client_tcp.stop();
    Serial.println("");
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, Feedback);
    JsonObject obj = doc.as<JsonObject>();
    getResponse = obj["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    if (getResponse == "null")
      getResponse = obj["error"]["message"].as<String>();
  } else {
    getResponse = "Connected to " + String(myDomain) + " failed.";
    Serial.println("Connected to " + String(myDomain) + " failed.");
  }
  
  return getResponse;
}

//––– Gemini Chat Function –––
String SendMessageToGemini(String key, String behavior, String message) {
  WiFiClientSecure client_tcp;
  client_tcp.setInsecure();
  const char* myDomain = "generativelanguage.googleapis.com";
  Serial.println("Connect to " + String(myDomain));
  String getResponse = "", Feedback = "";
  if (client_tcp.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    String Data = "{\"contents\": [{\"parts\":[{\"text\": \"" + behavior +
                  "\"}, {\"text\": \"" + message + "\"}]}]}";
    
    client_tcp.println("POST /v1beta/models/gemini-1.5-flash-latest:generateContent?key=" + key + " HTTP/1.1");
    client_tcp.println("Connection: close");
    client_tcp.println("Host: " + String(myDomain));
    client_tcp.println("Content-Type: application/json; charset=utf-8");
    client_tcp.println("Content-Length: " + String(Data.length()));
    client_tcp.println("Connection: close");
    client_tcp.println();
    
    for (int Index = 0; Index < Data.length(); Index += 1024) {
      client_tcp.print(Data.substring(Index, Index + 1024));
    }
    
    int waitTime = 10000;
    long startTime = millis();
    boolean state = false;
    boolean markState = false;
    while ((startTime + waitTime) > millis()) {
      Serial.print(".");
      delay(100);
      while (client_tcp.available()) {
        char c = client_tcp.read();
        if (String(c) == "{") markState = true;
        if (state == true && markState == true) Feedback += String(c);
        if (c == '\n') {
          if (getResponse.length() == 0) state = true;
          getResponse = "";
        } else if (c != '\r') {
          getResponse += String(c);
        }
        startTime = millis();
      }
      if (Feedback.length() > 0) break;
    }
    client_tcp.stop();
    Serial.println("");
    Serial.println(Feedback);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, Feedback);
    JsonObject obj = doc.as<JsonObject>();
    getResponse = obj["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    if (getResponse == "null")
      getResponse = obj["error"]["message"].as<String>();
  } else {
    getResponse = "Connected to " + String(myDomain) + " failed.";
    Serial.println("Connected to " + String(myDomain) + " failed.");
  }
  
  return getResponse;
}

//––– Setup –––
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Disable brown-out detection for stability.
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize WiFi.
  initWiFi();
  
  // ---- Camera Configuration (using TTGO T-Camera V1.6.2 pin definitions) ----
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  
  // Use pin definitions from select_pins.h:
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
  
  // Use higher resolution if PSRAM is available.
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    delay(1000);
    ESP.restart();
  }
  Serial.println("Camera init succeeded");
  
  // Optional sensor adjustments.
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  // Set frame size to CIF (352x288) for Gemini Vision compatibility.
  s->set_framesize(s, FRAMESIZE_CIF);
  
  // Wait a few seconds before entering the loop.
  delay(5000);
}

//––– Loop –––
void loop() {
  String response = "";
  String gemini_Chat = "Give what is in the image in ten words";
  
  // Capture a frame and send it to Gemini Vision API.
  response = SendStillToGeminiVision(gemini_Key, gemini_Chat);
  Serial.print("Gemini Vision Response:\t");
  Serial.println(response);
  
  // Prepare a follow-up message using guidelines.
  String gemini_behavior = "Please follow the guidelines: (1) If the scenario description indicates the presence of people, return '1'. (2) If the scenario description indicates there are no people, return '0'. (3) If it cannot be determined, return '-1'. (4) Do not provide additional explanations.";
  gemini_Chat = response;
  
  // Send a follow-up message to Gemini API.
  //response = SendMessageToGemini(gemini_Key, gemini_behavior, gemini_Chat);
  //Serial.println("Gemini Chat Response:");
  //Serial.println(response);
  
  // Wait 10 seconds before capturing the next frame.
  delay(5000);
}
