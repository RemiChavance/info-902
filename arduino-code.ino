#include <math.h>

const int pinTempSensor = A0;     // Temperature Sensor connect to A0
const int pinMoistureSensor = A1; // Moisture Sensor connect to A1
const int pinLightSensor = A2;    // Light Sensor connect to A2

#if defined(ARDUINO_ARCH_AVR)
#define debug  Serial
#elif defined(ARDUINO_ARCH_SAMD) ||  defined(ARDUINO_ARCH_SAM)
#define debug  SerialUSB
#else
#define debug  Serial
#endif

// LCD Screen setup
#include <Wire.h>
#include "rgb_lcd.h"
rgb_lcd lcd;


// Alert system
int alerts[4] = {0, 0, 0, 0};
int alertsIndex = 0;
/*0 -> temperature
  1 -> soil moisture
  2 -> light
  3 -> light resistance */
float MIN_TEMPERATURE = 18;
float MAX_TEMPERATURE = 24;
float MIN_SOIL_MOISTURE = 0;
float MAX_SOIL_MOISTURE = 50;
float MIN_LIGHT = 500;
float MAX_LIGHT = 1000;
float MIN_LIGHT_RESISTANCE = 2;
float MAX_LIGHT_RESISTANCE = 90;

// Buttons
const int touchPin6=6;
const int touchPin5=5;

// Questions
int questionsIndex = 0;
int nbQuestions = 3;
const char* questions[3][2] = {
  {"Dois-je arroser ?", "1"},
  {"Dois-je eclairer ?", "2"},
  {"Suis-je innovant ?", "-1"}
};
char* goodAnswer = "JUSTE ! Bravo :)                ";
char* badAnswer  = "FAUX ! Dommage  :(              ";
int enableQuestion = 1;

void setup()
{
  Serial.begin(9600);
  lcd.begin(16, 2);
  pinMode(touchPin6, INPUT);
  pinMode(touchPin5, INPUT);
}

// Return the temperature, in celsius.
float getTemperature()
{
  int B = 4275;
  int R0 = 100000;
  int a = analogRead(pinTempSensor);
  float R = 1023.0/a-1.0;
  R = R0*R;
  return 1.0/(log(R/R0)/B+1/298.15)-273.15; // convert to temperature via datasheet
}

// Return soil moisture in percentage.
float getSoilMoisture() 
{
  // < 300 --> DRY
  // < 600 --> MOIST
  // > 600 --> WET
  float sensorValue = analogRead(pinMoistureSensor);
  return map(sensorValue, 0, 1023, 0, 100);
}

// Return light value.
float getLight()
{
  return analogRead(pinLightSensor);
  // return map(sensorValue, 0, 1023, 0, 1000); 
}

// Return light resistance.
float getLightResistance()
{
  float light = getLight();
  return (float)(1023 - light) * 10 / light;
}


// Diplay an alert on LCD screen.
void displayAlert(char* title, float value, char* unit, char color)
{
  int colorR = 0;
  int colorG = 0;
  int colorB = 0;
  switch (color) {
    case 'R':
      colorR = 255;
      break;
    case 'G':
      colorG = 255;
      break;
    case 'B':
      colorB = 255;
      break;
  }
  lcd.clear();
  lcd.setRGB(colorR, colorG, colorB);
  lcd.print(title);
  lcd.setCursor(1, 1);
  if (value != -1) {
    lcd.print(value);
    lcd.print(unit);
  }
}

//
void handleAlerts(float temperature, float soilMoisture, float light, float lightResistance)
{
  /*
  0 -> temperature
  1 -> soil moisture
  2 -> luminosity
  3 -> light resistance
  */

  int thereIsAnAlert = 0;

  if (temperature < MIN_TEMPERATURE || temperature > MAX_TEMPERATURE) {
    alerts[0] = 1;
    thereIsAnAlert = 1;
  } else {
    alerts[0] = 0;
  }

  
  if (soilMoisture < MIN_SOIL_MOISTURE || soilMoisture > MAX_SOIL_MOISTURE) {
    alerts[1] = 1;
    thereIsAnAlert = 1;
  } else {
    alerts[1] = 0;
  }

  
  if (light < MIN_LIGHT || light > MAX_LIGHT) {
    alerts[2] = 1;
    thereIsAnAlert = 1;
  } else {
    alerts[2] = 0;
  }

  
  if (lightResistance < MIN_LIGHT_RESISTANCE || lightResistance > MAX_LIGHT_RESISTANCE) {
    alerts[3] = 1;
    thereIsAnAlert = 1;
  } else {
    alerts[3] = 0;
  }

  if (alerts[alertsIndex]) {
    switch (alertsIndex) {
      case 0:
        displayAlert("Temperature", temperature, " C", 'R');
        break;
      case 1:
        displayAlert("Humidite Sol", soilMoisture, " %", 'R');
        break;
      case 2:
        displayAlert("Luminosite", light, "", 'R');
        break;
      case 3:
        displayAlert("Resistance Lum.", lightResistance, "", 'R');
        break;
    }
  }

  alertsIndex = (alertsIndex + 1) % 4;

  if(!thereIsAnAlert) {
    displayAlert("All Good !", -1, "", 'G');
  }

  // Serial.print("\nalert index = ");
  // Serial.print(alertsIndex);
  // Serial.print("\nthere is an alert = ");
  // Serial.print(thereIsAnAlert);
}

