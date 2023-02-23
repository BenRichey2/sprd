/*
  This experimental code shows how to control the nRF52840 SAADC using a Timer and PPI.
  The SAADC uses EasyDMA to copy each sample into a variable in memory.
  The ADC samples pin A0.
  The Timer is started by pulling a digital pin LOW.
  At the end of the measurement the data is send via Serial to be displayed in the Serial Plotter.
  Note:
  - the maximum sampling rate is 200kSamples/s
  - this code has not been tested with a debugger, some samples might be lost due to mbedOS (needs further testing)
  - this code will likely not work when using analogRead() on other pins

  The circuit:
  - Arduino Nano 33 BLE/ BLE Sense board.

  This example code is in the public domain.
*/

#include "mbed.h"

#define SAMPLES_PER_SECOND  16000
#define PPI_CHANNEL         (7)

#define ADC_BUFFER_SIZE     1
volatile nrf_saadc_value_t adcBuffer[ADC_BUFFER_SIZE];
volatile bool adcFlag = false;


#define DATA_BUFFER_SIZE    16000
volatile nrf_saadc_value_t dataBuffer[DATA_BUFFER_SIZE];
uint32_t dataBufferIndex = 0;

#define BUTTON_START_PIN    10

#define DATA_SEND_INTERVAL  5


void setup()
{
  Serial.begin( 9600 );
  while ( !Serial );

  initADC();
  initTimer4();
  initPPI();
  pinMode( BUTTON_START_PIN, INPUT );
}

void loop()
{
  enum APP_STATE_TYPE { APP_WAIT_FOR_START,
                        APP_START_MEASUREMENT,
                        APP_COLLECT_SAMPLES,
                        APP_STOP_MEASUREMENT,
                        APP_SEND_DATA,
                        APP_STATE_RESTART = 255
                      };

  static uint32_t state = 0;

  switch ( state )
  {
    case APP_WAIT_FOR_START:
      state++;
      break;
    case APP_START_MEASUREMENT:
      Serial.println("Starting measurements");
      startTimer4();
      state++;
      break;
    case APP_COLLECT_SAMPLES:
      if ( adcFlag )
      {
        if ( copyData() == 0 )
        {
          state++;
        }
        adcFlag = false;
      }
      break;
    case APP_STOP_MEASUREMENT:
      stopTimer4();
      Serial.println("Done.");
      dataBufferIndex = 0;
      state++;
      break;
    case APP_SEND_DATA:
      if (sendData(DATA_SEND_INTERVAL) == 0)
        state++;
      break;
    default:
      dataBufferIndex = 0;
      state = 0;
      break;
  }
}


extern "C" void SAADC_IRQHandler_v( void )
{
  if ( NRF_SAADC->EVENTS_END != 0 )
  {
    NRF_SAADC->EVENTS_END = 0;
    adcFlag = true;
  }
}


void initADC()
{
  nrf_saadc_disable();

  NRF_SAADC->RESOLUTION = NRF_SAADC_RESOLUTION_12BIT;

  NRF_SAADC->CH[2].CONFIG = ( SAADC_CH_CONFIG_GAIN_Gain1_4    << SAADC_CH_CONFIG_GAIN_Pos ) |
                            ( SAADC_CH_CONFIG_MODE_SE         << SAADC_CH_CONFIG_MODE_Pos ) |
                            ( SAADC_CH_CONFIG_REFSEL_VDD1_4   << SAADC_CH_CONFIG_REFSEL_Pos ) |
                            ( SAADC_CH_CONFIG_RESN_Bypass     << SAADC_CH_CONFIG_RESN_Pos ) |
                            ( SAADC_CH_CONFIG_RESP_Bypass     << SAADC_CH_CONFIG_RESP_Pos ) |
                            ( SAADC_CH_CONFIG_TACQ_3us        << SAADC_CH_CONFIG_TACQ_Pos );

  NRF_SAADC->CH[2].PSELP = SAADC_CH_PSELP_PSELP_AnalogInput2 << SAADC_CH_PSELP_PSELP_Pos;
  NRF_SAADC->CH[2].PSELN = SAADC_CH_PSELN_PSELN_NC << SAADC_CH_PSELN_PSELN_Pos;

  NRF_SAADC->RESULT.MAXCNT = ADC_BUFFER_SIZE;
  NRF_SAADC->RESULT.PTR = ( uint32_t )&adcBuffer;

  NRF_SAADC->EVENTS_END = 0;
  nrf_saadc_int_enable( NRF_SAADC_INT_END );
  NVIC_SetPriority( SAADC_IRQn, 1UL );
  NVIC_EnableIRQ( SAADC_IRQn );

  nrf_saadc_enable();

  NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
  while ( NRF_SAADC->EVENTS_CALIBRATEDONE == 0 );
  NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
  while ( NRF_SAADC->STATUS == ( SAADC_STATUS_STATUS_Busy << SAADC_STATUS_STATUS_Pos ) );
}


void initTimer4()
{
  NRF_TIMER4->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER4->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER4->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;
  NRF_TIMER4->PRESCALER = 0;
  NRF_TIMER4->CC[0] = 16000000 / SAMPLES_PER_SECOND; // Needs prescaler set to 0 (1:1) 16MHz clock
//  NRF_TIMER4->TASKS_START = 1;
}

void startTimer4()
{
  NRF_TIMER4->TASKS_START = 1;
}

void stopTimer4()
{
  NRF_TIMER4->TASKS_STOP = 1;
}


void initPPI()
{
  NRF_PPI->CH[PPI_CHANNEL].EEP = ( uint32_t )&NRF_TIMER4->EVENTS_COMPARE[0];
  NRF_PPI->CH[PPI_CHANNEL].TEP = ( uint32_t )&NRF_SAADC->TASKS_START;
  NRF_PPI->FORK[PPI_CHANNEL].TEP = ( uint32_t )&NRF_SAADC->TASKS_SAMPLE;
  NRF_PPI->CHENSET = ( 1UL << PPI_CHANNEL );
}

int32_t copyData()
{
  dataBuffer[dataBufferIndex] = adcBuffer[0];
  dataBufferIndex = ( dataBufferIndex + 1 ) % DATA_BUFFER_SIZE;
  return dataBufferIndex;
}

int32_t sendData( uint32_t interval )
{
  static uint32_t previousMillis = 0;

  uint32_t currentMillis = millis();
  if ( currentMillis - previousMillis >= interval )
  {
    previousMillis = currentMillis;

    Serial.print( dataBuffer[dataBufferIndex] );
    Serial.print(",");
    dataBufferIndex = ( dataBufferIndex + 1 ) % DATA_BUFFER_SIZE;
    return dataBufferIndex;
  }
  return -1;
}
