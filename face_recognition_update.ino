#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_camera.h"

// ================= WiFi 配置 =================
const char* ssid = "华为畅享 60";
const char* password = "qwertyuiop";
const char* faceServerUrl = "http://192.168.43.221:5000/upload";       
const char* objectServerUrl = "http://192.168.43.221:5000/upload_object"; 

// ================= 引脚定义 =================
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// OLED 引脚
#define OLED_SDA 14
#define OLED_SCL 15
#define OLED_ADDR 0x3C

// 按钮引脚（已修复：物体识别按钮改用 GPIO 13）
#define BUTTON_FACE 12   
#define BUTTON_OBJECT 13  // 原来 GPIO 4 是闪光灯引脚，已更换

// 蜂鸣器引脚
#define BUZZER_PIN 2  // 原来 GPIO 13，现在改用 GPIO 2

// ================= OLED 初始化 =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= 状态变量 =================
bool faceRecognized = false;
String lastFaceName = "";
bool isProcessing = false;
bool oledWorking = false;

// 按钮状态追踪
bool lastBtnFaceState = HIGH;
bool lastBtnObjState = HIGH;

// ================= 初始化摄像头 =================
void initCamera() {
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
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("摄像头初始化失败：0x%x\n", err);
    return;
  }
  Serial.println("摄像头初始化成功");
}

// ================= OLED 自动换行显示（保留单词边界） =================

/**
 * 绘制自动换行文本，以单词为单位换行，并更新 y 坐标
 * param text 要显示的文本
 * param y    起始 y 坐标（传入引用，函数内部会更新）
 */
void drawWrappedText(const String &text, int16_t &y) {
    // 去除首尾空格，避免空字符串处理
    String trimmed = text;
    trimmed.trim();
    if (trimmed.length() == 0) return;

    // 分割单词（按空格分隔）
    const int MAX_WORDS = 50;           // 假设最多 50 个单词
    String words[MAX_WORDS];
    int wordCount = 0;

    int start = 0;
    int end = trimmed.indexOf(' ');
    while (end != -1 && wordCount < MAX_WORDS) {
        String word = trimmed.substring(start, end);
        if (word.length() > 0) {
            words[wordCount++] = word;
        }
        start = end + 1;
        end = trimmed.indexOf(' ', start);
    }
    // 最后一个单词
    if (start < trimmed.length()) {
        String lastWord = trimmed.substring(start);
        if (lastWord.length() > 0) {
            words[wordCount++] = lastWord;
        }
    }

    String currentLine = "";     // 当前行已积累的单词（带空格）

    for (int i = 0; i < wordCount; i++) {
        // 构建候选行（当前行 + 下一个单词）
        String candidate;
        if (currentLine.length() == 0) {
            candidate = words[i];
        } else {
            candidate = currentLine + " " + words[i];
        }

        // 测量候选行的宽度
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(candidate, 0, y, &x1, &y1, &w, &h);

        if (w <= SCREEN_WIDTH) {
            // 候选行宽度未超限，可以加入当前行
            currentLine = candidate;
        } else {
            // 候选行超限，需要换行
            if (currentLine.length() > 0) {
                // 绘制当前行
                display.setCursor(0, y);
                display.print(currentLine);
                y += 8;
                if (y >= SCREEN_HEIGHT) return;   // 屏幕已满，停止绘制
            }
            // 新行从当前单词开始
            currentLine = words[i];
        }
    }

    // 绘制最后一行（如果有内容且屏幕未满）
    if (currentLine.length() > 0 && y < SCREEN_HEIGHT) {
        display.setCursor(0, y);
        display.print(currentLine);
        y += 8;   // 更新 y，便于调用者知道结束位置（实际未使用）
    }
}

/**
 * 显示两行消息（自动换行，保留单词边界）
 * param line1 第一行文本
 * param line2 第二行文本（可选）
 */
void displayMessage(const String& line1, const String& line2 = "") {
    if (!oledWorking) return;

    display.clearDisplay();
    display.setTextSize(1);               
    display.setTextColor(SSD1306_WHITE);  
    int16_t y = 0;   // 起始 y 坐标

    if (line1.length() > 0) {
        drawWrappedText(line1, y);
    }
    if (line2.length() > 0 && y < SCREEN_HEIGHT) {
        drawWrappedText(line2, y);
    }

    display.display();
}

// ================= 蜂鸣器控制 =================
void beepSuccess() {
#ifdef BUZZER_PIN
  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcWriteTone(BUZZER_PIN, 2000);
  delay(150);
  ledcWriteTone(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
#endif
}

void beepFail() {
#ifdef BUZZER_PIN
  ledcAttach(BUZZER_PIN, 2000, 8);
  for (int i = 0; i < 2; i++) {
    ledcWriteTone(BUZZER_PIN, 1000);
    delay(100);
    ledcWriteTone(BUZZER_PIN, 0);
    delay(100);
  }
  ledcDetach(BUZZER_PIN);
#endif
}

// ================= 拍照并发送 =================
String captureAndSend(const char* serverUrl) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("拍照失败");
    return "Error";
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(15000);

  int httpCode = http.POST(fb->buf, fb->len);
  String response = "";
  
  if (httpCode == HTTP_CODE_OK) {
    response = http.getString();
    response.trim();
    Serial.print("服务器返回：");
    Serial.println(response);
  } else {
    Serial.printf("HTTP 错误：%d\n", httpCode);
    response = "Error";
  }

  http.end();
  esp_camera_fb_return(fb);
  return response;
}

