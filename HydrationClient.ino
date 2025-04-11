#include <ArduinoBLE.h>
#include <Wire.h>
#include <string>

#include "calorie_model_float.h"
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <math.h>

// === Tensor Arena ===
constexpr int kTensorArenaSize = 24 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// === TensorFlow Lite Globals ===
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// BLE UUIDs
#define SERVICE_UUID         "12345678-1234-5678-1234-56789abcdef0"
#define WATER_UUID           "abcdef01-1234-5678-1234-56789abcdef0"
#define AGE_CHAR_UUID        "abcdef02-1234-5678-1234-56789abcdef0"
#define HEIGHT_CHAR_UUID     "abcdef03-1234-5678-1234-56789abcdef0"
#define WEIGHT_CHAR_UUID     "abcdef04-1234-5678-1234-56789abcdef0"
#define DURATION_CHAR_UUID   "abcdef05-1234-5678-1234-56789abcdef0"
#define HEARTRATE_CHAR_UUID  "abcdef06-1234-5678-1234-56789abcdef0"
#define PREDICTION_CHAR_UUID "abcdef07-1234-5678-1234-56789abcdef0"
#define GENDER_CHAR_UUID     "abcdef08-1234-5678-1234-56789abcdef0"

// I2C flow sensor settings
#define SENSOR_ADDRESS 0x08
#define CMSTART 0x3608
#define CMSTOP  0x3FF9

// BLE service and characteristics
BLEService customService(SERVICE_UUID);

BLECharacteristic waterCharacteristic(WATER_UUID, BLERead | BLENotify, 2);
BLECharacteristic ageCharacteristic(AGE_CHAR_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse, 2);
BLECharacteristic heightCharacteristic(HEIGHT_CHAR_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse, 2);
BLECharacteristic weightCharacteristic(WEIGHT_CHAR_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse, 2);
BLECharacteristic durationCharacteristic(DURATION_CHAR_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse, 2);
BLECharacteristic heartRateCharacteristic(HEARTRATE_CHAR_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse, 2);
BLECharacteristic predictionCharacteristic(PREDICTION_CHAR_UUID, BLERead | BLENotify, 2);
BLECharacteristic genderCharacteristic(GENDER_CHAR_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse, 2);

// Data variables
uint16_t waterValue = 0;
uint16_t totalWater = 0;
uint16_t age = 25, height = 175, weight = 66;
uint16_t duration = 0, heartRate = 170, prediction = 0, gender = 1;

void start_meas_command() {
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(highByte(CMSTART));
  Wire.write(lowByte(CMSTART));
  delay(25);
  Wire.endTransmission();
}

void stop_meas_command() {
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(highByte(CMSTOP));
  Wire.write(lowByte(CMSTOP));
  Wire.endTransmission();
}

int16_t read_meas_data() {
  Wire.requestFrom(SENSOR_ADDRESS, 9);
  if (Wire.available() == 9) {
    uint8_t data[9];
    for (int i = 0; i < 9; i++) {
      data[i] = Wire.read();
    }
    int16_t flow = (data[0] << 8) | data[1];
    int16_t flow_ml = flow / 32;
    Serial.print("Flow rate (mL/min): ");
    Serial.println(flow_ml);
    return flow_ml;
  } else {
    Serial.println("Error: Incomplete measurement data...");
    return 0;
  }
}

