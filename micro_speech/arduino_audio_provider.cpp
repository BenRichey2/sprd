/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#if defined(ARDUINO) && !defined(ARDUINO_ARDUINO_NANO33BLE)
#define ARDUINO_EXCLUDE_CODE
#endif  // defined(ARDUINO) && !defined(ARDUINO_ARDUINO_NANO33BLE)

#ifndef ARDUINO_EXCLUDE_CODE

#ifndef PPI_CHANNEL
#define PPI_CHANNEL (7)
#endif

#ifndef ANALOG_PIN
#define ANALOG_PIN A0
#endif

#ifndef ADC_BUFFER_SIZE
#define ADC_BUFFER_SIZE 1
#endif

#ifndef SAMPLING_FREQUENCY
#define SAMPLING_FREQUENCY 20000
#endif

#include <algorithm>
#include <cmath>

#include <mbed.h>
#include <rtos.h>
#include <platform/Callback.h>

#include "PDM.h"
#include "audio_provider.h"
#include "micro_features_micro_model_settings.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "test_over_serial/test_over_serial.h"


using namespace test_over_serial;

namespace {
bool g_is_audio_initialized = false;
// An internal buffer able to fit 16x our sample size
constexpr int kAudioCaptureBufferSize = DEFAULT_PDM_BUFFER_SIZE * 32;
int16_t g_audio_capture_buffer[kAudioCaptureBufferSize];
// A buffer that holds our output
int16_t g_audio_output_buffer[kMaxAudioSampleSize];
// Mark as volatile so we can check in a while loop to see if
// any samples have arrived yet.
volatile int32_t g_latest_audio_timestamp = 0;
// test_over_serial sample index
uint32_t g_test_sample_index;
// test_over_serial silence insertion flag
bool g_test_insert_silence = true;
// Thread to continuously read in audio data
rtos::Thread audio_capture_thread;
// Debug
volatile int num_captures = 0;
volatile bool adcFlag = false;
volatile nrf_saadc_value_t adcBuffer[ADC_BUFFER_SIZE];
volatile int dataBufferIndex = 0;
}  // namespace

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
  NRF_TIMER4->CC[0] = 16000000 / SAMPLING_FREQUENCY; // Needs prescaler set to 0 (1:1) 16MHz clock
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
  g_audio_capture_buffer[dataBufferIndex] = adcBuffer[0];
  dataBufferIndex = ( dataBufferIndex + 1 ) % kAudioCaptureBufferSize;
  return dataBufferIndex;
}

int audio_read() {
  enum APP_STATE_TYPE { APP_WAIT_FOR_START,
                        APP_START_MEASUREMENT,
                        APP_COLLECT_SAMPLES,
                        APP_STOP_MEASUREMENT,
                        APP_STATE_RESTART = 255
                      };

  static uint32_t state = 0;
  bool stop = false;
  while (!stop) {
    switch (state)
    {
      case APP_WAIT_FOR_START:
        state++;
        break;
      case APP_START_MEASUREMENT:
        startTimer4();
        state++;
        break;
      case APP_COLLECT_SAMPLES:
        if (adcFlag)
        {
          if ((copyData() % DEFAULT_PDM_BUFFER_SIZE) == 0)
          {
            state++;
          }
          adcFlag = false;
        }
        break;
      case APP_STOP_MEASUREMENT:
        stopTimer4();
        state++;
        break;
      default:
        state = 0;
        stop = true;
        break;
    }
  }
  return DEFAULT_PDM_BUFFER_SIZE;
}

void CaptureSamples() {
    // This is how many bytes of new data we have each time this is called
    // const int number_of_samples = DEFAULT_PDM_BUFFER_SIZE / 2;
    const int number_of_samples = DEFAULT_PDM_BUFFER_SIZE;
    while (true) {
      // Calculate what timestamp the last audio sample represents
      const int32_t time_in_ms =
          g_latest_audio_timestamp +
          (number_of_samples / (kAudioSampleFrequency / 1000));
      // Determine the index, in the history of all samples, of the last sample
      const int32_t start_sample_offset =
          g_latest_audio_timestamp * (kAudioSampleFrequency / 1000);
      // Determine the index of this sample in our ring buffer
      const int capture_index = start_sample_offset % kAudioCaptureBufferSize;
      // Read the data to the correct place in our buffer
      int num_read = audio_read();
      if (num_read != DEFAULT_PDM_BUFFER_SIZE) {
        MicroPrintf("### short read (%d/%d) @%dms", num_read,
                    DEFAULT_PDM_BUFFER_SIZE, time_in_ms);
        }
      if (num_captures < 33)
        num_captures++;
      else {
        MicroPrintf("Just captured 1s of audio");
        num_captures = 0;
      }
      // This is how we let the outside world know that new audio data has arrived.
      g_latest_audio_timestamp = time_in_ms;
  }
}


