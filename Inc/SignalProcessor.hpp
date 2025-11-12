#ifndef SIGNAL_PROCESSOR_HPP_
#define SIGNAL_PROCESSOR_HPP_

#include <stdint.h>
#include <math.h>

/**
 * @brief Універсальний процесор сигналів для embedded систем
 * 
 * Призначення:
 *  - Обробка сигналів сенсорів (акселерометр, гіроскоп, температура, тощо)
 *  - Моніторинг аналогових сигналів (напруга, струм, потужність)
 *  - Обробка даних з АЦП
 *  - Контроль якості сигналів
 *  - Real-time фільтрація та аналіз
 * 
 * Можливості:
 *  - Базова статистика: Mean, Min, Max, StdDev, Variance, Range
 *  - Фільтри: EMA, SMA, Low-pass
 *  - Похідна (raw і згладжена)
 *  - Інтегратор (трапецоїдальний метод)
 *  - Виявлення викидів (outlier detection)
 *  - Контроль стабільності сигналу
 * 
 * Шаблонні параметри:
 *   T — тип даних (float, double, int16_t, int32_t, uint16_t)
 *   N — розмір циклічного буфера (від 2 до 65535)
 * 
 * Використання пам'яті: N * sizeof(T) + ~50 байт
 * 
 * @author Korzhak
 * @version 1.0
 * @date 2025
 */
template<typename T, uint16_t N>
class SignalProcessor {
    static_assert(N >= 2, "Buffer size must be at least 2");
    
private:
    // Циклічний буфер даних
    T buffer_[N];
    uint16_t count_;        // Поточна кількість елементів (0 до N)
    uint16_t index_;        // Індекс для наступного запису (0 до N-1)

    // Базова статистика (онлайн обчислення для ефективності)
    float sum_;             // Сума всіх значень
    float sumSq_;           // Сума квадратів
    T minVal_;              // Мінімальне значення
    T maxVal_;              // Максимальне значення
    bool needRecalcMinMax_; // Прапорець для ліниво перерахунку min/max

    // Фільтри
    float ema_;             // Exponential Moving Average

    // Похідна та інтегратор (для даних з часовими мітками)
    T lastValue_;           // Попереднє значення
    uint32_t lastTimeMs_;   // Попередня часова мітка (мс)
    uint16_t derivativePeriodMs_; // Період похідної в мс
    bool useEmaFilteredValueForDerivation_; // Чи використовуємо фільтроване значення для розрахунку похідної
    float derivative_;      // Похідна (raw)
    float derivativeFiltered_; // Згладжена похідна
    float integrator_;      // Накопичений інтеграл
    float lastIntegrandValue_; // Для трапецоїдального методу

    // Параметри фільтрів (можна налаштовувати)
    float alphaEma_;        // Коефіцієнт EMA (0.0 - 1.0)
    float alphaDerivFilter_;// Коефіцієнт згладжування похідної
    float alphaLowpass_;    // Коефіцієнт low-pass фільтра

    /**
     * Перерахунок min/max по всьому буферу
     * Викликається лінива (lazy), тільки коли потрібно і встановлений прапорець
     */
    void recalculateMinMax() {
        if (count_ == 0) {
            minVal_ = maxVal_ = 0;
            needRecalcMinMax_ = false;
            return;
        }
        
        minVal_ = buffer_[0];
        maxVal_ = buffer_[0];
        
        for (uint16_t i = 1; i < count_; i++) {
            if (buffer_[i] < minVal_) minVal_ = buffer_[i];
            if (buffer_[i] > maxVal_) maxVal_ = buffer_[i];
        }
        
        needRecalcMinMax_ = false;
    }

public:
    /**
     * Конструктор з типовими параметрами фільтрів
     */
    SignalProcessor()
        : count_(0), index_(0),
          sum_(0.0f), sumSq_(0.0f),
          minVal_(0), maxVal_(0), needRecalcMinMax_(false),
          ema_(0.0f),
          lastValue_(0), lastTimeMs_(0),
          derivative_(0.0f), derivativeFiltered_(0.0f),
          integrator_(0.0f), lastIntegrandValue_(0.0f),
          alphaEma_(0.1f), alphaDerivFilter_(0.2f), alphaLowpass_(0.1f)
    {}

    // ========================================
    // НАЛАШТУВАННЯ ПАРАМЕТРІВ
    // ========================================

