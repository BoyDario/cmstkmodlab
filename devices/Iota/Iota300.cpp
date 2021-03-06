#include <string.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <utility>
#include <fstream>

//#####################
// TODO:
// query error codes
//#####################

#include "Iota300ComHandler.h"

#include "Iota300.h"

///
///
///
Iota300::Iota300( const ioport_t ioPort )
  : VIota300(ioPort),
    uDelay_(250000)
{
  comHandler_ = new Iota300ComHandler( ioPort );
  isCommunication_ = false;
  Device_Init();
}

///
/// returns success flag
///
bool Iota300::SetFlow( const float flow ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::SetFlow] -- DEBUG: Called." << std::endl;
  #endif

  if( flow > Iota300UpperFlowLimit || flow < Iota300LowerFlowLimit ) {
    std::cerr << " [Iota300::SetFlow] ** ERROR: Flow in ml/min =" 
	      << flow << " exceeds soft limits." << std::endl;
    std::cerr << "  > (s. Iota300 class definition)" << std::endl;
    return false;
  }

  char buffer[10];

  int iFlow = flow;// * 100.;
  sprintf(buffer, "%03d", iFlow);
  // std::cout<<"flow = "<<iFlow<<std::endl;

  std::stringstream theCommand;
  theCommand << "#FSw" << buffer;
  // std::cout<<"buffer = "<<buffer<<std::endl;

  comHandler_->SendCommand( theCommand.str().c_str() );
  usleep( uDelay_ );

  comHandler_->SendCommand( "#FAr" );
  usleep( uDelay_ );

  comHandler_->ReceiveString( buffer );
  usleep( uDelay_ );
  StripBuffer( buffer );
  // std::cout<<"buffer = "<<buffer<<std::endl;

  if( std::fabs( iFlow - ToInteger(buffer) ) > 1 ) {
    std::cerr << " [Iota300::SetFlow] ** ERROR: check failed." << std::endl;
    std::cerr << "  > Expected: Q=" << flow << " but received (string):"
	      << buffer << "." << std::endl;
    return false;
  }
  
  return true;
}

///
/// return success flag
///
bool Iota300::SetPressure( const float pressure ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::SetPressure] -- DEBUG: Called." << std::endl;
  #endif

  if( pressure > Iota300UpperPressureLimit || pressure < Iota300LowerPressureLimit ) {
    std::cerr << " [Iota300::SetFlow] ** ERROR: p =" 
	      << pressure << " exceeds soft limits." << std::endl;
    std::cerr << "  > (s. Iota300 class definition)" << std::endl;
    return false;
  }
///
/// return success flag
///
  char buffer[10];

  int iPressure = pressure;// * 100.;
  sprintf(buffer, "%03d", iPressure);

  std::stringstream theCommand;
  theCommand << "#PSw" << buffer;

  comHandler_->SendCommand( theCommand.str().c_str() );
  usleep( uDelay_ );

  comHandler_->SendCommand( "#PAr" );
  usleep( uDelay_ );

  comHandler_->ReceiveString( buffer );
  usleep( uDelay_ );
  StripBuffer( buffer );

  if( std::fabs( iPressure - ToInteger(buffer) ) > 1 ) {
    std::cerr << " [Iota300::SetPressure] ** ERROR: check failed." << std::endl;
    std::cerr << "  > Expected: p=" << pressure << " but received (string):"
	      << buffer << "." << std::endl;
    return false;
  }
  
  return true;
}

///
/// return success flag
///
bool Iota300::SetPumpOn( void ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::SetPumpOn] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[100];

  usleep( uDelay_ );
  comHandler_->SendCommand( "#STw2" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  StripBuffer( buffer );

  int status = ToInteger(buffer);

  if( status != 2 ) {
    std::cerr << " [Iota300::SetPumpOn] ** ERROR: check failed."
        << std::endl;
    std::cerr << "  > Expected: ON (2) but received (string):"
        << buffer << "." << std::endl;
    return false;
  }

  return true;
}
///
/// return success flag
///
bool Iota300::SetPumpOff( void ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::SetPumpOff] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[100];

  usleep( uDelay_ );
  comHandler_->SendCommand( "#STw0" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  StripBuffer( buffer );

  int status = ToInteger(buffer);

  if( status != 0 ) {
    std::cerr << " [Iota300::SetPumpOff] ** ERROR: check failed."
        << std::endl;
    std::cerr << "  > Expected: Off (0) but received (string):"
        << buffer << "." << std::endl;
    return false;
  }

  return true;
}

