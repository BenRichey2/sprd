#define ANALOG_PIN A0


const int AUDIO_SAMPLE_FREQUENCY = 16000;
const int ADC_RES = 12;
const int RECORDING_DURATION_S = 1;
const int AUDIO_CAPTURE_BUFFER_SIZE = AUDIO_SAMPLE_FREQUENCY * RECORDING_DURATION_S;
int16_t audio_buffer[AUDIO_CAPTURE_BUFFER_SIZE];
// Sampling period in us -> subtract 5 to account for overhead.
unsigned long SAMPLING_PERIOD_US = ((1.0 / AUDIO_SAMPLE_FREQUENCY) * pow(10.0, 6)) - 5;

/*
* Reads <size> bytes of data from <ANALOG_PIN> using an
* <ADC_RESOLUTION>-bit ADC at a sampling rate of <AUDIO_SAMPLE_FREQUENCY>
* to <buffer>.
*/
void audio_read(int16_t *buffer, size_t size) {
  for (int i = 0; i < size; i++) {
    unsigned long new_time = micros();
    uint16_t val = analogRead(ANALOG_PIN);
    buffer[i] = val;
    while(micros() < (new_time + SAMPLING_PERIOD_US));
  }
}

void setup() {
  // Initialize serial connection for logging
  Serial.begin(9600);
  // Set the ADC resolution
  analogReadResolution(ADC_RES);
}

void loop() {
  Serial.print("Record 1s of audio data? (y/n): ");
  while(!Serial.available()) {
    delay(20);
  }
  Serial.println("");
  char input = Serial.read();
  if (input == 'y') {
    Serial.println("Recording...");
    audio_read(audio_buffer, AUDIO_CAPTURE_BUFFER_SIZE);
    Serial.println("Done.");
    Serial.println("Outputting raw CSV audio data:");
    for (int i = 0; i < AUDIO_CAPTURE_BUFFER_SIZE - 1; i++) {
      Serial.print((float) audio_buffer[i]);
      Serial.print(",");
    }
    Serial.println((float) audio_buffer[AUDIO_CAPTURE_BUFFER_SIZE - 1]);
  }
}
