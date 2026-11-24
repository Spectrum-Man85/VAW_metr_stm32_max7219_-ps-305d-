#include <Arduino.h>
#include <IWatchdog.h>
#include "LedControl.h" //https://github.com/wayoda/LedControl/archive/master.zip  
#include "INA226_mod.h"
#include "Calibration.h"
#include "MeasurementFilters.h"
#include "EncoderInput.h"
#include "MaxPortPolicy.h"

ApplicationCalibrationController calibration;
EncoderInput encoderInput;
EncoderAcceleration voltageAcceleration(16);
EncoderAcceleration currentAcceleration(64);
float rawCurrent = 0;
float calibrationVoltage = 0;
int previousVoltageDigits = -1;
int previousCurrentDigits = -1;
bool measurementValid = false;
void syncCalibrationBases();
void beginCurrentCalibration();
bool completeBaseCalibration();
void resetCalibrationEncoder();

#include "Wire.h"

#include "Val_1.h"
//#include "settings_vas.h"     // внешний файл с настройками 

int toneschema = 3; // 1 - без звука, 2 - только старт и сохранение, 3 - все звуки


#include "eep_new.h"
#include "btn.h"
#include "filters.h"
#include "temp_fan.h"
#include "memory_view.h"

//не всегда работает без отладчика в PlatformIO , прошивать лучше в ардуино ИДЕ

#define buildinled PC13 // встроенный светодиод на плате BluePill  

bool f_led = false; // флаг состояния светодиода

int version = 370; // версия прошивки трёхзначное число  например 162 (ver 16.2f)

float vv;    //напряжение
float aa;    //ток
float vv_sh; // падение напряжения на шунте
bool sh = false;     ; // если true умитывать падение на шунте  



//метки времени 
unsigned long lcd_time;  
unsigned long coil_time;  
unsigned long e_mode_time;
unsigned long btn_time; 
unsigned long led_time;  

//выходы=================================================================================

int buz_pin =  PA3; // Пин для бузера

const int sw1_pin =  PB14; // рэле для переключения обмоток трансформатора (Sw1)
const int sw2_pin =  PB13; // рэле для переключения обмоток трансформатора (Sw2)

//энкодер и кнопки 
int Res =  PB9; //
int Key =  PB5; //
int S1 =   PB3; //
int S2 =   PB4; //
//------------------------------------------------------------------------


// Определяем пороговые значения переключения обмоток трансформатора
const int thresholds[] = {8, 16, 24}; // Массив, содержащий точки переключения в вольтах
const int numThresholds = sizeof(thresholds) / sizeof(thresholds[0]); // Вычисление количества точек переключения
float hysteresis = 1.0f; // Гистерезис в вольтах
int step = 0; // Номер ступени

int ddv=0;  //падение напряжения на транзисторе , вольты (вычитается до переключения ступени  )

// Состояния для асинхронного опроса температуры

// Добавляем новые настройки PWM (ШИМ)

// Устанавливаем пороги температуры
float critTempHs = 80.0f;            // Критическая температура радиатора

bool f_ina_ok = false; // Флаг успешной инициализации INA226 

unsigned long ina_read_time = 0;
uint32_t ina_read_interval_ms = 0;
uint8_t ina226_active_avg = ina226_norm_avg;

