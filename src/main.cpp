#include <Arduino.h>
#include <FastLED.h>

#define LED_PIN     4  // Подключи WS2812 сюда (GPIO4)
#define NUM_MATRICES 4
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 32
#define NUM_LEDS_PER_MATRIX (MATRIX_WIDTH * MATRIX_HEIGHT)
#define TOTAL_LEDS (NUM_MATRICES * NUM_LEDS_PER_MATRIX)

#define LOCK_PIN    14  // Управление замком (GPIO14)

const int buttonPins[NUM_MATRICES] = {5, 0, 2, 12};  // Кнопки (GPIO5,12,13,15)

CRGB leds[TOTAL_LEDS];

bool isPressed[NUM_MATRICES];
unsigned long pressStartTime[NUM_MATRICES];
bool matrixComplete[NUM_MATRICES];
int currentMatrix = 0;

int xyToIndex(int x, int y);
void fillMatrix(int matrixIndex, int filledRows);
void resetAllProgress();


void setup() {
  Serial.begin(115200);

  // FastLED конфигурация
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, TOTAL_LEDS);
  FastLED.setBrightness(64);  // Умеренная яркость
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);  // Защита от перегруза
  FastLED.clear(true);

  // Пины кнопок
  for (int i = 0; i < NUM_MATRICES; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    isPressed[i] = false;
    pressStartTime[i] = 0;
    matrixComplete[i] = false;
  }

  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, LOW);  // Замок выключен
}

void loop() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_MATRICES; i++) {
    bool pressed = digitalRead(buttonPins[i]) == LOW;

    if (pressed) {
      if (!isPressed[i]) {
        isPressed[i] = true;
        pressStartTime[i] = now;

        if (i != currentMatrix || matrixComplete[i]) {
          Serial.print(i);
          Serial.print(currentMatrix);
          resetAllProgress();
          return;
        }
      }

      if (!matrixComplete[i]) {
        float progress = float(now - pressStartTime[i]) / 30000.0;
        int rowsToFill = constrain(int(progress * MATRIX_HEIGHT), 0, MATRIX_HEIGHT);

        fillMatrix(i, rowsToFill);
        FastLED.show();  // Отображаем
        yield();  // WDT reset prevention

        if (rowsToFill >= MATRIX_HEIGHT) {
          matrixComplete[i] = true;
          currentMatrix++;

          if (currentMatrix >= NUM_MATRICES) {
            Serial.println("✅ Все матрицы заполнены. Открываем замок.");
            digitalWrite(LOCK_PIN, HIGH);
          }
        }
      }

    } else {
      if (isPressed[i] && !matrixComplete[i]) {
        resetAllProgress();
        return;
      }
      isPressed[i] = false;
    }
  }

  delay(0);  // важно для ESP8266
}

// Заполнение конкретной матрицы до указанного ряда
void fillMatrix(int matrixIndex, int filledRows) {
  int startIndex = matrixIndex * NUM_LEDS_PER_MATRIX;

  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      int ledIndex = startIndex + xyToIndex(x, y);
      if (y < filledRows) {
        leds[ledIndex] = CRGB::Green;
      } else {
        leds[ledIndex] = CRGB::Black;
      }
    }
    yield();  // не даём зависнуть
  }
}

// Преобразование координат в линейный индекс (змейка)
int xyToIndex(int x, int y) {
  if (x % 2 == 0) {
    return x * MATRIX_HEIGHT + y;
  } else {
    return x * MATRIX_HEIGHT + (MATRIX_HEIGHT - 1 - y);
  }
}

// Сброс всех состояний
void resetAllProgress() {
  Serial.println(" Прогресс сброшен.");

  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_MATRICES; i++) {
    pressStartTime[i] = 0;
    matrixComplete[i] = false;
    isPressed[i] = false;
  }

  currentMatrix = 0;
  digitalWrite(LOCK_PIN, LOW);
}
