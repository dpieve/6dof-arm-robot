#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

constexpr const char* WIFI_SSID     = "esp32-project";
constexpr const char* WIFI_PASSWORD = "123password";

constexpr int SERVO_MIN_PULSE = 125;
constexpr int SERVO_MAX_PULSE = 625;
constexpr int SERVO_MIN_ANGLE = 0;
constexpr int SERVO_MAX_ANGLE = 180;

constexpr int NUM_SERVOS = 6;

constexpr unsigned long SERVO_UPDATE_INTERVAL_MS = 20;

WiFiServer server(80);
Adafruit_PWMServoDriver servoDriver(0x40);

int targetServoAngles[NUM_SERVOS]   = {90, 70, 180, 0, 90, 130};
int currentServoAngles[NUM_SERVOS]  = {90, 70, 180, 0, 90, 130};

int servoSpeed = 2;

int lastServoSequence[NUM_SERVOS] = {0, 0, 0, 0, 0, 0};
int lastSpeedSequence = 0;

unsigned long lastServoUpdate = 0;

void setup() {
  Wire.begin(21, 22);

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  server.begin();

  servoDriver.begin();
  servoDriver.setPWMFreq(60); 
  delay(20);

  for (int i = 0; i < NUM_SERVOS; i++) {
    setServoAngle(i, currentServoAngles[i]);
  }
}

void loop() {
  handleClient();
  updateServos();
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  String request;
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (c == '\n' && request.endsWith("\r\n\r\n")) {
        break;
      }
    }
  }

  int seqVal = parseParameter(request, "seq");
  if (seqVal < 0) {
    seqVal = 0;
  }

  if (request.indexOf("GET /slider?servo=") >= 0) {
    int servoIndex = parseParameter(request, "servo");
    int newAngle   = parseParameter(request, "angle");

    if (servoIndex >= 0 && servoIndex < NUM_SERVOS) {
      if (seqVal > lastServoSequence[servoIndex]) {
        lastServoSequence[servoIndex] = seqVal;
        if (newAngle >= SERVO_MIN_ANGLE && newAngle <= SERVO_MAX_ANGLE) {
          targetServoAngles[servoIndex] = newAngle;
        }
      }
    }
  }

  if (request.indexOf("speed=") >= 0) {
    int newSpeed = parseParameter(request, "speed");
    if (seqVal > lastSpeedSequence) {
      lastSpeedSequence = seqVal;
      if (newSpeed >= 1 && newSpeed <= 10) {
        servoSpeed = newSpeed;
      }
    }
  }

  sendWebPage(client);
  client.stop();
}

void updateServos() {
  if (millis() - lastServoUpdate < SERVO_UPDATE_INTERVAL_MS) return;
  lastServoUpdate = millis();

  for (int i = 0; i < NUM_SERVOS; i++) {
    if (currentServoAngles[i] == targetServoAngles[i]) continue;

    int diff = targetServoAngles[i] - currentServoAngles[i];
    int step = constrain(servoSpeed, 1, abs(diff));

    currentServoAngles[i] += (diff > 0) ? step : -step;
    setServoAngle(i, currentServoAngles[i]);
  }
}

void setServoAngle(int channel, int angle) {
  angle = constrain(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  int pulse = map(angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
  servoDriver.setPWM(channel, 0, pulse);
}

void sendWebPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html>");
  client.println("<head>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>ESP32 6-Servo Control</title>");
  client.println("<style>");
  client.println("  html { font-family: Helvetica; text-align:center; padding:0; margin:0; }");
  client.println("  body { margin: 0 auto; padding: 0; max-width: 600px; }");
  client.println("  h1 { margin-top: 20px; }");
  client.println("  .servo-container { margin: 20px auto; border: 1px solid #ccc; padding: 10px; }");
  client.println("  .slider-label { margin-bottom: 6px; font-weight: bold; }");
  client.println("  input[type=range] { width: 80%; max-width: 300px; }");
  client.println("</style>");
  client.println("</head><body>");

  client.println("<h1>6DOF Arm Robot</h1>");

  for (int i = 0; i < NUM_SERVOS; i++) {
    client.println("<div class='servo-container'>");
    client.println("<h3>Servo " + String(i) + "</h3>");

    client.println("<div class='slider-label'>Angle:</div>");
    client.println("<input type='range' min='0' max='180' value='" +
                   String(targetServoAngles[i]) + "' "
                   "id='servoSlider" + String(i) + "' "
                   "oninput='updateServoAngle(" + String(i) + ")'>");

    client.println("<p>Angle: <span id='angleValue" + String(i) + "'>" +
                   String(targetServoAngles[i]) + "</span>&deg;</p>");
    client.println("</div>");
  }

  client.println("<div class='servo-container'>");
  client.println("<h3>Global Speed</h3>");
  client.println("<div class='slider-label'>Speed (1=Slow, 10=Fast):</div>");
  client.println("<input type='range' min='1' max='10' value='" +
                 String(servoSpeed) +
                 "' id='speedSlider' oninput='updateSpeed()'>");
  client.println("<p>Speed: <span id='speedValue'>" + String(servoSpeed) + "</span></p>");
  client.println("</div>");

  client.println("<script>");
  client.println("var updateSeq = 1;");

  client.println("function updateServoAngle(index) {");
  client.println("  updateSeq++;");
  client.println("  var angle = document.getElementById('servoSlider' + index).value;");
  client.println("  document.getElementById('angleValue' + index).innerText = angle;");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.open('GET', '/slider?servo=' + index + '&angle=' + angle + '&seq=' + updateSeq, true);");
  client.println("  xhr.send();");
  client.println("}");

  client.println("function updateSpeed() {");
  client.println("  updateSeq++;");
  client.println("  var speed = document.getElementById('speedSlider').value;");
  client.println("  document.getElementById('speedValue').innerText = speed;");
  client.println("  var xhr = new XMLHttpRequest();");
  client.println("  xhr.open('GET', '/slider?speed=' + speed + '&seq=' + updateSeq, true);");
  client.println("  xhr.send();");
  client.println("}");
  client.println("</script>");

  client.println("</body></html>");
  client.println();
}

int parseParameter(const String& request, const char* paramName) {
  String lookFor = String(paramName) + "=";
  int startIndex = request.indexOf(lookFor);
  if (startIndex < 0) return -1;
  startIndex += lookFor.length();

  int endIndex = request.indexOf('&', startIndex);
  if (endIndex < 0) {
    endIndex = request.indexOf(' ', startIndex);
    if (endIndex < 0) {
      endIndex = request.length();
    }
  }

  String valueStr = request.substring(startIndex, endIndex);
  return valueStr.toInt();
}
