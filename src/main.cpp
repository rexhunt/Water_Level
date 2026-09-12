#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define ADC_PIN 0 // GPIO Pin input ADC is attached to
#define ADC_AVERAGE 30  // Number of samples to average the ADC over
#define ADC_DELAY 100   // Number of ms to delay between ADC samples

#define PIN_NEOPIXEL 8  // Change this to your board's NeoPixel pin (e.g., 48 on some ESP32-S3 boards)
#define NUM_PIXELS 1     // Number of LEDs

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
/* Zigbee Analog configuration */
#define ZIGBEE_ANALOG_ENDPOINT 1

uint8_t button = BOOT_PIN;

ZigbeeAnalog zbAnalog = ZigbeeAnalog(ZIGBEE_ANALOG_ENDPOINT);

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

//Set LED by Zigbee
void setLED(bool value) {
  uint8_t intensity = 0;
  if (value){ intensity = 100;}
  xQueueOverwrite(LRed, &intensity);
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

  //Optional: set Zigbee device name and model
  zbAnalog.setManufacturerAndModel("RexO", "ZBTankLevel");

  // Set callback function for light change
  //zbLight.onLightChange(setLED);

  //Set Analog level information for Zigbee
  zbAnalog.addAnalogInput();
  zbAnalog.setAnalogInputApplication(ESP_ZB_ZCL_AI_PERCENTAGE_OTHER); //Not sure how to set this yet
  zbAnalog.setAnalogInputDescription("Tank Level %");
  zbAnalog.setAnalogInputResolution(0.1);
  
  //Add endpoint to Zigbee Core
  Serial.println("Adding ZigbeeAnalog endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbAnalog);

  // When all EPs are registered, start Zigbee. By default acts as ZIGBEE_END_DEVICE
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  //Report analog input
  //zbAnalog.setAnalogInputReporting(0, 30, 10);  // report every 30 seconds if value changes by 10
  zbAnalog.setAnalogInputReporting(0, 30, 0.1);  

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
    //delay(500); //Replaced by delay at end of loop 
    //Update zigbee level to number of counts
    //zbAnalog.setAnalogInput(counts);
  }

  // Checking button for factory reset and reporting
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }

  //Read ADC and pass to zigbee
 uint16_t adc = 0;
 float percent;
 for (int i = 0; i < ADC_AVERAGE; i++) {
  adc = adc + analogRead(ADC_PIN);
  adc = adc / ADC_AVERAGE;
  delay(ADC_DELAY);
 }
 
 //percent = adc * (4095 / ADC_VOLT_MAX);
 percent = ((float)adc / 4095) * 100;
 Serial.print(adc);
 Serial.print(" ADC Value in counts ");
 Serial.print(percent);
 Serial.println("% ADC Value in percent");
 zbAnalog.setAnalogInput(percent);

  //Delay to slow down loop a bit
  delay(500);
}

// put function definitions here: