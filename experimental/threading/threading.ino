#include <mbed.h>
#include <rtos.h>
#include <platform/Callback.h>


rtos::Semaphore s1(1);
rtos::Semaphore s2(1);
rtos::Thread t1;
rtos::Thread t2;

void printPing(void)
{
  while(true)
  {
    s1.acquire();
    Serial.print("Ping ");
    s2.release();
  }
}

void printPong(void)
{
  s2.acquire();
  Serial.println("Pong");
  s1.release();
}

void setup() {
  Serial.begin(9600);
  t1.start(mbed::callback(printPing));
  t2.start(mbed::callback(printPong));
}

void loop() {}