    /**
     * Встановлення коефіцієнта EMA
     * @param alpha Коефіцієнт (0.0 - 1.0). Більше значення = швидша реакція
     */
    void setEmaAlpha(float alpha) { 
        alphaEma_ = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha;
    }

    void setDerivativePeriodMs(uint16_t period)
    {
    	derivativePeriodMs_ = period;
    }

    void setIsEmaUseForDerivative(bool isEmaUse)
    {
    	useEmaFilteredValueForDerivation_ = isEmaUse;
    }

    /**
     * Встановлення коефіцієнта згладжування похідної
     * @param alpha Коефіцієнт (0.0 - 1.0)
     */
    void setDerivativeFilterAlpha(float alpha) { 
        alphaDerivFilter_ = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha;
    }

    /**
     * Встановлення коефіцієнта low-pass фільтра
     * @param alpha Коефіцієнт (0.0 - 1.0). Менше значення = сильніше згладжування
     */
    void setLowpassAlpha(float alpha) { 
        alphaLowpass_ = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha;
    }

    // ========================================
    // ДОДАВАННЯ ДАНИХ
    // ========================================

    /**
     * Додавання нового значення до буфера
     * @param value Нове значення
     * @param timeMs Часова мітка в мілісекундах (опціонально, для похідної/інтегралу)
     */
    void add(T value, uint32_t timeMs = 0) {
        // Якщо буфер повний - видаляємо найстаріше значення зі статистики
        if (count_ == N) {
            T oldValue = buffer_[index_];
            sum_ -= (float)oldValue;
            sumSq_ -= (float)oldValue * (float)oldValue;
            
            // Якщо видаляємо min або max - позначаємо, що потрібен перерахунок
            if (oldValue == minVal_ || oldValue == maxVal_) {
                needRecalcMinMax_ = true;
            }
        } else {
            count_++;
        }

        // Зберігаємо нове значення в буфер
        buffer_[index_] = value;
        sum_ += (float)value;
        sumSq_ += (float)value * (float)value;

        // Оновлення min/max (швидке, якщо не потрібен повний перерахунок)
        if (count_ == 1) {
            minVal_ = maxVal_ = value;
            needRecalcMinMax_ = false;
        } else if (!needRecalcMinMax_) {
            if (value < minVal_) minVal_ = value;
            if (value > maxVal_) maxVal_ = value;
        }

        // Оновлення EMA фільтра
        if (count_ == 1) {
            ema_ = (float)value;
        } else {
            ema_ = alphaEma_ * (float)value + (1.0f - alphaEma_) * ema_;
        }

        // Похідна та інтегратор (якщо передані часові мітки)
        if (timeMs > 0 && (timeMs - lastTimeMs_ > derivativePeriodMs_)) {
            float dt = (float)(timeMs - lastTimeMs_) * 0.001f;  // секунди
            
            int val = useEmaFilteredValueForDerivation_ ? ema_ : value;
            // Сира похідна
            float rawDerivative = (float) ((int)val - lastValue_) / dt;
            derivative_ = rawDerivative;
            
            // Згладжена похідна (EMA)
            if (count_ <= 2) {
                derivativeFiltered_ = rawDerivative;
            } else {
                derivativeFiltered_ = alphaDerivFilter_ * rawDerivative + (1.0f - alphaDerivFilter_) * derivativeFiltered_;
            }
            
            // Інтегратор (трапецоїдальний метод для точності)
            if (count_ > 1) {
                integrator_ += 0.5f * (lastIntegrandValue_ + (float)value) * dt;
            }
            lastIntegrandValue_ = (float)value;

            if (timeMs > 0) lastTimeMs_ = timeMs;
            // Оновлення стану
            lastValue_ = val;
        }


        

        // Циклічне переміщення індексу
        index_++;
        if (index_ >= N) index_ = 0;
    }

    /**
     * Оператор += для зручного додавання
     */
    SignalProcessor& operator+=(T value) {
        add(value, 0);
        return *this;
    }

    /**
     * Повне скидання всіх даних та статистики
     */
    void reset() {
        count_ = 0;
        index_ = 0;
        sum_ = 0.0f;
        sumSq_ = 0.0f;
        minVal_ = maxVal_ = 0;
        needRecalcMinMax_ = false;
        ema_ = 0.0f;
        lastValue_ = 0;
        lastTimeMs_ = 0;
        derivative_ = 0.0f;
        derivativeFiltered_ = 0.0f;
        integrator_ = 0.0f;
        lastIntegrandValue_ = 0.0f;
    }