uint16_t ina226ConvTimeUs(uint8_t conv);
uint16_t ina226AvgCount(uint8_t avg);
uint32_t ina226ReadIntervalMs();
void resetInaReadTimer();
void setInaAveraging(uint8_t avg);
bool inaMeasurementsReady();
void readInaMeasurements();

 // разрядность датчика температуры и время конверсии для асинхронного режима (выбрать один из вариантов, закомментировав остальные)
 // int t_res= 9 ;  int t_c_time = 95 ;     // 9 бит (0.5f°C): \(\sim 93.75f\) мс (макс)
 // int t_res= 11 ; int t_c_time = 380 ;  // 11 бит (0.125f°C): \(\sim 375\) мс (макс)
 // int t_res= 12 ; int t_c_time = 750 ;  // 12 бит (0.0625f°C): \(\sim 750\) мс (макс)


  float kkk=0.0f;
  float kkka=0.0f;

 int val = 0;

 int dd =0; // Временная переменная для хранения текущего значения dd (падение напряжения на транзисторе) при асинхронном опросе

 //указатели на объекты

 LedControl lc = LedControl(PB1,PA7,PB0,1);// DIN(), CLK(), CS()

 INA226 ina(0.01f, 10.0f);  // 0.01f Ом, 1.0f Ампер макс, адрес 0x40 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  

 // фильтры 
 int fff =0;          //признак первого запуска

 int brightness = 4; // Яркость дисплея (0-15 ?)

 bool f_need_save = false; // Флаг необходимости сохранения настроек в EEPROM
 int e_mode = 0;
 bool A_mode = false; // Режим отображения тока (true - отображать ток, false - отображать напряжение)
 int last = 0;       //последнее значение энкодера
 
 float delta =0.0000f;

 int step_v = 0;
 int step_a = 0;

 const int CURRENT_HISTORY_SIZE = 10;
 float currentHistory[CURRENT_HISTORY_SIZE] = {0};
 int currentHistoryIndex = 0;
 int currentHistoryCount = 0;
 float aaNoise = 0.0f; // Разница max-min за последние 10 показаний тока


  long temp;
  unsigned long get_v_last_time = 0;

void print_max(float voltage, float current)
{
  const MaxFourDigitNumber values[] = {maxFourDigitNumber(voltage), maxFourDigitNumber(current)};
  for (int channel = 0; channel < 2; ++channel) {
    const MaxFourDigitNumber number = values[channel];
    int digits = number.digits;
    const int offset = channel * 4;
    for (int position = 0; position < 4; ++position) {
      if (!number.valid) lc.setChar(0, offset + position, '-', false);
      else lc.setDigit(0, offset + position, digits % 10,
                       number.decimalPosition > 0 && position == number.decimalPosition);
      digits /= 10;
    }
  }
}


void readInaMeasurements()
{
  const float busVoltage = ina.getVoltage();
  rawCurrent = ina.getCurrent();
  const float shuntVoltage = ina.getShuntVoltage();
  measurementValid = isfinite(busVoltage) && isfinite(rawCurrent) &&
                     isfinite(shuntVoltage) && busVoltage >= 0 && busVoltage <= 50;
  if (!measurementValid) { f_ina_ok = false; return; }
  calibrationVoltage = busVoltage * calibration.voltageBaseFactor();
  if (calibrationVoltage < 0) calibrationVoltage = 0;
  vv = calibrationVoltage;
  aa = fabsf(rawCurrent) * calibration.currentBaseFactor();
}

void switchCoils(int currentStep) // Переключаем обмотки в зависимости от номера ступени
{
    //String currentState;
    // Определяем состояние пинов в зависимости от текущей ступени
    switch (currentStep) 
    {
      case 0:
        digitalWrite(sw1_pin, LOW);
        digitalWrite(sw2_pin, LOW);
       // currentState = "00";
        break;
      case 1:
        digitalWrite(sw1_pin, HIGH);
        digitalWrite(sw2_pin, LOW);
       // currentState = "10";
        break;
      case 2:
        digitalWrite(sw1_pin, LOW);
        digitalWrite(sw2_pin, HIGH);
       // currentState = "01";
        break;
      case 3:
        digitalWrite(sw1_pin, HIGH);
        digitalWrite(sw2_pin, HIGH);
       // currentState = "11";
        break;
      //default:
      //  break;
    }

    // Сохраняем текущее состояние пинов для отображения
    //previousState = currentState;
  }