// ================= 等待按钮释放 =================
void waitForButtonRelease(int buttonPin) {
  unsigned long startTime = millis();
  while (digitalRead(buttonPin) == LOW && (millis() - startTime < 3000)) {
    delay(10);
  }
  delay(100);
}

// ================= 主循环 =================
void loop() {
  delay(10);
  
  if (isProcessing) {
    delay(50);
    return;
  }

  bool currentBtnFace = digitalRead(BUTTON_FACE);
  bool currentBtnObj = digitalRead(BUTTON_OBJECT);

  // ================= 人脸识别按钮 =================
  if (lastBtnFaceState == HIGH && currentBtnFace == LOW) {
    delay(50);
    if (digitalRead(BUTTON_FACE) == LOW) {
      isProcessing = true;
      Serial.println("【按钮 1 按下】开始人脸识别...");
      displayMessage("Recognizing...", "Face");
      
      String result = captureAndSend(faceServerUrl);
      
      if (result != "Error" && result.length() > 0) {
        lastFaceName = result;
        bool isKnown = (result != "unknown" && result != "Unknown" && result != "Error");
        
        if (isKnown) {
          faceRecognized = true;
          displayMessage("Hello,", lastFaceName);
          beepSuccess();
          Serial.println("人脸识别成功，解锁物体识别功能");
        } else {
          faceRecognized = false;
          displayMessage("Unknown", "Face");
          beepFail();
          Serial.println("人脸识别失败");
        }
      } else {
        displayMessage("Network", "Error");
        beepFail();
        Serial.println("网络错误");
      }
      
      waitForButtonRelease(BUTTON_FACE);
      isProcessing = false;
      lastBtnFaceState = HIGH;
    }
  }
  lastBtnFaceState = currentBtnFace;

  // ================= 物体识别按钮 =================
  if (faceRecognized) {
    if (lastBtnObjState == HIGH && currentBtnObj == LOW) {
      delay(50);
      if (digitalRead(BUTTON_OBJECT) == LOW) {
        isProcessing = true;
        Serial.println("【按钮 2 按下】开始物体识别...");
        displayMessage("Analyzing...", "Object");
        
        String result = captureAndSend(objectServerUrl);
        
        if (result != "Error" && result.length() > 0) {
          displayMessage("Object:", result);   
          beepSuccess();
          Serial.print("物体识别结果：");
          Serial.println(result);
        } 
        else {
          displayMessage("AI", "Error");
          beepFail();
          Serial.println("物体识别失败");
        }
        
        waitForButtonRelease(BUTTON_OBJECT);
        isProcessing = false;
        lastBtnObjState = HIGH;
      }
    }
  }
  lastBtnObjState = currentBtnObj;

  // ================= 调试信息 =================
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 5000) {
    lastDebugTime = millis();
    Serial.printf("调试 - 按钮状态：Face=%d, Obj=%d, 人脸已识别=%d\n", 
                  currentBtnFace, currentBtnObj, faceRecognized);
  }
}

// ================= 初始化 =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== 系统启动 ===");
  Serial.printf("PSRAM: %s (%d 字节)\n", psramFound() ? "YES" : "NO", ESP.getPsramSize());

  // 1. 初始化 OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oledWorking = true;
    displayMessage("System", "Starting...");
    Serial.println("OLED 初始化成功");
  } else {
    oledWorking = false;
    Serial.println("OLED 初始化失败，将继续运行");
  }

  // 2. 连接 WiFi
  WiFi.begin(ssid, password);
  Serial.print("连接 WiFi");
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 15) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi 连接成功");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    if (oledWorking) displayMessage("WiFi", "Connected");
  } else {
    Serial.println("\nWiFi 连接失败");
    if (oledWorking) displayMessage("WiFi", "Failed");
  }

  // 3. 初始化摄像头
  initCamera();
  
  // 4. 初始化按钮
  pinMode(BUTTON_FACE, INPUT_PULLUP);
  pinMode(BUTTON_OBJECT, INPUT_PULLUP);
  // 强制拉高，确保初始状态正确
  digitalWrite(BUTTON_FACE, HIGH);
  digitalWrite(BUTTON_OBJECT, HIGH);
  
  // 5. 初始化蜂鸣器
#ifdef BUZZER_PIN
  ledcAttach(BUZZER_PIN, 2000, 8);
#endif

  if (oledWorking) displayMessage("System", "Ready");
  Serial.println("=== 启动完成 ===");
  Serial.println("提示：按按钮 1 人脸识别，成功后按按钮 2 物体识别");
}