///
/// return success flag
///
bool Iota300::SetStatus( const float status ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::SetStatus] -- DEBUG: Called." << std::endl;
  #endif

  /*
  if( Flow > __IOTA300_UPPER_STATUS_LIMIT || status < __IOTA300_LOWER_STATUS_LIMIT ) {
    std::cerr << " [Iota300::SetStatus] ** ERROR: status =" 
	      << status << " exceeds soft limits." << std::endl;
    std::cerr << "  > (s. Iota300 class definition)" << std::endl;
    return false;
  }
  */

  char buffer[100];

  int iStatus = status;// * 0.1;// * 100.;
  sprintf(buffer, "%d", iStatus);

  std::stringstream theCommand;
  theCommand << "#STw" << buffer;

  comHandler_->SendCommand( theCommand.str().c_str() );
  usleep( uDelay_ );

  comHandler_->SendCommand( "#STr" );
  usleep( uDelay_ );

  comHandler_->ReceiveString( buffer );
  usleep( uDelay_ );
  StripBuffer( buffer );

  if( std::fabs( iStatus - ToInteger(buffer) ) > 1 ) {
    std::cerr << " [Iota300::SetStatus] ** ERROR: check failed." << std::endl;
    std::cerr << "  > Expected: status=" << status << " but received (string):"
	      << buffer << "." << std::endl;
    return false;
  }
  
  return true;
}

///
/// return success flag
///
float Iota300::GetSetFlow( void ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::GetFlow] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[1000];

  usleep( uDelay_ );
  // comHandler_->flush();
  comHandler_->SendCommand( "#FSr" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  StripBuffer( buffer );
//   std::cout<<"getflow buffer = "<< buffer <<std::endl;
  

  return ToFloat(buffer);
}

///
///
///
float Iota300::GetActFlow( void ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::GetFlow] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[1000];

  usleep( uDelay_ );
  comHandler_->SendCommand( "#FAr" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  // std::cout<<"getflow buffer = "<< buffer <<std::endl;
  StripBuffer( buffer );
  
  return ToFloat(buffer);
}

///
///
///
float Iota300::GetSetPressure( void ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::GetPressure] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[1000];

  usleep( uDelay_ );
  comHandler_->SendCommand( "#PSr" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  StripBuffer( buffer );

  //std::cout << buffer << std::endl;

  return ToFloat(buffer);
}
///
///
///
float Iota300::GetActPressure( void ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::GetPressure] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[1000];

  usleep( uDelay_ );
  comHandler_->SendCommand( "#PAr" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  StripBuffer( buffer );

  //std::cout << buffer << std::endl;

  return ToFloat(buffer);
}
///
///
///
float Iota300::GetStatus( void ) const {

#ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::GetStatus] -- DEBUG: Called." << std::endl;
#endif

  char buffer[1000];

  usleep( uDelay_ );
  comHandler_->SendCommand( "#STr" );

  usleep( uDelay_ );
  comHandler_->ReceiveString( buffer );
  StripBuffer( buffer );

  //std::cout << buffer << std::endl;

  return ToFloat(buffer);
}

///
/// strip trailing newlines & stuff
/// from machine answer
///
void Iota300::StripBuffer( char* buffer ) const {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::StripBuffer] -- DEBUG: Called." << std::endl;
  std::cout << "[Iota300::StripBuffer] -- DEBUG: Buffer is:" << std::endl;
  std::cout << " > " << buffer << std::endl;
  #endif

  for( unsigned int c = 0; c < strlen( buffer ); ++c ) {
    if( '\n' == buffer[c] || '\r' == buffer[c] ) {
      buffer[c] = 0;
      break;
    }
  }
}

int Iota300::ToInteger(const char* buffer) const
{
  std::string temp(buffer);
  temp.erase(0, 3);

  return std::atoi( temp.c_str() );
}

float Iota300::ToFloat(const char* buffer) const
{
  std::string temp(buffer);
  temp.erase(0, 3);

  return std::atof( temp.c_str() );// * 10;
}

///
/// read back software version
/// to check communication with device
///
void Iota300::Device_Init( void ) {

  #ifdef __IOTA300_DEBUG
  std::cout << "[Iota300::Device_Init] -- DEBUG: Called." << std::endl;
  #endif

  char buffer[1000];
  
  comHandler_->SendCommand( "#STr" );
  usleep( uDelay_ );

  comHandler_->ReceiveString( buffer );

  usleep( uDelay_ );
  StripBuffer( buffer );
  std::string temp(buffer);
  // std::cout<<"status = "<<buffer<<std::endl;
  if (temp.compare(0, 2, "ST")!=0) {
    std::cerr << " [Iota300::Device_Init] ** ERROR: Device communication problem." << std::endl;
    isCommunication_ = false;
    return;
  }

  isCommunication_ = true;
}