void getStep() 
{
    if (step < numThresholds - 1 && vv+ddv >= thresholds[step]) {
      // Проверка на увеличение ступени:
      // 1. Убедиться, что текущая ступень меньше максимальной (numThresholds - 1).
      // 2. Проверить, что текущее напряжение (v) больше или равно пороговому значению для текущей ступени.
      step++; // Увеличиваем номер ступени
      //if (step > 3) {step = 3;}
      switchCoils(step); // Вызываем функцию для переключения обмоток на новую ступень
    } else if (step > 0 && vv +ddv < (thresholds[step - 1] - hysteresis)) {
      // Проверка на уменьшение ступени:
      // 1. Убедиться, что текущая ступень больше 0 (чтобы избежать отрицательных индексов).
      // 2. Проверить, что текущее напряжение меньше порогового значения предыдущей ступени минус гистерезис.
      step--; // Уменьшаем номер ступени
      //if (step < 0) {step = 0;}
      switchCoils(step); // Вызываем функцию для переключения обмоток на новую ступень
    }
     /* Логика работы:
    1. Определение пороговых значений:
      - Мы создаём массив thresholds, который содержит точки переключения в вольтах. В данном случае это значения 8, 16, 24 вольт. Эти значения используются для определения, когда переключать обмотки трансформатора.
      - numThresholds вычисляется как количество элементов в массиве thresholds. Это позволяет нам легко управлять количеством точек переключения, не изменяя основную логику кода.
    2. Обновление ступени:
      - Увеличение ступени:
        Проверяем, что текущая ступень (step) меньше максимальной (это numThresholds - 1, так как индексация начинается с 0).
        Проверяем, что текущее напряжение (v) больше или равно пороговому значению для текущей ступени.
        Если оба условия выполнены, увеличиваем номер ступени на 1 и вызываем функцию switchCoils(step) для переключения обмоток на новую ступень.
      - Уменьшение ступени:
        Проверяем, что текущая ступень больше 0, чтобы избежать отрицательных индексов.
        Проверяем, что текущее напряжение меньше порогового значения предыдущей ступени минус гистерезис. Это условие помогает избежать ложных срабатываний при переключении обмоток.
        Если оба условия выполнены, уменьшаем номер ступени на 1 и вызываем функцию switchCoils(step) для переключения обмоток на новую ступень.
  */
  }

float rounding(float value)             // Функция для округления до 3 знаков после запятой
{
  return roundf(value * 1000.0f) / 1000.0f + 0.000001f;
}

float r_sh = 0.01f;

float get_v_sh(float aa)
{
  return aa * r_sh / kaa;
}

float kor_v(float voltage)
{
  return calibrateVoltage(voltage, calibration.voltageFactors());
}

float kor_a(float current)
{
  return calibrateCurrent(current, calibration.currentFactors());
}

float updateCurrentNoise(float current)
{
  float returnValue=0.0f;

  currentHistory[currentHistoryIndex] = current;
  currentHistoryIndex = (currentHistoryIndex + 1) % CURRENT_HISTORY_SIZE;
  if (currentHistoryCount < CURRENT_HISTORY_SIZE) {
    currentHistoryCount++;
  }

  float minValue = currentHistory[0];
  float maxValue = currentHistory[0];
  for (int i = 1; i < currentHistoryCount; i++) {
    float value = currentHistory[i];
    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;
  }


  returnValue = maxValue - minValue;

 if (returnValue > 0.01f)  
 {
    returnValue = 0.01f; // Минимальное значение, чтобы избежать деления на ноль
  } 


  return returnValue;
}

void print_max_init()
{
  int t=50;

  lc.setDigit(0,7,0,true);
  delay(t);
  lc.setDigit(0,6,0,true);
  delay(t);
  lc.setDigit(0,5,0,true);
  delay(t);
  lc.setDigit(0,4,0,true);
  delay(t);

  lc.setDigit(0,3,0,true);
  delay(t);
  lc.setDigit(0,2,0,true);
  delay(t);
  lc.setDigit(0,1,0,true);
  delay(t);
  lc.setDigit(0,0,0,true);
  delay(t);
}

