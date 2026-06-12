#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define configUSE_TICKLESS_IDLE 1

// Pin Configuration
#define DHTPIN 4
#define DHTTYPE DHT22

#define LED_PIN 26
#define BUZZER_PIN 27
#define BUTTON_PIN 18

// Object Initialization
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Sensor Data Structure
typedef struct {
  float temperature;
  float humidity;
} SensorData;

// RTOS Objects
QueueHandle_t sensorQueue;

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t emergencySemaphore;

// Global Variables
volatile uint32_t samplingInterval = 5000;

String currentMode = "NORMAL";
String powerState = "LOW POWER";

// Button ISR
void IRAM_ATTR buttonISR() {

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  xSemaphoreGiveFromISR(
    emergencySemaphore,
    &xHigherPriorityTaskWoken
  );

  portYIELD_FROM_ISR();
}

// Sensor Task
void SensorTask(void *pvParameters) {

  SensorData sensor;

  while (1) {

    sensor.temperature =
      dht.readTemperature();

    sensor.humidity =
      dht.readHumidity();

    if (!isnan(sensor.temperature) &&
        !isnan(sensor.humidity)) {

      xQueueOverwrite(
        sensorQueue,
        &sensor
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(
        samplingInterval
      )
    );
  }
}

// Alarm Task
void AlarmTask(void *pvParameters) {

  SensorData sensor;

  while (1) {

    if (
      xQueuePeek(
        sensorQueue,
        &sensor,
        portMAX_DELAY
      ) == pdTRUE
    ) {

      // Normal Mode
      if (
        sensor.temperature < 35
      ) {

        digitalWrite(
          LED_PIN,
          LOW
        );

        noTone(
          BUZZER_PIN
        );

        currentMode =
          "NORMAL";
      }

      // Warning Mode
      else if (
        sensor.temperature >= 35 &&
        sensor.temperature <= 40
      ) {

        digitalWrite(
          LED_PIN,
          LOW
        );

        noTone(
          BUZZER_PIN
        );

        currentMode =
          "WASPADA";
      }

      // Danger Mode
      else {

        digitalWrite(
          LED_PIN,
          HIGH
        );

        tone(
          BUZZER_PIN,
          1000
        );

        currentMode =
          "BAHAYA";
      }
    }

    vTaskDelay(
      pdMS_TO_TICKS(100)
    );
  }
}

// Power Management Task
void PowerMgmtTask(
  void *pvParameters
) {

  SensorData sensor;

  while (1) {

    if (
      xQueuePeek(
        sensorQueue,
        &sensor,
        portMAX_DELAY
      ) == pdTRUE
    ) {

      // Low Power Mode
      if (
        sensor.temperature < 35
      ) {

        samplingInterval =
          5000;

        powerState =
          "LOW POWER";
      }

      // Balanced Mode
      else if (
        sensor.temperature >= 35 &&
        sensor.temperature <= 40
      ) {

        samplingInterval =
          1000;

        powerState =
          "BALANCED";
      }

      // High Performance Mode
      else {

        samplingInterval =
          500;

        powerState =
          "HIGH PERFORMANCE";
      }
    }

    vTaskDelay(
      pdMS_TO_TICKS(500)
    );
  }
}

// Communication Task
void CommTask(
  void *pvParameters
) {

  SensorData sensor;

  while (1) {

    if (
      xQueuePeek(
        sensorQueue,
        &sensor,
        portMAX_DELAY
      ) == pdTRUE
    ) {

      xSemaphoreTake(
        serialMutex,
        portMAX_DELAY
      );

      Serial.println();

      Serial.println(
        "===== SENSOR DATA ====="
      );

      Serial.print(
        "Temperature : "
      );

      Serial.print(
        sensor.temperature
      );

      Serial.println(
        " C"
      );

      Serial.print(
        "Humidity    : "
      );

      Serial.print(
        sensor.humidity
      );

      Serial.println(
        " %"
      );

      Serial.print(
        "Mode         : "
      );

      Serial.println(
        currentMode
      );

      Serial.print(
        "Sampling     : "
      );

      Serial.print(
        samplingInterval
      );

      Serial.println(
        " ms"
      );

      Serial.print(
        "Power State  : "
      );

      Serial.println(
        powerState
      );

      Serial.println(
        "======================="
      );

      xSemaphoreGive(
        serialMutex
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(1000)
    );
  }
}

// Emergency Task
void EmergencyTask(
  void *pvParameters
) {

  while (1) {

    if (
      xSemaphoreTake(
        emergencySemaphore,
        portMAX_DELAY
      ) == pdTRUE
    ) {

      xSemaphoreTake(
        serialMutex,
        portMAX_DELAY
      );

      Serial.println();
      Serial.println(
        "===== EMERGENCY ====="
      );
      Serial.println(
        "Button Pressed"
      );

      xSemaphoreGive(
        serialMutex
      );

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("EMERGENCY!");

      lcd.setCursor(0, 1);
      lcd.print("Hold Button");

      // selama tombol ditekan
      while (
        digitalRead(
          BUTTON_PIN
        ) == LOW
      ) {

        digitalWrite(
          LED_PIN,
          HIGH
        );

        tone(
          BUZZER_PIN,
          2000
        );

        vTaskDelay(
          pdMS_TO_TICKS(50)
        );
      }

      // tombol dilepas
      digitalWrite(
        LED_PIN,
        LOW
      );

      noTone(
        BUZZER_PIN
      );

      lcd.clear();
    }
  }
}

// LCD Task
void LCDTask(
  void *pvParameters
) {

  SensorData sensor;

  while (1) {

    if (
      xQueuePeek(
        sensorQueue,
        &sensor,
        portMAX_DELAY
      ) == pdTRUE
    ) {

      lcd.setCursor(0, 0);

      lcd.print("T:");
      lcd.print(
        sensor.temperature,
        1
      );

      lcd.print(
        (char)223
      );

      lcd.print("C ");

      lcd.setCursor(
        9,
        0
      );

      lcd.print("H:");
      lcd.print(
        sensor.humidity,
        0
      );

      lcd.print("% ");

      lcd.setCursor(
        0,
        1
      );

      lcd.print(
        "Mode:"
      );

      lcd.print(
        currentMode
      );

      lcd.print("   ");
    }

    vTaskDelay(
      pdMS_TO_TICKS(500)
    );
  }
}

// Setup
void setup() {

  Serial.begin(115200);

  dht.begin();

  lcd.init();
  lcd.backlight();

  pinMode(
    LED_PIN,
    OUTPUT
  );

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  pinMode(
    BUTTON_PIN,
    INPUT_PULLUP
  );

  sensorQueue =
    xQueueCreate(
      1,
      sizeof(SensorData)
    );

  serialMutex =
    xSemaphoreCreateMutex();

  emergencySemaphore =
    xSemaphoreCreateBinary();

  attachInterrupt(
    digitalPinToInterrupt(
      BUTTON_PIN
    ),
    buttonISR,
    FALLING
  );

  xTaskCreate(
    SensorTask,
    "SensorTask",
    4096,
    NULL,
    3,
    NULL
  );

  xTaskCreate(
    AlarmTask,
    "AlarmTask",
    2048,
    NULL,
    4,
    NULL
  );

  xTaskCreate(
    PowerMgmtTask,
    "PowerTask",
    2048,
    NULL,
    2,
    NULL
  );

  xTaskCreate(
    CommTask,
    "CommTask",
    4096,
    NULL,
    1,
    NULL
  );

  xTaskCreate(
    EmergencyTask,
    "EmergencyTask",
    2048,
    NULL,
    5,
    NULL
  );

  xTaskCreate(
    LCDTask,
    "LCDTask",
    2048,
    NULL,
    1,
    NULL
  );

  Serial.println(
    "SYSTEM READY"
  );
}

// Main Loop
void loop() {
}
