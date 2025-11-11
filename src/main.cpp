#include <Arduino.h>        
#include <Wire.h>           
#include <LiquidCrystal_I2C.h> 
#include "HX711.h"          
#include <WiFi.h>           
#include <esp_now.h>
#include <ESP32Servo.h>     

// --- Cau hinh LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 
#define I2C_SDA 21
#define I2C_SCL 22

// --- Cau hinh HX711 ---
const int LOADCELL_DOUT_PIN = 4;
const int LOADCELL_SCK_PIN = 5;
HX711 scale;

// --- Cau hinh Servo ---
#define SERVO_PIN 17
Servo myServo;

// --- Cau hinh ESP-NOW ---
// Địa chỉ MAC của ESP nhận - THAY ĐỔI ĐỊA CHỈ MAC NÀY THEO ESP CỦA BẠN
uint8_t broadcastAddress[] = {0x3C, 0x71, 0xBF, 0x12, 0x34, 0x56};
// Ví dụ: Nếu MAC là 3C:71:BF:12:34:56 thì viết {0x3C, 0x71, 0xBF, 0x12, 0x34, 0x56}

typedef struct struct_message {
  float weight;
} struct_message;

struct_message myData;

// --- He so hieu chuan ---
float calibration_factor = 401.94;

// --- Bien cho May trang thai (State Machine) ---
enum ScaleState {
  CONNECTING,
  WAITING,
  MEASURING,
  DISPLAYING
};
ScaleState currentState = CONNECTING;
bool hasDisplayed = false;
bool isConnected = false;

unsigned long measurementStartTime = 0;     
float finalWeight = 0.0;          

// --- Cau hinh ---
const float TRIGGER_WEIGHT = 30.0; // Nguong de bat dau can (gram)
const float REMOVE_WEIGHT = 10.0;  // Nguong de reset (gram)
const float DEAD_ZONE = 2.0;       
const int MEASURE_TIME = 5000;     // Thoi gian do (5 giay)

// --- Cau hinh Servo ---
const int SERVO_STEP_DELAY = 25;    // Độ trễ giữa các bước cho phân loại (ms)
const int SERVO_STEP_SIZE = 2;      // Độ lớn mỗi bước quay cho phân loại (độ)
const int RETURN_STEP_DELAY = 25;   // Độ trễ giữa các bước khi đưa về giữa (ms)
const int RETURN_STEP_SIZE = 2;     // Độ lớn mỗi bước quay khi đưa về giữa (độ)

// Hàm callback khi gửi dữ liệu thành công
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Gửi dữ liệu - Trạng thái: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Thành công");
    isConnected = true;  // Đánh dấu đã kết nối
  } else {
    Serial.println("Thất bại");
  }
}

// Hàm callback khi nhận dữ liệu
void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Nhận dữ liệu từ MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.print("Khoi luong nhan duoc: ");
  Serial.print(myData.weight);
  Serial.println(" kg");
}

// Hàm gửi kết quả cân nặng 
void sendWeightResult(float weight_kg) {
  myData.weight = weight_kg;
  
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  if (result == ESP_OK) {
    Serial.printf("Gửi khoi luong: %.3f kg - Thành công\n", weight_kg);
  } else {
    Serial.printf("Gửi khoi luong: %.3f kg - Thất bại\n", weight_kg);
  }
}

// Hàm di chuyển servo từ từ
void moveServoSlowly(int startAngle, int endAngle, bool isReturning = false) {
  int stepDelay = isReturning ? RETURN_STEP_DELAY : SERVO_STEP_DELAY;
  int stepSize = isReturning ? RETURN_STEP_SIZE : SERVO_STEP_SIZE;
  
  if (startAngle < endAngle) {
    for (int angle = startAngle; angle <= endAngle; angle += stepSize) {
      myServo.write(angle);
      delay(stepDelay);
    }
  } else {
    for (int angle = startAngle; angle >= endAngle; angle -= stepSize) {
      myServo.write(angle);
      delay(stepDelay);
    }
  }
  myServo.write(endAngle); // Đảm bảo đến đúng vị trí cuối
}

void setup() {
  Serial.begin(115200);
  Serial.print("Dia chi MAC cua ESP (sender): ");
  Serial.println(WiFi.macAddress());

  // Khởi động HX711
  Serial.println("Khoi dong HX711...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  Serial.println("HX711 san sang.");

  // Khởi động LCD
  Serial.println("Khoi dong LCD I2C...");
  Wire.begin(I2C_SDA, I2C_SCL); 
  lcd.init();
  lcd.backlight();
  
  // Khởi động Servo
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(90);  // Đưa servo về vị trí giữa
  
  // Khởi động ESP-NOW
  Serial.println("Khoi dong ESP-NOW...");
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Khoi dong ESP-NOW that bai!");
    return;
  }
  
  // Đăng ký hàm callback
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  
  // Thêm peer (thiết bị nhận)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Them peer that bai!");
    return;
  }
  
  Serial.println("ESP-NOW san sang.");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ket noi voi bang");
  lcd.setCursor(0, 1);
  lcd.print("chuyen...");
  Serial.println("Dang o trang thai CONNECTING.");
}