void print_sh_text()
{
  const byte CHAR_S = B01011011;
  const byte CHAR_H = B00110111;

  lc.clearDisplay(0);
  lc.sendMax(0, 5, CHAR_S);
  lc.sendMax(0, 4, CHAR_H);
}

void print_sh_init()
{
  if (!sh) return;

  for (int i = 0; i < 2; i++) {
    print_sh_text();
    delay(300);
    lc.clearDisplay(0);
    delay(300);
  }
}
void clearLeft4()
{
  lc.setChar(0, 7, ' ', false);
  lc.setChar(0, 6, ' ', false);
  lc.setChar(0, 5, ' ', false);
  lc.setChar(0, 4, ' ', false);
}

void clearRight4()
{
  lc.setChar(0, 3, ' ', false);
  lc.setChar(0, 2, ' ', false);
  lc.setChar(0, 1, ' ', false);
  lc.setChar(0, 0, ' ', false);
}

void displayRightNumber(int value , bool reboot=false)
{
 static int lastValue = -1; // Для хранения последнего отображённого значения
 
 if (value != lastValue || reboot) // Обновляем дисплей только если значение изменилось или требуется перезагрузка
 {
    lastValue = value; // Обновляем последнее отображённое значение 


    // Левые 4 разряда
    const byte D1 = 3; // самый левый
    const byte D2 = 2;
    const byte D3 = 1;
    const byte D4 = 0;


    // ограничим диапазон
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    // сначала гасим все 4 правых разряда
    clearRight4();

    if (value < 10)
    {
      // 0...9 -> только самый левый
      lc.setDigit(0, D4, value, false);
    }
    else if (value < 100)
    {
      // 10...99 -> два левых разряда
      lc.setDigit(0, D3, value / 10, false);
      lc.setDigit(0, D4, value % 10, false);
    }
    else
    {
        // 100 -> три левых разряда
      lc.setDigit(0, D2, 1, false);
      lc.setDigit(0, D3, 0, false);
      lc.setDigit(0, D4, 0, false);
    }
 }


}

void displayRightUInt(int value, bool reboot=false)
{
 static int lastValue = -1;
 if (value != lastValue || reboot)
 {
    lastValue = value;
    const byte D1 = 3;
    const byte D2 = 2;
    const byte D3 = 1;
    const byte D4 = 0;

    if (value < 0) value = 0;
    if (value > 9999) value = 9999;

    clearRight4();

    int d1 = (value / 1000) % 10;
    int d2 = (value / 100) % 10;
    int d3 = (value / 10) % 10;
    int d4 = value % 10;

    if (value >= 1000) {
      lc.setDigit(0, D1, d1, false);
      lc.setDigit(0, D2, d2, false);
      lc.setDigit(0, D3, d3, false);
      lc.setDigit(0, D4, d4, false);
    } else if (value >= 100) {
      lc.setChar(0, D1, ' ', false);
      lc.setDigit(0, D2, d2, false);
      lc.setDigit(0, D3, d3, false);
      lc.setDigit(0, D4, d4, false);
    } else if (value >= 10) {
      lc.setChar(0, D1, ' ', false);
      lc.setChar(0, D2, ' ', false);
      lc.setDigit(0, D3, d3, false);
      lc.setDigit(0, D4, d4, false);
    } else {
      lc.setChar(0, D1, ' ', false);
      lc.setChar(0, D2, ' ', false);
      lc.setChar(0, D3, ' ', false);
      lc.setDigit(0, D4, d4, false);
    }
 }
}


