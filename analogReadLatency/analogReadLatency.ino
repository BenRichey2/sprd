#define ANALOG_PIN A0

//unsigned long sampling_period_us = (1.0 / 16000) * pow(10.0, 6);
unsigned long sampling_period_us = 51;

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println(sampling_period_us);

}

void audio_read() {
  for (int i = 0; i < 16000; i++) {
    unsigned long new_time = micros();
    uint8_t val = analogRead(ANALOG_PIN);
    while(micros() < (new_time + sampling_period_us));
  }
}

void loop() {
/*
  unsigned long start, duration;
  start = millis();
  audio_read();
  duration = millis() - start;
  Serial.println(duration);
  */
  Serial.println(analogRead(ANALOG_PIN));
  delay(3);

}
