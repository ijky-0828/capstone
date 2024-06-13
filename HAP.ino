#include <SoftwareSerial.h>
#include <FirebaseESP32.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <time.h>
#include "HUSKYLENS.h"

// Firebase 프로젝트 설정
#define FIREBASE_HOST "https://logmanage-a6ce8-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "9ZujkF3PrGmv4xy2eGOvQL8b8Nm67pj6HAyL0lbq"

// WiFi 설정
#define WIFI_SSID "KY"
#define WIFI_PASSWORD "20000828"

// Twilio 계정 정보
const char* account_sid = "AC010ff35f16897d6349b8c316ab1d79ab";
const char* auth_token = "0160c2903caac3ece484294fc87f6c70";

// Twilio 및 수신자 정보
const char* twilio_phone_number = "+15096318413";
const char* recipient_phone_number = "+821041203106"; // 수신자 전화번호로 변경

// GPS 모듈 시리얼 포트
SoftwareSerial mySerial(26, 34); // TX, RX 핀

// Firebase 객체
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// 센서 및 LED 핀 설정
const int porta_a0 = 35;
const int porta_D0 = 27;
int leitura_porta_analogica = 0;
int leitura_porta_digital = 0;
const int pinoled = 4;

bool shockDetected = false;
int shockLevel = 0;

WiFiClientSecure client;

// NTP 서버 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 9; // 한국 표준시 (KST) 오프셋
const int daylightOffset_sec = 0;

HUSKYLENS huskylens;
// HUSKYLENS green line >> SDA; blue line >> SCL

unsigned long previousMillis = 0; // 이전 시간 저장 변수
const long interval = 1000; // 1초 간격

// Base64 인코딩 함수
String base64_encode(String data) {
  const char* input = data.c_str();
  int inputLen = data.length();
  const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  String encodedData;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (inputLen--) {
    char_array_3[i++] = *(input++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; (i < 4) ; i++)
        encodedData += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      encodedData += base64_chars[char_array_4[j]];

    while ((i++ < 3))
      encodedData += '=';
  }

  return encodedData;
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600); // GPS 모듈을 위한 시리얼 포트

  // WiFi 연결
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("WiFi 연결 중...");
  }
  Serial.println("WiFi 연결 완료!");

  // 시간 동기화
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  // Firebase 설정
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Firebase 초기화
  Firebase.begin(&firebaseConfig, &firebaseAuth);

  // Firebase 연결 상태 확인
  if (Firebase.ready()) {
    Serial.println("Firebase 연결 성공!");
  } else {
    Serial.println("Firebase 연결 실패!");
  }

  // 핀 모드 설정
  pinMode(porta_a0, INPUT);
  pinMode(porta_D0, INPUT);
  pinMode(pinoled, OUTPUT);

  client.setInsecure();

  // HUSKYLENS 초기화
  Wire.begin(21, 22); // ESP32의 기본 I2C 핀 (SDA: 21, SCL: 22)
  while (!huskylens.begin(Wire)) {
    Serial.println(F("Begin failed!"));
    Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS (General Settings>>Protocol Type>>I2C)"));
    Serial.println(F("2.Please recheck the connection."));
    delay(100);
  }
}

void loop() {
  if (!shockDetected) {
    leitura_porta_analogica = analogRead(porta_a0);
    Serial.println(leitura_porta_analogica);

    if (leitura_porta_analogica <= 1000) {
      Serial.println("4단계!");
      shockLevel = 4;
      shockDetected = true;
    } else if (leitura_porta_analogica <= 2000) {
      Serial.println("3단계!");
      shockLevel = 3;
      shockDetected = true;
    } else if (leitura_porta_analogica <= 3000) {
      Serial.println("2단계!");
    } else if (leitura_porta_analogica <= 4094) {
      Serial.println("1단계!");
      shockLevel = 1;
      shockDetected = true;
    }

    leitura_porta_digital = digitalRead(porta_D0);
    if (leitura_porta_digital != 1) {
      digitalWrite(pinoled, HIGH);
      delay(10);
      digitalWrite(pinoled, LOW);
    }
    delay(100);
  } else {
    // 충격 감지 후 동작을 실행
    executeSecondCode(shockLevel);
  }
}

