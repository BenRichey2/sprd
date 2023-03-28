void setup() {
  // Initialize serial port w/ a 9600 baud rate
  Serial.begin(9600);
}

void loop() {

  Serial.print("Read 8-, 10-, and 12-bit resolution ADC values from pin A0? (y/n): ");
  while(!Serial.available())
  {
    delay(20);
  }
  Serial.println("");
  char input = Serial.read();
  if (input == 'y')
  {
    // Set analog resolution to 8 bits
    analogReadResolution(8);
    // Read and print value
    Serial.print("8-bit ADC value: ");
    Serial.println(analogRead(A0));

    // Set analog resolution to 10 bits
    analogReadResolution(10);
    // Read and print value
    Serial.print("10-bit ADC value: ");
    Serial.println(analogRead(A0));

    // Set analog resolution to 8 bits
    analogReadResolution(12);
    // Read and print value
    Serial.print("12-bit ADC value: ");
    Serial.println(analogRead(A0));
  }
}