void loop() {
  // --- LENH TRU BI KHAN CAP ---
  if (Serial.available()) {
    char temp = Serial.read();
    if ((temp == 't' || temp == 'T') && (currentState == WAITING || currentState == DISPLAYING)) {
      scale.tare(); 
      Serial.println("DA TRU BI!");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DA TRU BI!");
      delay(1000); 
      currentState = WAITING; 
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("San sang can!");
    }
  }

  // --- CO MAY TRANG THAI CHINH ---
  switch (currentState) {
    case CONNECTING: {
      // Gửi gói dữ liệu để kiểm tra kết nối
      myData.weight = 0.0;
      esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
      
      if (isConnected) {
        // Đã kết nối thành công
        Serial.println("Da ket noi voi ESP kia!");
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Da ket noi!");
        delay(2000);
        
        // Chuyển sang trạng thái WAITING
        currentState = WAITING;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("San sang can!");
      }
      break;
    }
    
    case WAITING: {
      float currentWeight = scale.get_units(5);
      
      // === PHAN SUA DOI DE LOAI BO NHAY SO VA SO AM ===
      float displayWeight_kg;

      // Neu khoi luong am hoac nam trong "vung chet", thi coi la 0
      if (currentWeight <= 0 || (currentWeight > -DEAD_ZONE && currentWeight < DEAD_ZONE)) {
        displayWeight_kg = 0.0;
      } else {
        displayWeight_kg = currentWeight / 1000.0;
      }
      
      // Hien thi "live" da duoc xu ly
      lcd.setCursor(0, 1);
      lcd.print(displayWeight_kg, 3);
      lcd.print(" kg "); // Co khoang trang de xoa so cu
      // === KET THUC SUA DOI ===

      // Kiem tra de bat dau can (van dung gia tri GOC)
      if (currentWeight > TRIGGER_WEIGHT) {
        Serial.println("Phat hien vat nang > 30g. Bat dau do...");
        currentState = MEASURING;
        measurementStartTime = millis(); 
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Dang do...");
      }
      break;
    }
    
    case MEASURING: {
      unsigned long elapsed = millis() - measurementStartTime;
      
      // Đọc cân nặng hiện tại
      float currentWeight = scale.get_units(3);  // Lấy 3 mẫu để đọc nhanh
      float currentWeight_kg = currentWeight / 1000.0;

      // Hiển thị "Dang do" và cân nặng
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Dang do...");
      
      // Hiển thị cân nặng ở hàng 2
      lcd.setCursor(0, 1);
      if (currentWeight <= 0 || (currentWeight > -DEAD_ZONE && currentWeight < DEAD_ZONE)) {
        lcd.print("0.000 kg");
      } else {
        lcd.print(currentWeight_kg, 3);
        lcd.print(" kg");
      }

      // Kiểm tra hết giờ
      if (elapsed >= MEASURE_TIME) {
        Serial.println("Het gio. Lay ket qua.");
        finalWeight = scale.get_units(10);  // Lấy kết quả cuối với 10 mẫu
        hasDisplayed = false;
        currentState = DISPLAYING;
      }
      break;
    }
    
    case DISPLAYING: {
      // --- KHOI LOGIC CHI CHAY MOT LAN ---
      if (!hasDisplayed) {
        // Chuyen sang kg và hiển thị kết quả
        float finalWeight_kg = finalWeight / 1000.0;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Khoi luong:");
        lcd.setCursor(0, 1);
        lcd.print(finalWeight_kg, 3);
        lcd.print(" kg");
        
        delay(2000); // Hiển thị kết quả 2 giây
        
        // Thông báo đẩy xuống băng chuyền
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Day xuong");
        lcd.setCursor(0, 1);
        lcd.print("bang chuyen...");
        
        delay(2000); // Chờ 2 giây
        
        // Điều khiển servo theo trọng lượng
        int currentAngle = myServo.read();
        if (finalWeight > 100) {
          Serial.println("Vat > 100g, quay servo sang trai.");
          moveServoSlowly(currentAngle, 0);
          sendWeightResult(finalWeight_kg);
        } else {
          Serial.println("Vat <= 100g, quay servo sang phai.");
          moveServoSlowly(currentAngle, 180);
          sendWeightResult(finalWeight_kg);
        }
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Cho lay hang...");
        
        hasDisplayed = true;
      }

      // Kiểm tra cân đã về 0 chưa
      float currentWeight = scale.get_units(5);
      if (currentWeight < REMOVE_WEIGHT) {
        // Chờ và kiểm tra lại để đảm bảo cân đã ổn định
        delay(500);
        currentWeight = scale.get_units(5);
        
        if (currentWeight < REMOVE_WEIGHT) {
          Serial.println("Can da ve 0, dua servo ve giua...");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Dua servo ve...");
          
          // Đưa servo về giữa với tốc độ nhanh hơn
          int currentAngle = myServo.read();
          moveServoSlowly(currentAngle, 90, true);  // true = sử dụng tốc độ trả về
          
          currentState = WAITING;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("San sang can!");
        }
      }
      break;
    }
  }

  delay(200);

  delay(200); 
}