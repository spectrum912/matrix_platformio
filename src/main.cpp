
#include <FastLED.h>

#define LED_PIN     21
#define NUM_MATRICES 4
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 32
#define NUM_LEDS_PER_MATRIX (MATRIX_WIDTH * MATRIX_HEIGHT)
#define TOTAL_LEDS (NUM_MATRICES * NUM_LEDS_PER_MATRIX)

#define LOCK_PIN    14
const int buttonPins[NUM_MATRICES] = {4, 12, 15, 5};

#define MAX_BUBBLES 10

CRGB leds[TOTAL_LEDS];

bool isPressed[NUM_MATRICES];
unsigned long pressStartTime[NUM_MATRICES];
bool matrixComplete[NUM_MATRICES];
unsigned long levelRiseStartTime[NUM_MATRICES] = {0}; // для каждой матрицы своё время старта роста яркости
int oldLevel[NUM_MATRICES] = {0};
int currentMatrix = 0;

struct Bubble {
    int x;
    float y;
    float speed;
    bool active;
    CHSV color;
};

Bubble bubbles[NUM_MATRICES][MAX_BUBBLES];
const uint8_t hand16x32[] = {
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000111, 0b11110000,
        0b00001111, 0b11111000,
        0b00011111, 0b11111100,
        0b00111111, 0b11111110,
        0b01111111, 0b11111110,
        0b11111111, 0b11111110,
        0b11001111, 0b11111110,
        0b10001111, 0b11111110,
        0b00001111, 0b11111110,
        0b00001101, 0b10110110,
        0b00001101, 0b10110110,
        0b00001101, 0b10110110,
        0b00001101, 0b10110110,
        0b00001101, 0b10110110,
        0b00001101, 0b10110110,
        0b00001101, 0b10110000,
        0b00001101, 0b10110000,
        0b00000001, 0b10000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000,
        0b00000000, 0b00000000
};


void fillMatrix(int matrixIndex, int filledRows);
void showHandSilhouette(int matrixIndex);
void showRedX(int matrixIndex);
void blinkSuccess(int matrixIndex);
int xyToIndex(int x, int y);
void resetAllProgress();


void setup() {
    Serial.begin(115200);
    Serial.println(1);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, TOTAL_LEDS);
    FastLED.setBrightness(64);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);
    FastLED.clear(true);

    for (int i = 0; i < NUM_MATRICES; i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        isPressed[i] = false;
        pressStartTime[i] = 0;
        matrixComplete[i] = false;
        for (int j = 0; j < MAX_BUBBLES; j++) bubbles[i][j].active = false;
    }

    pinMode(LOCK_PIN, OUTPUT);
    digitalWrite(LOCK_PIN, LOW);
}

void loop() {

    for (int i = 0; i < NUM_MATRICES; i++) {
        bool pressed = digitalRead(buttonPins[i]) == LOW;

        if (!pressed && !isPressed[i] && !matrixComplete[i] && pressStartTime[i] == 0) {
            showHandSilhouette(i);
        }
    }

    for (int i = 0; i < NUM_MATRICES; i++) {
        bool pressed = digitalRead(buttonPins[i]) == LOW;

        if (pressed) {
            if (!isPressed[i]) {
                isPressed[i] = true;
                pressStartTime[i] = millis();

                if (i != currentMatrix || matrixComplete[i]) {
                    showRedX(i);
                    resetAllProgress();
                    return;
                }
            }

            if (!matrixComplete[i]) {
                float progress = float(millis() - pressStartTime[i]) / 32000.0;
                int rowsToFill = constrain(int(progress * MATRIX_HEIGHT), 0, MATRIX_HEIGHT);

                fillMatrix(i, rowsToFill);

                if (rowsToFill >= MATRIX_HEIGHT) {
                    matrixComplete[i] = true;
                    blinkSuccess(i);
                    currentMatrix++;

                    if (currentMatrix >= NUM_MATRICES) {
                        Serial.println("✅ Все матрицы заполнены. Открываем замок.");
                        digitalWrite(LOCK_PIN, HIGH);
                    }
                }
            }
        } else {
            if (isPressed[i] && !matrixComplete[i]) {
                showRedX(i);
                resetAllProgress();
                return;
            }
            isPressed[i] = false;
        }

        if (matrixComplete[i]) {
            fillMatrix(i, MATRIX_HEIGHT);
        }
    }
    FastLED.show();

}