void displayLeftNumber(int value , bool reboot=false)
{
 static int lastValue = -1; // Для хранения последнего отображённого значения
 
 if (value != lastValue || reboot) // Обновляем дисплей только если значение изменилось или требуется перезагрузка
 {
    lastValue = value; // Обновляем последнее отображённое значение 


    // Левые 4 разряда
    const byte D1 = 7; // самый левый
    const byte D2 = 6;
    const byte D3 = 5;
    const byte D4 = 4;


    // ограничим диапазон
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    // сначала гасим все 4 левых разряда
    clearLeft4();

    if (value < 10)
    {
      // 0...9 -> только самый левый
      lc.setDigit(0, D1, value, false);
    }
    else if (value < 100)
    {
      // 10...99 -> два левых разряда
      lc.setDigit(0, D1, value / 10, false);
      lc.setDigit(0, D2, value % 10, false);
    }
    else
    {
        // 100 -> три левых разряда
      lc.setDigit(0, D1, 1, false);
      lc.setDigit(0, D2, 0, false);
      lc.setDigit(0, D3, 0, false);
    }
 }


}

void displayRightFloat(float value, bool reboot=false)
{
  (void)reboot;
  const MaxFourDigitNumber number = maxFourDigitNumber(value);
  int digits = number.digits;
  for (int position = 0; position < 4; ++position) {
    if (!number.valid) lc.setChar(0, 0 + position, '-', false);
    else lc.setDigit(0, 0 + position, digits % 10,
                     number.decimalPosition > 0 && position == number.decimalPosition);
    digits /= 10;
  }
}

void displayLeftFloat(float value, bool reboot=false)
{
  (void)reboot;
  const MaxFourDigitNumber number = maxFourDigitNumber(value);
  int digits = number.digits;
  for (int position = 0; position < 4; ++position) {
    if (!number.valid) lc.setChar(0, 4 + position, '-', false);
    else lc.setDigit(0, 4 + position, digits % 10,
                     number.decimalPosition > 0 && position == number.decimalPosition);
    digits /= 10;
  }
}

int get_v(float voltage)
{
  return static_cast<int>(voltageCalibrationIndex(voltage)) + 1;
}

int get_a(float current)
{
  return static_cast<int>(currentCalibrationIndex(current)) + 1;
}

void t_one(int ms, bool systemEvent = false)    // обёртка для пищалки
{
  if (toneschema == 1 || (toneschema == 2 && !systemEvent)) {
    return;
  }


  if (act_buzer) {
    digitalWrite(buz_pin, HIGH);
    delay(ms);
    digitalWrite(buz_pin, LOW);
  } else {
    tone(buz_pin, 1000, ms);
    delay(ms); // Подождать, чтобы обеспечить паузу между звуками
    noTone(buz_pin); // Остановить генерацию звука после указанной длительности
    delay(ms); // Подождать, чтобы обеспечить паузу между звуками
  }
  


}

void if_t_eeprom()
{
  const auto report = calibration.processWrites(millis());
  if (report.voltageSaved || report.currentSaved) t_one(100, true);
  if (report.voltageFailed || report.currentFailed || report.voltageUnavailable || report.currentUnavailable)
    Serial.println("Calibration save pending: storage unavailable");
  if (f_need_save && millis() - e_mode_time > static_cast<unsigned long>(save_time)) {
    writeEEPROM();
  }
}

void if_t_step_up()
{
  if (e_mode == 1 && !calibration.firstVoltageCalibrationPending() &&
      !calibration.firstCurrentCalibrationPending() && millis() - e_mode_time > 5000) {
    e_mode = 0;
    setInaAveraging(ina226_norm_avg);
    resetInaReadTimer();
    MeasurementFilters::resetAll();
    t_one(100);
  }
}

void if_t_coil()
{
  if (measurementValid && millis()-coil_time>30)    {  getStep();        coil_time = millis();} // обновление состояния реле  по таймеру
}

uint16_t ina226ConvTimeUs(uint8_t conv)
{
  switch (conv & 0b111) {
    case INA226_CONV_140US: return 140;
    case INA226_CONV_204US: return 204;
    case INA226_CONV_332US: return 332;
    case INA226_CONV_588US: return 588;
    case INA226_CONV_1100US: return 1100;
    case INA226_CONV_2116US: return 2116;
    case INA226_CONV_4156US: return 4156;
    case INA226_CONV_8244US: return 8244;
  }

  return 1100;
}

