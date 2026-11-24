#include "filters.h"

#include <Arduino.h>

extern float vv;
extern float aa;
extern float aaNoise;//

//aaNoise
static float fv1 = 0.0f;
static float fv2 = 0.0f;
static float fv3 = 0.0f;
static float fa1 = 0.0f;
static float fa2 = 0.0f;
static float fa3 = 0.0f;

static const uint8_t FILTER_VV_DOWN_CONFIRM_COUNT = 3;
static uint8_t vv_filter_branch = 0;
static uint8_t vv_filter_down_candidate = 0;
static uint8_t vv_filter_down_count = 0;
static float vv_filter_output = 0.0f;

static const uint8_t FILTER_AA_DOWN_CONFIRM_COUNT = 3;
static uint8_t aa_filter_branch = 0;
static uint8_t aa_filter_down_candidate = 0;
static uint8_t aa_filter_down_count = 0;
static float aa_filter_output = 0.0f;

template <typename T, uint16_t N>
class MedianN {
 public:
  T filter(T val) {
    _buf[_i] = val;
    if ((_i < N - 1) && (_buf[_i] > _buf[_i + 1])) {
      for (uint8_t i = _i; i < N - 1; i++) {
        if (_buf[i] > _buf[i + 1]) {
          T t = _buf[i];
          _buf[i] = _buf[i + 1];
          _buf[i + 1] = t;
        }
      }
    } else {
      if ((_i > 0) && (_buf[_i - 1] > _buf[_i])) {
        for (uint8_t i = _i; i > 0; i--) {
          if (_buf[i] < _buf[i - 1]) {
            T t = _buf[i];
            _buf[i] = _buf[i - 1];
            _buf[i - 1] = t;
          }
        }
      }
    }
    if (++_i >= N) _i = 0;
    return _buf[N / 2];
  }

  void init(T val) {
    for (uint8_t i = 0; i < N; i++) _buf[i] = val;
  }

 private:
  T _buf[N];
  uint8_t _i = 0;
};

static MedianN<int, 15> med11;
static MedianN<int, 30> med33;
static MedianN<int, 60> med199;

static MedianN<int, 15> meda11;
static MedianN<int, 30> meda33;
static MedianN<int, 60> meda199;

static float vvBranchValue(uint8_t branch)
{
  switch (branch) {
    case 1: return fv1;
    case 2: return fv2;
    case 3: return fv3;
  }

  return vv;
}

static float aaBranchValue(uint8_t branch)
{
  switch (branch) {
    case 1: return fa1;
    case 2: return fa2;
    case 3: return fa3;
  }

  return aa;
}

void init_filters()
{
  int v = vv * 1000;
  int a = aa * 1000;

  med11.init(v);
  med33.init(v);
  med199.init(v);

  meda11.init(a);
  meda33.init(a);
  meda199.init(a);

  vv_filter_branch = 0;
  vv_filter_down_candidate = 0;
  vv_filter_down_count = 0;
  vv_filter_output = vv;

  aa_filter_branch = 0;
  aa_filter_down_candidate = 0;
  aa_filter_down_count = 0;
  aa_filter_output = aa;
}