TfLiteStatus InitAudioRecording() {
  if (!g_is_audio_initialized) {
    //analogReadResolution(8);
    initADC();
    initTimer4();
    initPPI();
    audio_capture_thread.start(mbed::callback(CaptureSamples));
    /*
    // Hook up the callback that will be called with each sample
    PDM.onReceive(CaptureSamples);
    // Start listening for audio: MONO @ 16KHz
    PDM.begin(1, kAudioSampleFrequency);
    // gain: -20db (min) + 6.5db (13) + 3.2db (builtin) = -10.3db
    PDM.setGain(13);
    */
    // Block until we have our first audio sample
    while (!g_latest_audio_timestamp) {
    }
    g_is_audio_initialized = true;
  }

  return kTfLiteOk;
}

TfLiteStatus GetAudioSamples(int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
  // This next part should only be called when the main thread notices that the
  // latest audio sample data timestamp has changed, so that there's new data
  // in the capture ring buffer. The ring buffer will eventually wrap around and
  // overwrite the data, but the assumption is that the main thread is checking
  // often enough and the buffer is large enough that this call will be made
  // before that happens.

  // Determine the index, in the history of all samples, of the first
  // sample we want
  const int start_offset = start_ms * (kAudioSampleFrequency / 1000);
  // Determine how many samples we want in total
  const int duration_sample_count =
      duration_ms * (kAudioSampleFrequency / 1000);
  for (int i = 0; i < duration_sample_count; ++i) {
    // For each sample, transform its index in the history of all samples into
    // its index in g_audio_capture_buffer
    const int capture_index = (start_offset + i) % kAudioCaptureBufferSize;
    // Write the sample to the output buffer
    g_audio_output_buffer[i] = g_audio_capture_buffer[capture_index];
  }

  // Set pointers to provide access to the audio
  *audio_samples_size = duration_sample_count;
  *audio_samples = g_audio_output_buffer;

  return kTfLiteOk;
}

namespace {

void InsertSilence(const size_t len, int16_t value) {
  for (size_t i = 0; i < len; i++) {
    const size_t index = (g_test_sample_index + i) % kAudioCaptureBufferSize;
    g_audio_capture_buffer[index] = value;
  }
  g_test_sample_index += len;
}

int32_t ProcessTestInput(TestOverSerial& test) {
  constexpr size_t samples_16ms = ((kAudioSampleFrequency / 1000) * 16);

  InputHandler handler = [](const InputBuffer* const input) {
    if (0 == input->offset) {
      // don't insert silence
      g_test_insert_silence = false;
    }

    for (size_t i = 0; i < input->length; i++) {
      const size_t index = (g_test_sample_index + i) % kAudioCaptureBufferSize;
      g_audio_capture_buffer[index] = input->data.int16[i];
    }
    g_test_sample_index += input->length;

    if (input->total == (input->offset + input->length)) {
      // allow silence insertion again
      g_test_insert_silence = true;
    }
    return true;
  };

  test.ProcessInput(&handler);

  if (g_test_insert_silence) {
    // add 16ms of silence just like the PDM interface
    InsertSilence(samples_16ms, 0);
  }

  // Round the timestamp to a multiple of 64ms,
  // This emulates the PDM interface during inference processing.
  g_latest_audio_timestamp = (g_test_sample_index / (samples_16ms * 4)) * 64;
  return g_latest_audio_timestamp;
}

}  // namespace

int32_t LatestAudioTimestamp() {
  TestOverSerial& test = TestOverSerial::Instance(kAUDIO_PCM_16KHZ_MONO_S16);
  if (!test.IsTestMode()) {
    // check serial port for test mode command
    test.ProcessInput(nullptr);
  }
  if (test.IsTestMode()) {
    if (g_is_audio_initialized) {
      // stop capture from hardware
      PDM.end();
      g_is_audio_initialized = false;
      g_test_sample_index =
          g_latest_audio_timestamp * (kAudioSampleFrequency / 1000);
    }
    return ProcessTestInput(test);
  } else {
    // CaptureSamples() updated the timestamp
    return g_latest_audio_timestamp;
  }
  // NOTREACHED
}

#endif  // ARDUINO_EXCLUDE_CODE
