#include "PfeifferModel.h"

/*
  PfeifferModel implementation
  */
const QString PfeifferModel::Pfeiffer_PORT = QString("/dev/ttyS5");

PfeifferModel::PfeifferModel(float updateInterval, QObject *parent) :
    QObject(parent)
//  , state_(OFF) // Initialize all fields to prevent random values
//  , controller_(NULL)
  , AbstractDeviceModel<PfeifferTPG262_t>()
  , updateInterval_(updateInterval)
{
    timer_ = new QTimer(this);
    timer_->setInterval(updateInterval_ * 1000);
    connect( timer_, SIGNAL(timeout()), this, SLOT(updateInformation()) );

    setDeviceEnabled(true);
    setControlsEnabled(true);
}

/**
  Sets up the communication with the Pfeiffer TPG262 gauge and retrieves the
  settings and read-outs.
  */
void PfeifferModel::initialize() {

  setDeviceState(INITIALIZING);

  renewController(Pfeiffer_PORT);

  bool enabled = ( controller_ != NULL ) && ( controller_->IsCommunication() );

  if ( enabled ) {
    setDeviceState(READY);
    updateInformation();
  }
  else {
    setDeviceState( OFF );
    delete controller_;
    controller_ = NULL;
  }
}

/// \see AbstractDeviceModel::setDeviceState
void PfeifferModel::setDeviceState( State state ) {

  if ( state_ != state ) {
    state_ = state;

    emit deviceStateChanged(state);
  }
}

/**
  Updates the cached information about the Pfeiffer chiller and signals any
  changes.
  */
void PfeifferModel::updateInformation() {

  if ( state_ == READY ) {
    emit informationChanged();
  }
}

/// Attempts to enable/disable the (communication with) the Pfeiffer TPG262 gauge.
void PfeifferModel::setDeviceEnabled(bool enabled) {
  AbstractDeviceModel<PfeifferTPG262_t>::setDeviceEnabled(enabled);
}

void PfeifferModel::setControlsEnabled(bool enabled) {
  emit controlStateChanged(enabled);
}