void get_filters()
{
  fv1 = (float)(med199.filter(vv * 1000)) / 1000;
  fv2 = (float)(med33.filter(vv * 1000)) / 1000;
  fv3 = (float)(med11.filter(vv * 1000)) / 1000;

  fa1 = (float)(meda199.filter(aa * 1000)) / 1000;
  fa2 = (float)(meda33.filter(aa * 1000)) / 1000;
  fa3 = (float)(meda11.filter(aa * 1000)) / 1000;
}





 void filter_get_vv()
  {
  uint8_t candidate_branch = 0;
  float candidate_value = vv;

  if (fabsf(vv - fv1) < 0.0005f) 
  {
    candidate_branch = 1;
    candidate_value = fv1;
  } else if (fabsf(vv - fv2) < 0.001f) 
  {
    candidate_branch = 2;
    candidate_value = fv2;
  } else if (fabsf(vv - fv3) < 0.0015f) 
  {
    candidate_branch = 3;
    candidate_value = fv3;
  }

  if (candidate_branch == 0) 
  {
    vv_filter_branch = 0;
    vv_filter_down_candidate = 0;
    vv_filter_down_count = 0;
    vv_filter_output = vv;
    return;
  }

  if (vv_filter_branch == 0 || candidate_branch <= vv_filter_branch) 
  {
    vv_filter_branch = candidate_branch;
    vv_filter_down_candidate = 0;
    vv_filter_down_count = 0;
    vv_filter_output = candidate_value;
    vv = vv_filter_output;
    return;
  }

  if (vv_filter_down_candidate == candidate_branch) {
    if (vv_filter_down_count < FILTER_VV_DOWN_CONFIRM_COUNT) 
    {
      vv_filter_down_count++;
    }
  } else {
    vv_filter_down_candidate = candidate_branch;
    vv_filter_down_count = 1;
  }

  if (vv_filter_down_count >= FILTER_VV_DOWN_CONFIRM_COUNT) 
  {
    vv_filter_branch = candidate_branch;
    vv_filter_down_candidate = 0;
    vv_filter_down_count = 0;
    vv_filter_output = candidate_value;
    vv = vv_filter_output;
  } else {
    vv_filter_output = vvBranchValue(vv_filter_branch);
    vv = vv_filter_output;
  }
}

 void filter_get_aa()
  {
  uint8_t candidate_branch = 0;
  float candidate_value = aa;

  if (fabsf(aa - fa1) < 0.5f*aaNoise) 
  {
    candidate_branch = 1;
    candidate_value = fa1;
  } else if (fabsf(aa - fa2) < aaNoise) 
  {
    candidate_branch = 2;
    candidate_value = fa2;
  } else if (fabsf(aa - fa3) < 1.5f*aaNoise) 
  {
    candidate_branch = 3;
    candidate_value = fa3;
  }

  if (candidate_branch == 0) 
  {
    aa_filter_branch = 0;
    aa_filter_down_candidate = 0;
    aa_filter_down_count = 0;
    aa_filter_output = aa;
    return;
  }

  if (aa_filter_branch == 0 || candidate_branch <= aa_filter_branch) 
  {
    aa_filter_branch = candidate_branch;
    aa_filter_down_candidate = 0;
    aa_filter_down_count = 0;
    aa_filter_output = candidate_value;
    aa = aa_filter_output;
    return;
  }

  if (aa_filter_down_candidate == candidate_branch) {
    if (aa_filter_down_count < FILTER_AA_DOWN_CONFIRM_COUNT) 
    {
      aa_filter_down_count++;
    }
  } else {
    aa_filter_down_candidate = candidate_branch;
    aa_filter_down_count = 1;
  }

  if (aa_filter_down_count >= FILTER_AA_DOWN_CONFIRM_COUNT) 
  {
    aa_filter_branch = candidate_branch;
    aa_filter_down_candidate = 0;
    aa_filter_down_count = 0;
    aa_filter_output = candidate_value;
    aa = aa_filter_output;
  } else {
    aa_filter_output = aaBranchValue(aa_filter_branch);
    aa = aa_filter_output;
  }
}





void step_update_display_from_filtered(float filtered_v, float filtered_a)
{

 // Шаговый обновлятор отображаемого значения.
 // Берёт отфильтрованные значения (в вольтах и амперах), переводит в mV/mA
 // и увеличивает/уменьшает текущее отображаемое значение на 1 единицу (1 mV / 1 mA)
 // до тех пор, пока отображаемое не сравняется с отфильтрованным.
 static float disp_v_mV = 0.0f;
 static float disp_a_mA = 0.0f;
 static bool disp_inited = false;



  int target_v = (int)roundf(filtered_v * 1000.0f);
  int target_a = (int)roundf(filtered_a * 1000.0f);

  if (!disp_inited) 
  {
    disp_v_mV = target_v;
    disp_a_mA = target_a;
    disp_inited = true;
  }

  float diff_v = fabsf((target_v - disp_v_mV) / 1000.0f);
  float diff_a = fabsf((target_a - disp_a_mA) / 1000.0f);
  float displayThreshold_a = filtered_a * 0.015f; // Порог для обновления дисплея (1% от текущего значения)

 // if (diff_v < displayThreshold) {
 //   target_v = disp_v_mV;
 // }
  if (diff_a < displayThreshold_a) {
    target_a = disp_a_mA;
  }

  // Вычисляем шаг в зависимости от разницы (в вольтах/амперах)
  int delta_v = target_v - disp_v_mV; // в mV
  int delta_a = target_a - disp_a_mA; // в mA

  // if(delta_a != 0) 
  // {
   // delay(1);
   // }

    // Вычисляем шаг: если разница <0.001f => шаг 0.1f mV; <0.01f => 100 mV; <0.1f => 1000 mV
    auto calc_step = [](float delta_milli) -> float {
      float absdelta = delta_milli >= 0.0f ? delta_milli : -delta_milli;
      if (absdelta == 0.0f) return 0.0f;
      float abs_units = absdelta / 1000.0f; // в V или A
      float step = 1000.0f;
      if (abs_units < 0.001f) step = 0.05f;
      else if (abs_units < 0.01f) step = 10.0f;
      else if (abs_units < 0.1f) step =  50.0f;
      else step = 1000.0f;
      if (step > absdelta) step = absdelta;
      return step;
    };

    float step_v = calc_step(delta_v);
    float step_a = calc_step(delta_a);

  if (delta_v > 0.0f) disp_v_mV += step_v;
  else if (delta_v < 0.0f) disp_v_mV -= step_v;

  if (delta_a > 0.0f) disp_a_mA += step_a;
  else if (delta_a < 0.0f) disp_a_mA -= step_a;

  // Записываем в глобальные переменные для дальнейшего использования при выводе
  vv = (float)disp_v_mV / 1000.0f;
  aa = (float)disp_a_mA / 1000.0f;
}



