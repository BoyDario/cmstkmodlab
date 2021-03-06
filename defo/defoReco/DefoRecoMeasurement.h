#ifndef _DEFORECOMEASUREMENT_H
#define _DEFORECOMEASUREMENT_H

#include "DefoMeasurement.h"
#include "DefoPointRecognitionModel.h"

class DefoRecoMeasurement : public DefoMeasurement {
public:
  DefoRecoMeasurement(const QString& imageLocation, bool preview);

  void setRecoThresholdValue(DefoPointRecognitionModel::Threshold threshold, int value);
  int getRecoThresholdValue(DefoPointRecognitionModel::Threshold threshold);
  void setRecoHalfSquareWidthValue(int value) { recoPointRecognitionHalfSquareWidth_ = value; }
  int getRecoHalfSquareWidthValue() const { return recoPointRecognitionHalfSquareWidth_; }

  void write(const QDir& path);
  void read(const QDir& path);

protected:

  std::vector<int> recoPointRecognitionThresholds_;
  int recoPointRecognitionHalfSquareWidth_;
};

#endif // _DEFORECOMEASUREMENT_H