void displayLongText(char* text)
{
  lcd.setRGB(255, 255, 255);
  lcd.setCursor(0, 0);

  // print from 0 to 9:
  for (int thisChar = 0; thisChar < 16; thisChar++)
  {
    if (text[thisChar] == '\0') break;
    lcd.print(text[thisChar]);
    delay(100);
  }

  lcd.setCursor(0, 1);

  // print from 0 to 9:
  for (int thisChar = 16; thisChar < 32; thisChar++)
  {
    if (text[thisChar] == '\0') break;
    lcd.print(text[thisChar]);
    delay(100);
  }
}

int requestAnswer(char* question)
{
  int YESanswer = 0;
  int NOanswer = 0;

  displayLongText(question);

  while (YESanswer == 0 && NOanswer == 0) {
    YESanswer = digitalRead(touchPin6);
    NOanswer = digitalRead(touchPin5);
  }

  int response = -1;
  
  if (YESanswer) {
    response = 1;
  }
  if (NOanswer) {
    response = 0;
  }

  return response;
}


void handleQuestions(int secondsPassed) {
  if (secondsPassed % 30 != 0) return;
  if (!enableQuestion) return;

  char* textQuestion = questions[questionsIndex][0];
  char* answerDomain = questions[questionsIndex][1];

  int answer = requestAnswer(textQuestion);
  int expectedAnswer = 1;

  /* Domain
    0 -> temperature
    1 -> soil moisture
    2 -> luminosity
    3 -> light resistance
  */
  int answerDomainInt = atoi(answerDomain);

  if (answerDomainInt == 0) {
    float temperatureCelsius = getTemperature();
    if (temperatureCelsius < MIN_TEMPERATURE || temperatureCelsius > MAX_TEMPERATURE) {
      expectedAnswer = 1;
    } else {
      expectedAnswer = 0;
    }
  } else if (answerDomainInt == 1) {
    float soilMoisture = getSoilMoisture();
    if (soilMoisture < MIN_SOIL_MOISTURE) {
      expectedAnswer = 1;
    } else {
      expectedAnswer = 0;
    }
  } else if (answerDomainInt == 2) {
    float light = getLight();
    if (light < MIN_LIGHT) {
      expectedAnswer = 1;
    } else {
      expectedAnswer = 0;
    }
  } else if (answerDomainInt == 3) {
    float lightResistance = getLightResistance();
    if (lightResistance < MIN_LIGHT_RESISTANCE || lightResistance > MAX_LIGHT_RESISTANCE) {
      expectedAnswer = 1;
    } else {
      expectedAnswer = 0;
    }
  } else {
    expectedAnswer = 1;
  }
  
  if (expectedAnswer == answer) {
    displayLongText(goodAnswer);
  } else {
    displayLongText(badAnswer);
  }

  questionsIndex = (questionsIndex + 1) % nbQuestions;
}

// Main loop.
int secondsPassed = 0;
void loop()
{
  float temperatureCelsius = getTemperature();
  float moisturePercentage = getSoilMoisture();
  float light = getLight();
  float lightResistance = getLightResistance();

  
  handleAlerts(temperatureCelsius, moisturePercentage, light, lightResistance);  
  handleQuestions(secondsPassed);

  // Serial.print("\ntemperature (in °C) = ");
  // Serial.println(temperatureCelsius);

  // Serial.print("Moisture (in percentage) = " );
  // Serial.println(moisturePercentage);

  // Serial.print("Light = " );
  // Serial.println(light);

  // Serial.print("Light Resistance = " );
  // Serial.println(lightResistance);

  delay(1000);
  secondsPassed++;
}