uint16_t ina226AvgCount(uint8_t avg)
{
  switch (avg & 0b111) {
    case INA226_AVG_X1: return 1;
    case INA226_AVG_X4: return 4;
    case INA226_AVG_X16: return 16;
    case INA226_AVG_X64: return 64;
    case INA226_AVG_X128: return 128;
    case INA226_AVG_X256: return 256;
    case INA226_AVG_X512: return 512;
    case INA226_AVG_X1024: return 1024;
  }

  return 1;
}

uint32_t ina226ReadIntervalMs()
{
  const bool slow = e_mode == 1;
  uint32_t conv_us = slow ? 2200U :
      (uint32_t)ina226ConvTimeUs(ina226_v_conv) + ina226ConvTimeUs(ina226_a_conv);
  uint32_t avg_count = ina226AvgCount(ina226_active_avg);

  return (conv_us * avg_count + 999) / 1000;
}

void setInaAveraging(uint8_t avg)
{
  const bool slow = e_mode == 1;
  ina.setSampleTime(INA226_VBUS, slow ? INA226_CONV_1100US : ina226_v_conv);
  ina.setSampleTime(INA226_VSHUNT, slow ? INA226_CONV_1100US : ina226_a_conv);
  ina.setAveraging(avg);
  ina226_active_avg = avg;
}

void resetInaReadTimer()
{
  ina_read_interval_ms = ina226ReadIntervalMs();
  ina_read_time = millis();
}

bool inaMeasurementsReady()
{
  uint32_t interval_ms = ina226ReadIntervalMs();
  unsigned long now = millis();

  if (interval_ms != ina_read_interval_ms) {
    ina_read_interval_ms = interval_ms;
    ina_read_time = now;
    return false;
  }

  if (now - ina_read_time >= interval_ms) {
    ina_read_time = now;
    return true;
  }

  return false;
}



// MAX7219 bits: DP A B C D E F G. Center the two-letter label.
void displayGlobalCalibrationLabel(int highDigit, bool voltage)
{
  lc.setRow(0, highDigit, 0x00);
  lc.setRow(0, highDigit - 1, 0x5E);  // G
  lc.setRow(0, highDigit - 2, voltage ? 0x3E : 0x77);  // U / A
  lc.setRow(0, highDigit - 3, 0x00);
}

void if_t_lcd_m1_v()
{
  if (e_mode == 1)    // обновление дисплея в режиме 1
  {
    if (calibration.firstVoltageCalibrationPending()) {
      displayGlobalCalibrationLabel(7, true);  // GU instead of current
    } else {
      displayLeftNumber(step_v );
    }
    displayRightFloat(vv);
  }
  
}

void if_t_lcd_m1_a()
{
  if (e_mode == 1)    // обновление дисплея в режиме 1
  {
    if (calibration.firstCurrentCalibrationPending()) {
      displayGlobalCalibrationLabel(3, false);  // GA instead of voltage
    } else {
      displayRightNumber(step_a );
    }
    displayLeftFloat(aa);
  }
  
}

void if_t_lcd ()
{
  long gg;
  gg = millis() - lcd_time;
  if (gg > 30)
  {
    lcd_time = millis();

    lc.sendMax(0,0x0F, 0x00); // Display Test: off
    lc.sendMax(0,0x0B, 0x07); // Scan Limit: все 8 цифр/строк
    lc.sendMax(0,0x09, 0x00); // Decode Mode: No decode (для матриц)
    // lc.sendMax(0,0x0A, 0x05); // Intensity: яркость (0-15)
    lc.sendMax(0,0x0C, 0x01); // Shutdown: Normal operation
    lc.setIntensity(0,brightness);


    if (e_mode == 0 )    { print_max(measurementValid ? vv : NAN, measurementValid ? aa : NAN);       } // обновление дисплея по таймеру
    if (e_mode == 1 )    
      {  
        if(A_mode)   {  if_t_lcd_m1_a(); }   // обновляем данные в подменю 
        if(!A_mode)  {  if_t_lcd_m1_v();}
      } 
    if (e_mode == 2 )    { updateMemoryViewDisplay(); } 
  }
}

