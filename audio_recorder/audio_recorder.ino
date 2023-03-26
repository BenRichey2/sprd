#define ANALOG_PIN A0
#define PPI_CHANNEL (7)
#define ADC_BUFFER_SIZE 1
#define SAMPLING_FREQUENCY 16000
#define RECORDING_LEN_S 5

#include <mbed.h>

const unsigned int BUFF_SIZE = RECORDING_LEN_S * SAMPLING_FREQUENCY;
int16_t g_audio_capture_buffer[BUFF_SIZE];
volatile nrf_saadc_value_t adcBuffer[ADC_BUFFER_SIZE];
volatile int dataBufferIndex = 0;
volatile bool done = false;

// This is the callback that is executed every time
// the timer throws an interrupt. The ADC is running continuously
// and asynchronously from the CPU. Simply copy a value from the ADC
// to our audio buffer and increment the buffer index.
extern "C" void SAADC_IRQHandler_v( void )
{
  if (NRF_SAADC->EVENTS_END != 0)
  {
    NRF_SAADC->EVENTS_END = 0;
    g_audio_capture_buffer[dataBufferIndex] = adcBuffer[0];
    dataBufferIndex = (dataBufferIndex + 1) % BUFF_SIZE;
    if (dataBufferIndex == 0) {
      stopTimer4();
      done = true;
    }
  }
}

// Initialize the analog to digital converter (ADC)
// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.nrf52832.ps.v1.1%2Fsaadc.html
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

// Initialize a timer that triggers an interrupt every
// 1 / <SAMPLING_FREQUENCY> seconds.
// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.nrf52832.ps.v1.1%2Fppi.html
void initTimer4()
{
  NRF_TIMER4->MODE = TIMER_MODE_MODE_Timer;
  NRF_TIMER4->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
  NRF_TIMER4->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;
  NRF_TIMER4->PRESCALER = 0;
  NRF_TIMER4->CC[0] = 16000000 / SAMPLING_FREQUENCY; // Needs prescaler set to 0 (1:1) 16MHz clock
}

// Start the timer to trigger our interrupt function
// that reads data from the ADC
void startTimer4()
{
  NRF_TIMER4->TASKS_START = 1;
}

void stopTimer4()
{
  NRF_TIMER4->TASKS_STOP = 1;
}

// Initialize the Programmable Peripheral Interconnect (PPI)
// This allows us to have the ADC run separately from the CPU
// and trigger an interrupt when its time to copy a sample from
// the ADC to program memory.
// https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.nrf52832.ps.v1.1%2Fppi.html
void initPPI()
{
  NRF_PPI->CH[PPI_CHANNEL].EEP = ( uint32_t )&NRF_TIMER4->EVENTS_COMPARE[0];
  NRF_PPI->CH[PPI_CHANNEL].TEP = ( uint32_t )&NRF_SAADC->TASKS_START;
  NRF_PPI->FORK[PPI_CHANNEL].TEP = ( uint32_t )&NRF_SAADC->TASKS_SAMPLE;
  NRF_PPI->CHENSET = ( 1UL << PPI_CHANNEL );
}

void setup() {
  Serial.begin(9600);
  initADC();
  initTimer4();
  initPPI();
  delay(3000);
}

void loop() {
  Serial.print("Record 5s of audio data? (y/n): ");
  while(!Serial.available());
  Serial.println("");
  char input = Serial.read();
  if (input == 'y') {
    Serial.println("Recording...");
    startTimer4();
    while (!done);
    done = false;
    Serial.println("Done.");
    Serial.println("Outputting raw CSV audio data:");
    for (int i = 0; i < BUFF_SIZE - 1; i++) {
      Serial.print((float) g_audio_capture_buffer[i]);
      Serial.print(",");
    }
    Serial.println((float) g_audio_capture_buffer[BUFF_SIZE - 1]);
  }
}