// 두 번째 코드의 기능을 실행하는 함수
void executeSecondCode(int shockLevel) {
  // GPS 데이터 직접 설정
  float latitude = 37.581703;
  float longitude = 127.009952;

  // 현재 시간 가져오기
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeString[64];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // HUSKYLENS 데이터 요청
  int faceCount = 0;
  if (!huskylens.request()) {
    Serial.println(F("Fail to request data from HUSKYLENS, recheck the connection!"));
  } else if(!huskylens.isLearned()) {
    Serial.println(F("Nothing learned, press learn button on HUSKYLENS to learn one!"));
  } else if(!huskylens.available()) {
    Serial.println(F("No block or arrow appears on the screen!"));
  } else {
    Serial.println(F("###########"));
    while (huskylens.available()) {
      HUSKYLENSResult result = huskylens.read();
      if (result.command == COMMAND_RETURN_BLOCK) {
        faceCount++;
      }
    }
    Serial.println(String() + faceCount + F("명"));
  }

  // Firebase에 GPS 데이터, 충격 단계, 얼굴 수 업로드
  uploadToFirebase(latitude, longitude, shockLevel, faceCount, String(timeString));

  // SMS 보내기
  sendSMS(latitude, longitude, shockLevel, faceCount, String(timeString));

  // 충격 감지 상태 초기화
  shockDetected = false;
}

void uploadToFirebase(float latitude, float longitude, int shockLevel, int faceCount, String timestamp) {
  // Firebase에 업로드할 데이터 구조
  FirebaseJson json;
  json.set("latitude", latitude);
  json.set("longitude", longitude);
  json.set("shockLevel", shockLevel);
  json.set("faceCount", faceCount);
  json.set("timestamp", timestamp);

  // 이벤트마다 고유한 키 생성 (예: 타임스탬프 사용)
  String eventKey = String(timestamp); // 타임스탬프를 키로 사용 (다른 고유한 값 사용 가능)

  // 하위 경로 생성
  String path = "/157너 7019/event_data/" + eventKey;

  // Firebase에 데이터 업로드
  if (Firebase.setJSON(firebaseData, path.c_str(), json)) {
    Serial.println("Event data uploaded successfully.");
  } else {
    Serial.println("Failed to upload event data.");
    Serial.println(firebaseData.errorReason());
  }
}

void sendSMS(float latitude, float longitude, int shockLevel, int faceCount, String timestamp) {
  // 메시지 본문 작성
  String message_body = String(shockLevel) + "단계 충격 발생 (시간: " + timestamp + ", 위치: ";
  message_body += String(latitude, 6);
  message_body += ", ";
  message_body += String(longitude, 6);
  message_body += "), 얼굴 수: ";
  message_body += String(faceCount);

  // Twilio API URL
  String url = "/2010-04-01/Accounts/" + String(account_sid) + "/Messages.json";

  // HTTP 요청 본문 작성
  String postData = "To=" + String(recipient_phone_number) + "&From=" + String(twilio_phone_number) + "&Body=" + message_body;

  // Twilio 서버에 연결
  if (client.connect("api.twilio.com", 443)) {
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: api.twilio.com");
    client.println("Authorization: Basic " + base64_encode(String(account_sid) + ":" + String(auth_token)));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Content-Length: " + String(postData.length()));
    client.println();
    client.println(postData);

    // 응답 확인
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("Headers received");
        break;
      }
    }
    // 본문 확인
    String response = client.readStringUntil('\n');
    Serial.println("Response: " + response);
  } else {
    Serial.println("Failed to connect to Twilio");
  }
}