void if_enkoder()
{
  if (e_mode == 2) { handleMemoryViewEncoder(); return; }
  EncoderEvent event = EncoderEvent::None;
  for (uint8_t count = 0; count < 32 && encoderInput.popEvent(event); ++count) {
    if (e_mode != 1 || !measurementValid) continue;
    const bool increase = event == EncoderEvent::Right;
    e_mode_time = millis();
    calibration.noteEncoderActivity(e_mode_time);
    if (!A_mode) {
      const uint32_t multiplier = maxVoltageStepMultiplier(
          kor_v(calibrationVoltage), voltageAcceleration.onDetent(increase));
      const auto result = calibration.adjustVoltage(calibrationVoltage, increase, multiplier);
      if (result.changed) {
        calibrationVoltage = result.adjustedLoadVoltage;
        vv = maxCompensatedVoltage(result.correctedVoltage,
            kor_a(fabsf(rawCurrent) * calibration.currentBaseFactor()),
            calibration.currentBaseFactor(), r_sh, sh);
        MeasurementFilters::resetVoltage();
      }
    } else {
      const uint32_t multiplier = maxCurrentStepMultiplier(
          calibration.firstCurrentCalibrationPending(), currentAcceleration.onDetent(increase));
      const auto result = calibration.adjustCurrent(rawCurrent, increase, multiplier);
      if (result.changed) { aa = result.correctedCurrent; MeasurementFilters::resetCurrent(); }
    }
    syncCalibrationBases();
  }
}

void run_once()
{
  if (fff != 0) return;
  MeasurementFilters::resetAll();
  lc.shutdown(0, false);
  lc.setIntensity(0, brightness);
  lc.clearDisplay(0);
  fff = 1;
}

void setup() 
{
  
  //выходы=================================================================================
  pinMode(buildinled,   OUTPUT);
  pinMode(buz_pin,  OUTPUT);  digitalWrite(buz_pin, LOW);

  pinMode(sw1_pin,  OUTPUT);
  pinMode(sw2_pin,  OUTPUT);

  //входы=================================================================================
  pinMode(Res, INPUT_PULLUP);
  pinMode(Key, INPUT_PULLUP);
  pinMode(S1, INPUT_PULLUP);
  pinMode(S2, INPUT_PULLUP);
  
  // wdt
    if (IWatchdog.isReset(true)) {
    // Optional: Indicate a watchdog reset happened
  }

 


  Serial.begin(115200);
  encoderInput.configurePins();
  Wire.begin(BoardPins::kI2cSda, BoardPins::kI2cScl);
  Wire.setClock(400000);
  readEEPROM();
  if (calibration.firstVoltageCalibrationPending()) { e_mode = 1; A_mode = false; }
   // Чтение сохранённых настроек из EEPROM при старте
  t_one(500, true);

  f_ina_ok = ina.begin();

  if (f_ina_ok) 
  {
    ina.setSampleTime (INA226_VBUS,    ina226_v_conv);     // Повысим время выборки напряжения 
    ina.setSampleTime (INA226_VSHUNT,  ina226_a_conv);     // Повысим время выборки тока
    setInaAveraging(e_mode == 1 ? ina226_slow_avg : ina226_norm_avg);
    resetInaReadTimer();
   // Wire.setClock(50000);   // 50 кГц
  }

  tempFanInit();

  encoderInput.startInterrupts();

  btnInit();
  attachInterrupt(digitalPinToInterrupt(Key), buttonISR, CHANGE);

 
 // Initialize watchdog with 1 second timeout (1,000,000 microseconds)
  // Max for IWDG is usually around 26 seconds.
  IWatchdog.begin(3000000); 

}