uint16_t getPrediction(uint16_t gender1, uint16_t age1, uint16_t weight1, uint16_t duration1, uint16_t hr1) {
  float raw_input[5] = {
  ((float)age1     - 42.7898f)     / 16.97969815f,
  ((float)weight1  - 74.96686667f) / 15.03515554f,
  ((float)duration1- 15.5306f)     / 8.31892603f,
  ((float)hr1      - 95.51853333f) / 9.58300874f,
  ((float)gender1  - 0.49646667f)  / 0.49998752f
  };


  Serial.println("\n🧪 === Model Inputs ===");
  Serial.print("👤 Age: "); Serial.println(age1);
  Serial.print("⚖️  Weight (kg): "); Serial.println(weight1);
  Serial.print("🚻 Gender: "); Serial.println(gender1 == 1 ? "Male" : "Female");
  Serial.print("⏱️  Duration (min): "); Serial.println(duration1);
  Serial.print("❤️ Heart Rate (bpm): "); Serial.println(hr1);

  for (int i = 0; i < 5; i++) {
    input->data.f[i] = raw_input[i];
  }

  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("❌ Inference failed");
    delay(3000);
    return 0;
  }

  float calories_burned = output->data.f[0];
  float water_loss_mL = calories_burned / 2.4;
  float sun_water_mL = (duration1 / 60.0) * 700.0;
  float total_water_mL = water_loss_mL + sun_water_mL;
  float total_water_L = total_water_mL / 1000.0;

  Serial.println("\n🔁 === Output Metrics ===");
  Serial.print("🔥 Calories Burned: ");
  Serial.println(calories_burned, 1);
  Serial.print("💧 Water Lost (mL): ");
  Serial.println(water_loss_mL, 1);
  Serial.print("☀️  Sun Exposure Water (mL): ");
  Serial.println(sun_water_mL, 1);
  Serial.print("📦 Total Water Needed (mL): ");
  Serial.print(total_water_mL, 1);
  Serial.print("  (");
  Serial.print(total_water_L, 2);
  Serial.println(" L)");
  Serial.println("============================\n");

  return (uint16_t)roundf(total_water_mL);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  model = tflite::GetModel(calorie_nn_float_tflite);
  static tflite::AllOpsResolver resolver;
  interpreter = new tflite::MicroInterpreter(model, resolver, tensor_arena, kTensorArenaSize);
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("❌ Tensor allocation failed");
    while (1);
  }
  input = interpreter->input(0);
  output = interpreter->output(0);
  Serial.println("✅ ML Initialized");

  if (!BLE.begin()) {
    Serial.println("BLE failed!");
    while (1);
  }

  // IF YOU WANT TO PLACE YOUR CODE MADDI, DO IT HERE!

  BLE.setLocalName("NANO33");
  BLE.setDeviceName("NANO33");
  BLE.setAdvertisedService(customService);

  customService.addCharacteristic(waterCharacteristic);
  customService.addCharacteristic(ageCharacteristic);
  customService.addCharacteristic(heightCharacteristic);
  customService.addCharacteristic(weightCharacteristic);
  customService.addCharacteristic(durationCharacteristic);
  customService.addCharacteristic(heartRateCharacteristic);
  customService.addCharacteristic(predictionCharacteristic);
  customService.addCharacteristic(genderCharacteristic);

  BLE.addService(customService);
  BLE.advertise();
  Serial.println("📡 BLE device is now advertising...");

  waterCharacteristic.writeValue((uint8_t *)&waterValue, 2);
  ageCharacteristic.writeValue((uint8_t *)&age, 2);
  heightCharacteristic.writeValue((uint8_t *)&height, 2);
  weightCharacteristic.writeValue((uint8_t *)&weight, 2);
  durationCharacteristic.writeValue((uint8_t *)&duration, 2);
  heartRateCharacteristic.writeValue((uint8_t *)&heartRate, 2);
  predictionCharacteristic.writeValue((uint8_t *)&prediction, 2);
  genderCharacteristic.writeValue((uint8_t *)&gender, 2);
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());

    start_meas_command();

    while (central.connected()) {
      delay(1000);
      int16_t flow_ml = read_meas_data();
      totalWater += (uint16_t)((float)flow_ml / 60);
      waterCharacteristic.writeValue((uint16_t *)&totalWater, 2);

      if (durationCharacteristic.written()) {
        durationCharacteristic.readValue((uint8_t *)&duration, 2);
        prediction = getPrediction(gender, age, weight, duration, heartRate);
        predictionCharacteristic.writeValue((uint16_t *)&prediction, 2);
        Serial.print("Prediction sent: ");
        Serial.println(prediction);
      }

      if (ageCharacteristic.written()) ageCharacteristic.readValue((uint8_t *)&age, 2);
      if (heightCharacteristic.written()) heightCharacteristic.readValue((uint8_t *)&height, 2);
      if (weightCharacteristic.written()) weightCharacteristic.readValue((uint8_t *)&weight, 2);
      if (heartRateCharacteristic.written()) heartRateCharacteristic.readValue((uint8_t *)&heartRate, 2);
      if (genderCharacteristic.written()) genderCharacteristic.readValue((uint8_t *)&gender, 2);
    }

    stop_meas_command();
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}