    // ========================================
    // БАЗОВА СТАТИСТИКА
    // ========================================

    /** Кількість значень у буфері (0 до N) */
    uint16_t getCount() const { return count_; }

    /** Сума всіх значень */
    float getSum() const { return sum_; }

    /** Середнє арифметичне */
    float getMean() const { 
        return (count_ > 0) ? (sum_ / (float)count_) : 0.0f; 
    }

    /** Sample variance (незміщена оцінка дисперсії) */
    float getVariance() const {
        if (count_ <= 1) return 0.0f;
        float mean = getMean();
        return (sumSq_ - (float)count_ * mean * mean) / (float)(count_ - 1);
    }

    /** Стандартне відхилення (корінь з дисперсії) */
    float getStdDev() const { 
        return sqrtf(getVariance()); 
    }

    /** Коефіцієнт варіації (CV) - відносна мінливість у відсотках */
    float getCoefficientOfVariation() const {
        float mean = getMean();
        if (mean == 0.0f) return 0.0f;
        return (getStdDev() / mean) * 100.0f;
    }

    /** Мінімальне значення у буфері */
    T getMin() {
        if (needRecalcMinMax_) recalculateMinMax();
        return minVal_;
    }

    /** Максимальне значення у буфері */
    T getMax() {
        if (needRecalcMinMax_) recalculateMinMax();
        return maxVal_;
    }

    /** Розмах (різниця між max і min) */
    float getRange() {
        if (needRecalcMinMax_) recalculateMinMax();
        return (float)(maxVal_ - minVal_);
    }

    // ========================================
    // ФІЛЬТРИ
    // ========================================

    /** Exponential Moving Average */
    float getEma() const { return ema_; }

    /** Simple Moving Average (те саме що getMean) */
    float getSma() const { return getMean(); }

    // ========================================
    // ПОХІДНА ТА ІНТЕГРАТОР
    // ========================================

    /** Сира похідна dv/dt */
    float getDerivative() const { return derivative_; }

    /** Згладжена похідна */
    float getDerivativeFiltered() const { return derivativeFiltered_; }

    /** Накопичений інтеграл */
    float getIntegral() const { return integrator_; }

    /** Скидання тільки інтегратора (без інших даних) */
    void resetIntegral() {
        integrator_ = 0.0f;
        lastIntegrandValue_ = 0.0f;
    }

    // ========================================
    // АНАЛІЗ СИГНАЛУ
    // ========================================

    /**
     * Перевірка значення на викид (outlier) за 3-sigma правилом
     * @param value Значення для перевірки
     * @param sigmaThreshold Поріг (за замовчуванням 3.0)
     * @return true якщо значення є викидом
     */
    bool isOutlier(T value, float sigmaThreshold = 3.0f) const {
        if (count_ < 2) return false;
        
        float mean = getMean();
        float stdDev = getStdDev();
        
        if (stdDev == 0.0f) return false;
        
        float deviation = (float)value - mean;
        if (deviation < 0.0f) deviation = -deviation;
        
        return deviation > sigmaThreshold * stdDev;
    }

    /**
     * Перевірка стабільності сигналу
     * @param maxStdDev Максимальне допустиме стандартне відхилення
     * @return true якщо сигнал стабільний
     */
    bool isStable(float maxStdDev) const {
        return (count_ >= N / 2) && (getStdDev() < maxStdDev);
    }

    /**
     * Перевірка чи буфер заповнений
     */
    bool isFull() const {
        return count_ == N;
    }

    /**
     * Перевірка чи буфер порожній
     */
    bool isEmpty() const {
        return count_ == 0;
    }

    // ========================================
    // ДОСТУП ДО ДАНИХ
    // ========================================

    /**
     * Прямий доступ до циклічного буфера
     * УВАГА: порядок елементів може бути не послідовний!
     */
    const T* getBuffer() const { 
        return buffer_; 
    }

    /**
     * Отримання буферу розміру
     */
    uint16_t getBufferSize() const {
        return N;
    }

    /**
     * Отримання останнього доданого значення
     */
    T getLastValue() const {
        return lastValue_;
    }

    /**
     * Отримання останньої часової мітки
     */
    uint32_t getLastTime() const {
        return lastTimeMs_;
    }
};

#endif
