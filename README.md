# SignalProcessor - Універсальна бібліотека обробки сигналів для STM32

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-STM32-blue.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32-32-bit-arm-cortex-mcus.html)
[![Language](https://img.shields.io/badge/language-C%2B%2B-orange.svg)](https://isocpp.org/)

Легка та ефективна бібліотека для real-time обробки сигналів на мікроконтролерах STM32. Оптимізована для embedded систем з обмеженими ресурсами.

## Зміст

- [Можливості](#можливості)
- [Встановлення](#встановлення)
- [Швидкий старт](#швидкий-старт)
- [Детальна документація](#детальна-документація)
- [Приклади використання](#приклади-використання)
- [API Reference](#api-reference)
- [Використання пам'яті](#використання-памяті)
- [Продуктивність](#продуктивність)
- [FAQ](#faq)
  
---

## Можливості

### Базова статистика
- Середнє арифметичне (Mean)
- Стандартне відхилення (StdDev)
- Дисперсія (Variance)
- Мінімум та Максимум
- Розмах (Range)
- Коефіцієнт варіації (CV)

### Фільтри
- Exponential Moving Average (EMA)
- Simple Moving Average (SMA)
- Low-pass фільтр першого порядку

### Аналіз сигналів
- Похідна (raw та згладжена)
- Інтегратор (трапецоїдальний метод)
- Виявлення викидів (outlier detection)
- Контроль стабільності сигналу

### Архітектура
- Циклічний буфер (ring buffer) - фіксована пам'ять
- Онлайн-обчислення - O(1) складність
- Шаблонний клас - підтримка різних типів даних
- Без динамічної алокації пам'яті
- Без залежностей від STL

---

## Встановлення

### Варіант 1: Копіювання файлу
Просто скопіюйте `SignalProcessor.hpp` у папку вашого проекту:

```
YourProject/
├── Inc/
│   └── SignalProcessor.hpp
├── Src/
│   └── main.cpp
```

### Варіант 2: Додавання як submodule (Git)
```bash
git submodule add https://github.com/Korzhak/SignalProcessor.git Submodules/SignalProcessor
```

### Підключення
```cpp
#include "SignalProcessor.hpp"
```

---

## Швидкий старт

### Базовий приклад

```cpp
#include "SignalProcessor.hpp"

// Створення процесора сигналів для 100 останніх значень типу float
SignalProcessor<float, 100> sensor;

void loop() {
    // Читання даних з датчика
    float value = readSensor();
    
    // Додавання нового значення
    sensor.add(value);
    
    // Отримання статистики
    float mean = sensor.getMean();
    float stdDev = sensor.getStdDev();
    float min = sensor.getMin();
    float max = sensor.getMax();
    
    // Фільтровані дані
    float smoothed = sensor.getEma();
}
```

### Приклад для ІНС (акселерометр)

```cpp
SignalProcessor<float, 100> accelX;
SignalProcessor<float, 100> accelY;
SignalProcessor<float, 100> accelZ;

void IMU_Init() {
    // Налаштування параметрів фільтрів
    accelX.setEmaAlpha(0.2f);        // Швидша реакція
    accelX.setLowpassAlpha(0.1f);    // Сильне згладжування
}

void IMU_Update(uint32_t currentTime) {
    // Читання акселерометра
    float ax = readAccelX();
    float ay = readAccelY();
    float az = readAccelZ();
    
    // Додавання з часовою міткою (для похідної/інтегралу)
    accelX.add(ax, currentTime);
    accelY.add(ay, currentTime);
    accelZ.add(az, currentTime);
    
    // Перевірка стабільності для ініціалізації
    if (accelX.isStable(0.01f) && 
        accelY.isStable(0.01f) && 
        accelZ.isStable(0.01f)) {
        // Датчик стабільний - можна ініціалізувати орієнтацію
        initOrientation();
    }
    
    // Виявлення ривків (jerk detection)
    float jerkX = accelX.getDerivative();
    if (fabs(jerkX) > 10.0f) {
        // Різка зміна прискорення!
    }
}
```

---

## Детальна документація

### Шаблонні параметри

```cpp
template<typename T, uint16_t N>
class SignalProcessor;
```

- **T** - тип даних (`float`, `double`, `int16_t`, `int32_t`, `uint16_t` тощо)
- **N** - розмір циклічного буфера (мінімум 2, максимум 65535)

### Конструктор

```cpp
SignalProcessor<float, 100> processor;
```

Створює процесор з типовими параметрами:
- EMA alpha: 0.1
- Derivative filter alpha: 0.2
- Lowpass alpha: 0.1

---

## API Reference

### Налаштування параметрів

#### `setEmaAlpha(float alpha)`
Встановлює коефіцієнт Exponential Moving Average.
- **Параметр**: `alpha` (0.0 - 1.0)
- **Більше значення** = швидша реакція на зміни
- **Менше значення** = плавніше згладжування

```cpp
sensor.setEmaAlpha(0.3f);  // Швидка реакція
sensor.setEmaAlpha(0.05f); // Повільна, плавна реакція
```

#### `setDerivativeFilterAlpha(float alpha)`
Встановлює згладжування для похідної.

```cpp
sensor.setDerivativeFilterAlpha(0.2f);
```

#### `setLowpassAlpha(float alpha)`
Встановлює коефіцієнт low-pass фільтра.

```cpp
sensor.setLowpassAlpha(0.1f);
```

---

### Додавання даних

#### `add(T value, uint32_t timeMs = 0)`
Додає нове значення до буфера.

```cpp
sensor.add(10.5f);              // Без часової мітки
sensor.add(10.5f, HAL_GetTick());    // З часовою міткою
```

#### `operator+=(T value)`
Зручний оператор для додавання.

```cpp
sensor += 10.5f;
```

#### `reset()`
Повне скидання всіх даних та статистики.

```cpp
sensor.reset();
```

---

### Базова статистика

#### `getCount()`
Повертає кількість значень у буфері (0 до N).

```cpp
uint16_t count = sensor.getCount();
```

#### `getSum()`
Повертає суму всіх значень.

```cpp
float sum = sensor.getSum();
```

#### `getMean()`
Повертає середнє арифметичне.

```cpp
float mean = sensor.getMean();
```

#### `getVariance()`
Повертає незміщену оцінку дисперсії (sample variance).

```cpp
float variance = sensor.getVariance();
```

#### `getStdDev()`
Повертає стандартне відхилення.

```cpp
float stdDev = sensor.getStdDev();
```

#### `getCoefficientOfVariation()`
Повертає коефіцієнт варіації у відсотках (CV = StdDev/Mean × 100%).

```cpp
float cv = sensor.getCoefficientOfVariation();
// CV < 10% - низька варіабельність
// CV > 30% - висока варіабельність
```

#### `getMin()` / `getMax()`
Повертає мінімальне/максимальне значення.

```cpp
float min = sensor.getMin();
float max = sensor.getMax();
```

#### `getRange()`
Повертає розмах (різницю між max та min).

```cpp
float range = sensor.getRange();
```

---

### Фільтри

#### `getEma()`
Повертає значення Exponential Moving Average.

```cpp
float ema = sensor.getEma();
```

#### `getSma()`
Повертає Simple Moving Average (те саме що `getMean()`).

```cpp
float sma = sensor.getSma();
```

#### `getLowpass()`
Повертає значення low-pass фільтра.

```cpp
float filtered = sensor.getLowpass();
```

---

### Похідна та інтегратор

#### `getDerivative()`
Повертає сиру похідну dv/dt (потрібні часові мітки).

```cpp
float derivative = sensor.getDerivative();
```

#### `getDerivativeFiltered()`
Повертає згладжену похідну.

```cpp
float smoothDerivative = sensor.getDerivativeFiltered();
```

#### `getIntegral()`
Повертає накопичений інтеграл (трапецоїдальний метод).

```cpp
float integral = sensor.getIntegral();
```

#### `resetIntegral()`
Скидає тільки інтегратор (без інших даних).

```cpp
sensor.resetIntegral();
```

---

### Аналіз сигналів

#### `isOutlier(T value, float sigmaThreshold = 3.0f)`
Перевіряє чи значення є викидом за 3-sigma правилом.

```cpp
float newValue = readSensor();
if (sensor.isOutlier(newValue)) {
    // Аномальне значення!
}

// З іншим порогом
if (sensor.isOutlier(newValue, 2.5f)) {
    // Більш чутливе виявлення викидів
}
```

#### `isStable(float maxStdDev)`
Перевіряє стабільність сигналу.

```cpp
if (sensor.isStable(0.01f)) {
    // Сигнал стабільний, std < 0.01
}
```

#### `isFull()`
Перевіряє чи буфер заповнений.

```cpp
if (sensor.isFull()) {
    // Буфер містить N значень
}
```

#### `isEmpty()`
Перевіряє чи буфер порожній.

```cpp
if (sensor.isEmpty()) {
    // Немає даних
}
```

---

### Доступ до даних

#### `getBuffer()`
Повертає вказівник на циклічний буфер.

```cpp
const float* buffer = sensor.getBuffer();
```

**УВАГА**: Буфер циклічний. Елементи можуть бути не послідовними

#### `getBufferSize()`
Повертає розмір буфера (шаблонний параметр N).

```cpp
uint16_t size = sensor.getBufferSize();
```

#### `getLastValue()`
Повертає останнє додане значення.

```cpp
float lastValue = sensor.getLastValue();
```

#### `getLastTime()`
Повертає останню часову мітку.

```cpp
uint32_t lastTime = sensor.getLastTime();
```

---

## Приклади використання

### 1. Обробка даних акселерометра

```cpp
SignalProcessor<float, 100> accelX;
SignalProcessor<float, 100> accelY;
SignalProcessor<float, 100> accelZ;

void setup() {
    // Налаштування фільтрів
    accelX.setEmaAlpha(0.15f);
    accelY.setEmaAlpha(0.15f);
    accelZ.setEmaAlpha(0.15f);
}

void loop() {
    uint32_t time = HAL_GetTick();
    
    // Читання IMU
    float ax = IMU_ReadAccelX();
    float ay = IMU_ReadAccelY();
    float az = IMU_ReadAccelZ();
    
    // Додавання даних
    accelX.add(ax, time);
    accelY.add(ay, time);
    accelZ.add(az, time);
    
    // Фільтровані значення
    float filteredAx = accelX.getLowpass();
    float filteredAy = accelY.getLowpass();
    float filteredAz = accelZ.getLowpass();
    
    // Використання для розрахунків
    updateOrientation(filteredAx, filteredAy, filteredAz);
}
```

### 2. Моніторинг температури з виявленням аномалій

```cpp
SignalProcessor<float, 50> tempSensor;

void monitorTemperature() {
    float temp = readTemperatureSensor();
    
    // Перевірка на викид перед додаванням
    if (tempSensor.getCount() > 10 && tempSensor.isOutlier(temp, 2.5f)) {
        logError("Temperature anomaly detected: %.2f", temp);
        return; // Не додаємо аномальне значення
    }
    
    tempSensor += temp;
    
    // Статистика
    float avgTemp = tempSensor.getMean();
    float tempStdDev = tempSensor.getStdDev();
    float minTemp = tempSensor.getMin();
    float maxTemp = tempSensor.getMax();
    
    // Перевірка перегріву
    if (avgTemp > 75.0f) {
        triggerCooling();
    }
}
```

### 3. Обробка даних з АЦП

```cpp
SignalProcessor<uint16_t, 200> adcVoltage;

void readADC() {
    uint16_t rawADC = HAL_ADC_GetValue(&hadc1);
    adcVoltage += rawADC;
    
    if (adcVoltage.isFull()) {
        // Конвертація в вольти
        float avgADC = adcVoltage.getMean();
        float voltage = (avgADC / 4095.0f) * 3.3f;
        
        // Оцінка шуму
        float noiseLevel = adcVoltage.getStdDev();
        float snr = (avgADC / noiseLevel); // Signal-to-Noise Ratio
        
        // Якщо SNR низький - проблема з давачем
        if (snr < 10.0f) {
            logWarning("Low SNR: %.2f", snr);
        }
    }
}
```

### 4. Інтегрування швидкості в позицію

```cpp
SignalProcessor<float, 150> velocity;

void updatePosition(uint32_t currentTime) {
    float v = getVelocityFromEncoder();
    velocity.add(v, currentTime);
    
    // Позиція = інтеграл швидкості
    float position = velocity.getIntegral();
    
    // Прискорення = похідна швидкості
    float acceleration = velocity.getDerivativeFiltered();
    
    // Скидання інтегратора при проходженні нуля
    if (positionSensorTriggered()) {
        velocity.resetIntegral();
    }
}
```

### 5. Контроль якості давача

```cpp
SignalProcessor<int16_t, 300> sensorQuality;

void checkSensorHealth() {
    int16_t data = readSensor();
    sensorQuality += data;
    
    if (sensorQuality.isFull()) {
        // Коефіцієнт варіації для оцінки стабільності
        float cv = sensorQuality.getCoefficientOfVariation();
        
        if (cv > 20.0f) {
            // Високий CV = нестабільний давач
            logError("Sensor unstable, CV: %.2f%%", cv);
            initiateSensorCalibration();
        } else if (cv < 1.0f) {
            // Дуже низький CV = можливо давач завис
            logWarning("Sensor might be stuck, CV: %.2f%%", cv);
        }
        
        // Перевірка розмаху
        float range = sensorQuality.getRange();
        if (range < 10) {
            logWarning("Sensor range too small: %.2f", range);
        }
    }
}
```

### 6. Автоматична калібрування при старті

```cpp
bool calibrateIMU() {
    SignalProcessor<float, 200> calibBuffer;
    
    printf("Calibrating IMU... Keep device still!\n");
    
    // Збір 200 зразків
    for (int i = 0; i < 200; i++) {
        float accel = readAccelX();
        calibBuffer += accel;
        HAL_Delay(10); // 10мс між зразками
    }
    
    // Перевірка стабільності
    if (!calibBuffer.isStable(0.02f)) {
        printf("Calibration failed - device not stable!\n");
        return false;
    }
    
    // Розрахунок біасу
    float bias = calibBuffer.getMean();
    float noise = calibBuffer.getStdDev();
    
    printf("Calibration complete:\n");
    printf("  Bias: %.4f\n", bias);
    printf("  Noise: %.4f\n", noise);
    
    // Збереження калібровки
    saveCalibration(bias);
    return true;
}
```

---

## Використання пам'яті

Формула: **N × sizeof(T) + ~50 байт**

| Конфігурація | Пам'ять RAM |
|--------------|-------------|
| `SignalProcessor<float, 50>` | ~250 байт |
| `SignalProcessor<float, 100>` | ~450 байт |
| `SignalProcessor<float, 200>` | ~850 байт |
| `SignalProcessor<uint16_t, 100>` | ~250 байт |
| `SignalProcessor<int16_t, 200>` | ~450 байт |
| `SignalProcessor<int32_t, 100>` | ~450 байт |

### Рекомендації

- **STM32F1** (64KB RAM): N ≤ 200
- **STM32F4** (192KB RAM): N ≤ 1000
- **STM32H7** (1MB RAM): N ≤ 5000

---

## Продуктивність

### Часова складність операцій

| Операція | Складність | Примітка |
|----------|------------|----------|
| `add()` | O(1) | Константний час |
| `getMean()` | O(1) | Попередньо обчислено |
| `getStdDev()` | O(1) | Попередньо обчислено |
| `getMin()` / `getMax()` | O(1) або O(N) | O(N) тільки після видалення екстремуму |
| `getEma()` | O(1) | Константний час |
| `reset()` | O(1) | Константний час |

---

## Підбір параметрів

### Розмір буфера (N)

| Застосування | Рекомендований N |
|--------------|------------------|
| Швидке виявлення змін | 20-50 |
| Балансований (типовий) | 50-150 |
| Точна статистика | 150-500 |
| Калібрування | 200-1000 |

### Коефіцієнт EMA (alpha)

| Alpha | Характеристика | Застосування |
|-------|----------------|--------------|
| 0.01 - 0.05 | Дуже повільна реакція | Довгострокові тренди |
| 0.05 - 0.15 | Повільна реакція | Температура, напруга |
| 0.15 - 0.30 | Середня реакція | Акселерометр, гіроскоп |
| 0.30 - 0.50 | Швидка реакція | Виявлення ривків |
| 0.50 - 1.00 | Дуже швидка | Майже без згладжування |

### Low-pass Alpha

| Alpha | Згладжування | Затримка |
|-------|--------------|----------|
| 0.01 | Дуже сильне | Велика |
| 0.05 | Сильне | Середня |
| 0.10 | Середнє | Мала |
| 0.20 | Слабке | Мінімальна |

---

## FAQ


### Q: Як обрати розмір буфера?
**A:** Залежить від частоти семплювання:
- 100 Hz × 1 сек = N=100
- 1000 Hz × 0.1 сек = N=100



## Внесок

Будемо раді вашим покращенням! 

1. Fork репозиторій
2. Створіть гілку (`git checkout -b feature/AmazingFeature`)
3. Commit зміни (`git commit -m 'Add some AmazingFeature'`)
4. Push у гілку (`git push origin feature/AmazingFeature`)
5. Відкрийте Pull Request