void fillMatrix(int matrixIndex, int filledRows) {
    int startIndex = matrixIndex * NUM_LEDS_PER_MATRIX;
    float time = millis() / 300.0;  // волна

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            int ledIndex = startIndex + xyToIndex(x, y);
            float wave = 1.5f * sinf(x * 0.5 + time);
            float liquidLevel = filledRows - wave;

            if (y < floor(liquidLevel)) {
                // Полностью заполненные ряды
                uint8_t noise = inoise8(x * 50, y * 50, millis() / 4);
                uint8_t hue = map(noise, 0, 255, 0, 120);
                leds[ledIndex] = CHSV(hue, 255, 255);
            }
            else if (y == floor(liquidLevel)) {
                // Строка, где начинается наростание
                if (oldLevel[matrixIndex] != filledRows){
                    oldLevel[matrixIndex] = filledRows;
                    levelRiseStartTime[matrixIndex] = millis();
                }

//                if (levelRiseStartTime[matrixIndex] == 0) {
//                    // зафиксировать время старта наростания
//                    levelRiseStartTime[matrixIndex] = millis();
//                }

                unsigned long elapsed = millis() - levelRiseStartTime[matrixIndex];
                uint8_t brightness = constrain(map(elapsed, 0, 1000, 0, 255), 0, 255);

                uint8_t noise = inoise8(x * 50, y * 50, millis() / 4);
                uint8_t hue = map(noise, 0, 255, 0, 120);
                leds[ledIndex] = CHSV(hue, 255, brightness);
            }
            else {
                // Ещё не заполнившиеся строки
                leds[ledIndex] = CRGB::Black;
            }
        }
    }
}






void showHandSilhouette(int matrixIndex) {
    int start = matrixIndex * NUM_LEDS_PER_MATRIX;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        uint16_t rowBits = (hand16x32[y * 2] << 8) | hand16x32[y * 2 + 1];
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            if (rowBits & (1 << (15 - x))) {
                leds[start + xyToIndex(x, y)] = CRGB::Blue;
            } else {
                leds[start + xyToIndex(x, y)] = CRGB::Black;
            }
        }
    }
//    FastLED.show();
}

void showRedX(int matrixIndex) {
    int start = matrixIndex * NUM_LEDS_PER_MATRIX;
    FastLED.clear();
    for (int i = 0; i < MATRIX_HEIGHT; i++) {
        for (int j = 0; j < MATRIX_WIDTH; j++) {
            if (i == j || j == (MATRIX_WIDTH - 1 - i)) {
                leds[start + xyToIndex(j, i)] = CRGB::Red;
            }
        }
    }
    FastLED.show();
    delay(2000);
    FastLED.clear();
    FastLED.show();
}

void blinkSuccess(int matrixIndex) {
    int start = matrixIndex * NUM_LEDS_PER_MATRIX;
    for (int b = 0; b < 3; b++) {
        for (int i = 0; i < MATRIX_HEIGHT; i++) {
            for (int j = 0; j < MATRIX_WIDTH; j++) {
                leds[start + xyToIndex(j, i)] = CRGB::Green;
            }
        }
        FastLED.show();
        delay(200);
        FastLED.clear();
        FastLED.show();
        delay(200);
    }
}

int xyToIndex(int x, int y) {
    int matrixOffset = (y / 32) * NUM_LEDS_PER_MATRIX;
    int localY = y % 32;
    int indexInMatrix = (localY % 2 == 0)
                        ? localY * 16 + x
                        : localY * 16 + (15 - x);
    return matrixOffset + indexInMatrix;
}

void resetAllProgress() {
    Serial.println(" reset  Progress.");
    FastLED.clear();
    FastLED.show();

    for (int i = 0; i < NUM_MATRICES; i++) {
        pressStartTime[i] = 0;
        matrixComplete[i] = false;
        isPressed[i] = false;
        for (int j = 0; j < MAX_BUBBLES; j++) bubbles[i][j].active = false;
    }

    currentMatrix = 0;
    digitalWrite(LOCK_PIN, LOW);
}