void loop() { 
  //отладка =================================================================================

  IWatchdog.reload(); 
 run_once();

 if (!f_ina_ok) {
   static uint32_t lastReconnect = 0;
   if (millis() - lastReconnect >= 1000) {
     lastReconnect = millis();
     f_ina_ok = ina.begin();
     if (f_ina_ok) {
       setInaAveraging(e_mode == 1 ? ina226_slow_avg : ina226_norm_avg);
       resetInaReadTimer();
       MeasurementFilters::resetAll();
     }
   }
 }

  // проверяем энкодер на изменения  ----------------------------------===================
  encoderInput.poll(millis());
  if (encoderInput.consumeActivity()) calibration.noteEncoderActivity(millis());
  if (e_mode == 2) {
    EncoderEvent event;
    for (uint8_t count = 0; count < 32 && encoderInput.popEvent(event); ++count)
      val += event == EncoderEvent::Right ? 4 : -4;
  }
  btn();
  if_enkoder();

 
  //мигаем встроенным светодиодом для индикации работы
  if (millis() - led_time > 1000)  {    digitalWrite(buildinled, HIGH);  }
  if (millis() - led_time > 2000)  {    digitalWrite(buildinled, LOW);   led_time = millis(); }

  if (f_ina_ok && inaMeasurementsReady() )   // получение и обработка данных тока и напряжения ...
  { 
    readInaMeasurements();
    if (measurementValid) {
      step_v = get_v(calibrationVoltage);
      step_a = get_a(fabsf(rawCurrent) * calibration.currentBaseFactor());
      const float calibratedCurrent = kor_a(fabsf(rawCurrent) * calibration.currentBaseFactor());
      const float calibratedVoltage = maxCompensatedVoltage(
          kor_v(calibrationVoltage), calibratedCurrent, calibration.currentBaseFactor(), r_sh, sh);
      const auto frame = MeasurementFilters::process(
          calibratedVoltage, calibratedCurrent,
          previousVoltageDigits, previousCurrentDigits, e_mode != 1);
      vv = frame.voltage.value;
      aa = frame.current.value;
      if (e_mode != 1 && aa <= 0.002f) aa = 0;
      previousVoltageDigits = voltageDisplayDigits(vv);
      previousCurrentDigits = currentDisplayDigits(aa, false);
    }

  } 

 if_t_lcd ();
 tempFanTick();
 if_t_coil();
 if_t_step_up();
 if_t_eeprom();

  //delay (30);
}

void syncCalibrationBases() {
  kvv = calibration.voltageBaseFactor();
  kaa = calibration.currentBaseFactor();
}

void beginCurrentCalibration() {
  calibration.enterCurrentCalibration();
  voltageAcceleration.reset();
  currentAcceleration.reset();
  MeasurementFilters::resetAll();
}

bool completeBaseCalibration() {
  if (calibration.firstVoltageCalibrationPending()) {
    if (calibration.finishFirstVoltageCalibration() != CalibrationEepromTransactionResult::kSaved) return false;
    // Continue the same calibration session directly from GU to GA.
    e_mode = 1;
    A_mode = true;
    beginCurrentCalibration();
    syncCalibrationBases();
    e_mode_time = millis();
    calibration.noteEncoderActivity(e_mode_time);
    setInaAveraging(ina226_slow_avg);
    resetInaReadTimer();
    clearLeft4();
    clearRight4();
    if_t_lcd_m1_a();
    return true;
  }
  if (calibration.firstCurrentCalibrationPending()) {
    if (!calibration.finishFirstCurrentCalibration()) return false;
    e_mode = 0;
    A_mode = false;
    syncCalibrationBases();
    resetCalibrationEncoder();
    setInaAveraging(ina226_norm_avg);
    resetInaReadTimer();
    print_max(vv, aa);
    return true;
  }
  return false;
}

void resetCalibrationEncoder() { voltageAcceleration.reset(); currentAcceleration.reset(); MeasurementFilters::resetAll(); }
