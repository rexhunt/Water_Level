#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define PIN_NEOPIXEL 8  // Change this to your board's NeoPixel pin (e.g., 48 on some ESP32-S3 boards)
#define NUM_PIXELS 1     // Number of LEDs

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

/*Create separate task for controlling Onboard LED
  This allows delay without affecting other parts of loop()
*/
//Create handles for tasks
TaskHandle_t LED_h;

//Create handles for Queues
QueueHandle_t LCount = NULL;
QueueHandle_t LRed = NULL;
QueueHandle_t LGreen = NULL;
QueueHandle_t LBlue = NULL;

void LED_Code(void * parameter) {
  //Declare variables
  int counter = 0;
  uint8_t red, green, blue = 0;
  Serial.println("LED Loop initialized");

  //Make code loop
  for(;;){
    // Get updated colours if changed
    if (xQueuePeek(LRed, &red, 0)){ //Set timeout to 0 to return immediately if queue is empty. Otherwise will wait forever
      //Serial.print(red);
      //Serial.println(" LED Red Value");
    }
    if (xQueuePeek(LGreen, &green, 0)){
      //Serial.print(green);
      //Serial.println(" LED Green Value");
    }
    if (xQueuePeek(LBlue, &blue, 0)){
      //Serial.print(blue);
      //Serial.println(" LED Blue Value");
    }

    // Control the LED here
    pixels.setPixelColor(0, pixels.Color(red, green, blue)); // Set pixel to colour
    pixels.show();   // Send the updated color to the hardware
    vTaskDelay(500 / portTICK_PERIOD_MS);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn pixel off
    pixels.show();   // Send the updated color
    vTaskDelay(1000 / portTICK_PERIOD_MS); 

    //Increment counter and update queue
    counter = counter + 1;
    xQueueSend(LCount, &counter, portMAX_DELAY);
  }
}


// put function declarations here:

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Initialize the digital pin as an output
  pixels.begin();
  
  //Create queues for inter task comms
  LCount = xQueueCreate(5, sizeof(int)); //queuesize of 5, not sure what this should be set to
  LRed = xQueueCreate(1, sizeof(uint8_t)); 
  LGreen = xQueueCreate(1, sizeof(uint8_t)); // Colour queues are only 1 long, more like global variable than FIFO queue, use peek to leave value intact
  LBlue = xQueueCreate(1, sizeof(uint8_t)); 
  if (LCount == NULL && LRed == NULL && LGreen == NULL && LBlue == NULL) {
    Serial.println("Failed to create queue!");
    while (1);
  }

  //Create task for controlling the LED
  xTaskCreatePinnedToCore(
      LED_Code, /* Function to implement the task */
      "LED", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &LED_h,  /* Task handle. */
      0); /* Core where the task should run */
  
  //Set initial LED Colour
  uint8_t colour = 10;
  xQueueOverwrite(LBlue, &colour);

  Serial.println("setup() finished");
}

void loop() {
  // put your main code here, to run repeatedly:

  //Get the number of cycles the LED has flashed
  int counts;
  if (xQueueReceive(LCount, &counts, 0)){ //portMAX_DELAY will hold up loop until the queue is updated
    Serial.print(counts);
    Serial.println(" LED Flash Count");
    //Make the green LED change brightness
    xQueueOverwrite(LGreen, &counts);
    delay(500);
  }
}

// put function definitions here: