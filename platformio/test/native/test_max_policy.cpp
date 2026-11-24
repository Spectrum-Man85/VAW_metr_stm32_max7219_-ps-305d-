#include "MaxPortPolicy.h"
#ifdef _MSC_VER
#define __attribute__(x)
#endif
#include "Calibration.h"
#include "EncoderInput.h"
#include "MeasurementFilters.h"
#define CHECK(x) if (!(x)) return __LINE__
extern "C" int _fltused = 0;
struct MemoryStorage {
  unsigned char bytes[1024] = {};
  bool begin() { return true; }
  size_t capacity() const { return sizeof(bytes); }
  bool readBytes(size_t offset, void* out, size_t count) {
    if (offset+count>sizeof(bytes)) return false;
    memcpy(out, bytes+offset,count); return true;
  }
  bool writeBytes(size_t offset, const void* in, size_t count) {
    if (offset+count>sizeof(bytes)) return false;
    memcpy(bytes+offset,in,count); return true;
  }
  bool commit() { return true; }
};
int main() {
  const float compensated = maxCompensatedVoltage(11.0f, 1.2f, 1.0f, 0.01f, true);
  CHECK(compensated > 10.987f && compensated < 10.989f);
  CHECK(maxCompensatedVoltage(11, 1.2f, 1, 0.01f, false)==11);
  CHECK(maxCompensatedVoltage(0, 1, 1, 0.01f, true)==0);
  CHECK(maxVoltageStepMultiplier(9.0f,1)==1);
  CHECK(maxVoltageStepMultiplier(12.0f,1)==10);
  CHECK(maxCurrentStepMultiplier(false,1)==1);
  CHECK(maxCurrentStepMultiplier(true,1)==10);
  CHECK(maxCurrentStepMultiplier(true,64)==640);
  CHECK(maxFourDigitNumber(9.9996f).digits==1000);
  CHECK(maxFourDigitNumber(9.9996f).decimalPosition==2);
  CHECK(maxFourDigitNumber(1.2345f).digits==1235);
  CHECK(!maxFourDigitNumber(-1).valid);
  CHECK(maxFourDigitNumber(10).digits==1000);
  CHECK(maxFourDigitNumber(10).decimalPosition==2);
  CHECK(kCurrentCalibrationEepromOffset+sizeof(CurrentCalibrationEepromRecord)<=848);
  CHECK(848+88<=kCalibrationEepromCapacity);
  CalibrationController<MemoryStorage> c;
  CHECK(c.beginStorage().storageReady);
  c.storageForTest().bytes[900] = 123;
  c.beginFirstVoltageCalibration();
  auto v=c.adjustVoltage(5.0f,true,1);
  CHECK(v.changed && v.displayDigits==5001);
  CHECK(c.finishFirstVoltageCalibration()==CalibrationEepromTransactionResult::kSaved);
  CHECK(c.storageForTest().bytes[900]==123);
  c.beginStorage();
  CHECK(!c.firstVoltageCalibrationPending());
  c.enterCurrentCalibration();
  CHECK(c.firstCurrentCalibrationPending());
  auto a=c.adjustCurrent(0.25f,true,10);
  CHECK(a.changed && a.displayDigits==2510);
  CHECK(c.finishFirstCurrentCalibration());
  c.beginStorage();
  CHECK(c.currentBaseCalibrationCompleted());
  CHECK(c.storageForTest().bytes[900]==123);
  c.processWrites(0);
  c.noteEncoderActivity(0);
  auto a2=c.adjustCurrent(0.25f,true,1);
  CHECK(a2.changed);
  CHECK(!c.processWrites(29999).currentSaved);
  CHECK(c.processWrites(30001).currentSaved);
  CHECK(c.storageForTest().bytes[900]==123);
  MeasurementFilters::MedianFilter<int,11> filter;
  for(int i=0;i<11;++i) filter.add(100);
  CHECK(filter.add(900)==100);
  MeasurementFilters::resetAll();
  auto frame=MeasurementFilters::process(12,0.25f,-1,-1,true);
  CHECK(frame.voltage.value==12 && frame.current.value==0.25f);
  EncoderDecoder decoder;
  decoder.initialize(true,true);
  CHECK(decoder.update(true,false)==EncoderEvent::None);
  CHECK(decoder.update(false,false)==EncoderEvent::None);
  CHECK(decoder.update(false,true)==EncoderEvent::None);
  CHECK(decoder.update(true,true)==EncoderEvent::Right);
  EncoderAcceleration acceleration(16);
  CHECK(acceleration.onDetent(true)==1);
  for(int i=0;i<30;++i) acceleration.onDetent(true);
  CHECK(acceleration.onDetent(false)==1);
  return 0;
}
