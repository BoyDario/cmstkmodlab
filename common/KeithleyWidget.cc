#include "KeithleyWidget.h"

KeithleyWidget::KeithleyWidget(KeithleyModel* model,
                                       QWidget *parent) :
    QWidget(parent),
    model_(model)
{
  QVBoxLayout* layout = new QVBoxLayout(this);
  setLayout(layout);

  keithleyCheckBox_ = new QCheckBox("Enable Multimeter", this);
  layout->addWidget(keithleyCheckBox_);

  sensorControlWidget_= new QWidget(this);
  layout->addWidget(sensorControlWidget_);

  QGridLayout* sensorLayout = new QGridLayout(sensorControlWidget_);
  sensorControlWidget_->setLayout(sensorLayout);

  // Add all the temperature sensor displays
  for ( unsigned int i = 0; i < KeithleyModel::SENSOR_COUNT; ++i ) {
    sensorLayout->addWidget(new KeithleyTemperatureWidget(model_, i, this),
                            i/4, i%4);
  }

  connect(keithleyCheckBox_,
          SIGNAL(toggled(bool)),
          model_,
          SLOT(setDeviceEnabled(bool)));

  connect(model_,
          SIGNAL(deviceStateChanged(State)),
          this,
          SLOT(keithleyStateChanged(State)));

  connect(model_,
          SIGNAL(controlStateChanged(bool)),
          this,
          SLOT(controlStateChanged(bool)));

  keithleyStateChanged(model_->getDeviceState());
}

/// Updates the GUI when the Keithley multimeter is enabled/disabled.
void KeithleyWidget::keithleyStateChanged(State newState) {

  keithleyCheckBox_->setChecked(newState == READY || newState == INITIALIZING);
  sensorControlWidget_->setEnabled(newState == READY);

}

/// Updates the GUI when the Keithley multimeter is enabled/disabled.
void KeithleyWidget::controlStateChanged(bool enabled) {
  keithleyCheckBox_->setEnabled(enabled);
}

/* TemperatureWidget implementation */

const unsigned int KeithleyTemperatureWidget::LCD_SIZE = 5;
const QString KeithleyTemperatureWidget::LABEL_FORMAT = QString("Sensor %1");

KeithleyTemperatureWidget::KeithleyTemperatureWidget(KeithleyModel* model,
                                                             unsigned int sensor,
                                                             QWidget *parent) :
    QWidget(parent),
    model_(model),
    sensor_(sensor)
{
  QFormLayout* layout = new QFormLayout(this);
  setLayout(layout);

  enabledCheckBox_ = new QCheckBox(LABEL_FORMAT.arg(sensor), this);

  currentTempDisplay_ = new QLCDNumber(LCD_SIZE, this);
  currentTempDisplay_->setSegmentStyle(QLCDNumber::Flat);
  currentTempDisplay_->setSmallDecimalPoint(true);
  currentTempDisplay_->setNumDigits(5);

  currentGradientDisplay_ = new QLCDNumber(LCD_SIZE/2, this);
  currentGradientDisplay_->setSegmentStyle(QLCDNumber::Flat);
  currentGradientDisplay_->setSmallDecimalPoint(true);
  currentGradientDisplay_->setNumDigits(8);

  connect(model_,
          SIGNAL(sensorStateChanged(uint,State)),
          this,
          SLOT(sensorStateChagned(uint,State)));

  connect(model_,
          SIGNAL(temperatureChanged(uint,double)),
          this,
          SLOT(sensorTemperatureChanged(uint,double)));

  connect(model_,
          SIGNAL(temperatureGradientChanged(uint,double)),
          this,
          SLOT(sensorTemperatureGradientChanged(uint,double)));

  connect(enabledCheckBox_,
          SIGNAL(toggled(bool)),
          this,
          SLOT(enabledCheckBoxToggled(bool)));

  connect(model_,
          SIGNAL(deviceStateChanged(State)),
          this,
          SLOT(keithleyStateChanged(State)));

  connect(model_,
          SIGNAL(controlStateChanged(bool)),
          this,
          SLOT(controlStateChanged(bool)));

  layout->addRow(enabledCheckBox_);
  layout->addRow(QString::fromUtf8("T"), currentTempDisplay_);
  layout->addRow(QString::fromUtf8("dT/dt") ,currentGradientDisplay_);
  //layout->addRow(QString::fromUtf8("T (°C)"), currentTempDisplay_);
  //layout->addRow(QString::fromUtf8("dT/dt (°C/min)") ,currentGradientDisplay_);

  updateWidgets();
}

/// Convenience method to set all the widgets correctly.
void KeithleyTemperatureWidget::updateWidgets() {

  State sensorState = model_->getSensorState(sensor_);

  enabledCheckBox_->setChecked(
       sensorState == READY
    || sensorState == INITIALIZING
  );

  currentTempDisplay_->setEnabled( sensorState == READY );
}

/// Updates the GUI according to the current device state.
void KeithleyTemperatureWidget::keithleyStateChanged(State /*state*/) {
  updateWidgets();
}

/// Updates the GUI when the Keithley multimeter is enabled/disabled.
void KeithleyTemperatureWidget::controlStateChanged(bool enabled) {
  if (enabled) {
    State state = model_->getDeviceState();
    enabledCheckBox_->setEnabled(state == READY
			      || state == INITIALIZING);
  } else {
    enabledCheckBox_->setEnabled(false);
  }
}

/// Updates the GUI according to the current sensor state.
void KeithleyTemperatureWidget::sensorStateChagned(unsigned int sensor,
                                                       State /* state */)
{
  if ( sensor_ == sensor )
      updateWidgets();
}

/// Relays the new temperature to the history model.
void KeithleyTemperatureWidget::sensorTemperatureChanged(unsigned int sensor,
                                                         double temperature)
{
    if (sensor_!=sensor) return;
    currentTempDisplay_->display(QString::number(temperature, 'f', 2));
}

/// Relays the new temperature to the history model.
void KeithleyTemperatureWidget::sensorTemperatureGradientChanged(unsigned int sensor,
                                                                 double gradient)
{
    if (sensor_!=sensor) return;
    currentGradientDisplay_->display(QString::number(gradient, 'f', 5));
}

/// Updates the model according to the GUI change.
void KeithleyTemperatureWidget::enabledCheckBoxToggled(bool enabled)
{
  model_->setSensorEnabled(sensor_, enabled);
}
