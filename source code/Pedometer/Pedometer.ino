#include <TimeLib.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include "OLEDDisplayUi.h"
#include "images.h"
#include <SimpleTimer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WidgetRTC.h>

#define POWER_KEY 1
#define MENU_KEY  2
#define UPLOAD_KEY 3

boolean upload = false;

SSD1306  display(0x3c, 18, 0);

OLEDDisplayUi ui ( &display );

SimpleTimer timer;
WidgetRTC rtc;

int screenW = 128;
int screenH = 64;
int clockCenterX = screenW/2;
int clockCenterY = ((screenH-16)/2)+16;   // top yellow part is 16 px height
int clockRadius = 23;

#define DEVICE (0x53)      //ADXL345 device address
#define TO_READ (6)        //num of bytes we are going to read each time (two bytes for each axis)

byte buff[TO_READ] ;        //6 bytes buffer for saving data read from the device
char str[100];              //string buffer to transform data before sending it to the serial port
int regAddress = 0x32;      //first axis-acceleration-data register on the ADXL345
int xx, yy, zz;                //three axis acceleration data

static int currentValue = 0;
static unsigned long stepsSum=0;

char auth[] = "YourAuthToken";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "YourNetworkName";
char pass[] = "YourPassword";

const char running_Logo_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x64,0x03,0x00,0x00,0x00,0xF8,0x01,0x00,0x00,0x00,0xF8,0x01,0x00,0x00,0x00,0xFC,
  0x01,0x00,0x00,0x00,0xFC,0x05,0x00,0x00,0x00,0xFC,0x01,0x00,0x00,0x00,0xFC,0x00,
  0x00,0x00,0x00,0xF8,0x01,0x00,0x00,0x00,0xF8,0x01,0x00,0x00,0x00,0xE0,0x03,0x00,
  0x00,0x60,0xF1,0x07,0x00,0x00,0x20,0xF8,0x17,0x00,0x00,0xC0,0xF8,0x0F,0x00,0x00,
  0xE0,0xFB,0x17,0x00,0x00,0xC0,0xFF,0x13,0x00,0x00,0x00,0xFF,0x03,0x00,0x00,0x80,
  0xFE,0x03,0x00,0x00,0x00,0xF9,0x03,0x00,0x00,0x00,0xFA,0x03,0x00,0x00,0x00,0xF8,
  0x03,0x00,0x00,0x00,0xF0,0x07,0x00,0x00,0x00,0xF4,0x07,0x00,0x00,0x00,0xF4,0x0F,
  0x00,0x00,0x00,0xF9,0x0F,0x00,0x00,0x00,0xFC,0x1F,0x00,0x00,0x80,0xFE,0x1F,0x00,
  0x00,0x00,0xFF,0x1F,0x00,0x00,0xA0,0xFF,0x5F,0x00,0x00,0xC0,0x3F,0x3F,0x00,0x00,
  0xE8,0x1F,0x3F,0x00,0x00,0xE8,0xA7,0x3E,0x00,0x00,0xF0,0x03,0x7C,0x00,0x00,0xE0,
  0x05,0x7C,0x00,0x00,0xE0,0x05,0xF8,0x01,0x00,0xC0,0x01,0xF0,0x03,0x00,0xC0,0x03,
  0xE8,0x07,0x00,0xC0,0x03,0x88,0x6F,0x00,0x80,0x03,0x40,0x1E,0x00,0xA0,0x03,0x40,
  0xFC,0x00,0x80,0x03,0x00,0xF8,0x01,0x00,0x07,0x00,0xF4,0x00,0x00,0x07,0x00,0xE8,
  0x00,0x80,0x0F,0x00,0xE8,0x00,0x90,0x0F,0x00,0xE0,0x00,0xE8,0x0F,0x00,0xE8,0x00,
  0xF0,0x09,0x00,0x60,0x01,0xF0,0x04,0x00,0x00,0x00,
};

// utility function for digital clock display: prints leading 0
String twoDigits(int digits){
  if(digits < 10) {
    String i = '0'+String(digits);
    return i;
  }
  else {
    return String(digits);
  }
}

void clockOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  if((hour()==0) && (minute()==0) && (second()==0))
    stepsSum = 0;
}

void analogClockFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->drawCircle(clockCenterX + x, clockCenterY + y, 2);

  //hour ticks
  for( int z=0; z < 360;z= z + 30 ){
    float angle = z ;
    angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
    int x2 = ( clockCenterX + ( sin(angle) * clockRadius ) );
    int y2 = ( clockCenterY - ( cos(angle) * clockRadius ) );
    int x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 8 ) ) ) );
    int y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 8 ) ) ) );
    display->drawLine( x2 + x , y2 + y , x3 + x , y3 + y);
  }

  // display second hand
  float angle = second() * 6 ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  int x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 5 ) ) ) );
  int y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 5 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
  
  // display minute hand
  angle = minute() * 6 ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 4 ) ) ) );
  y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 4 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);

  // display hour hand
  angle = hour() * 30 + int( ( minute() / 12 ) * 6 )   ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 2 ) ) ) );
  y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 2 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
}

void digitalClockFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  String date = String(year())+"/"+twoDigits(month())+"/"+twoDigits(day());
  String timenow = String(hour())+":"+twoDigits(minute())+":"+twoDigits(second());
  
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(clockCenterX + x , 20, timenow);
  display->setFont(ArialMT_Plain_16);
  display->drawString(60 , 45, date);
}

void writeTo(int device, byte address, byte val) {
  Wire.beginTransmission(device); //start transmission to device 
  Wire.write(address);        // send register address
  Wire.write(val);        // send value to write
  Wire.endTransmission(); //end transmission
}

//reads num bytes starting from address register on device in to buff array
void readFrom(int device, byte address, int num, byte buff[]) {
  Wire.beginTransmission(device); //start transmission to device 
  Wire.write(address);        //sends address to read from
  Wire.endTransmission(); //end transmission

  Wire.beginTransmission(device); //start transmission to device
  Wire.requestFrom(device, num);    // request 6 bytes from device

  int i = 0;
  while(Wire.available())    //device may send less than requested (abnormal)
  { 
    buff[i] = Wire.read(); // receive a byte
    i++;
  }
  Wire.endTransmission(); //end transmission
}

void runningFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  float calValue = stepsSum*0.4487;
  
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(clockCenterX , clockCenterY, str);

  sprintf(str,"%.2fcal",calValue);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(100 , 20, str);
  
  display->drawXbm(10, 14, 34, 50, running_Logo_bits);
}

void uploadFrame(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_16);
  display->drawString(60 , 45, "upload data ...");
}

// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { analogClockFrame, digitalClockFrame, runningFrame, uploadFrame};

// how many frames are there?
int frameCount = 4;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { clockOverlay };
int overlaysCount = 1;

void uploadToBlynk(void){
  if(upload == true){
    Blynk.virtualWrite(V0,stepsSum);
    Blynk.virtualWrite(V1,stepsSum);
  }
}

void uiInit(void){
  ui.setTargetFPS(30);
  //ui.setActiveSymbol(activeSymbol);
  //ui.setInactiveSymbol(inactiveSymbol);
  ui.setIndicatorPosition(TOP);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.disableAutoTransition();
  ui.switchToFrame(2);
  ui.init();
  display.flipScreenVertically();
}

void adxl345Init(void){
  writeTo(DEVICE, 0x2D, 0);      
  writeTo(DEVICE, 0x2D, 16);
  writeTo(DEVICE, 0x2D, 8);
}

void updateAdxl345(void){
  readFrom(DEVICE, regAddress, TO_READ, buff); //read the acceleration data from the ADXL345
  xx = (((int)buff[1]) << 8) | buff[0];   
  yy = (((int)buff[3])<< 8) | buff[2];
  zz = (((int)buff[5]) << 8) | buff[4];
  
  if(xx < 100){
    sprintf(str, "%d", stepsSum);  
    return;
  }

  if(fabs(xx - currentValue) > 80){
    if(xx < currentValue){
      stepsSum++;
    }
    currentValue = xx;
  }
  sprintf(str, "%d", stepsSum);  
}

int getKeys(void){
    if(digitalRead(D2) == LOW){
      delay(5);
      if(digitalRead(D2) == LOW){
        while(digitalRead(D2) == LOW);
        return POWER_KEY;
      }
    }
     if(digitalRead(D3) == LOW){
      delay(5);
      if(digitalRead(D3) == LOW){
        while(digitalRead(D3) == LOW);
        return MENU_KEY;
      }
    }
     if(digitalRead(D4) == LOW){
      delay(5);
      if(digitalRead(D4) == LOW){
        while(digitalRead(D4) == LOW);
        return UPLOAD_KEY;
      }
    }
    return 0;
}

void doKeysFunction(void){
  static int uiFrameIndex = 2;
  int keys = getKeys();
  if(keys == POWER_KEY){
    static char i = 0;
    if(i){
      ui.init();
     display.flipScreenVertically();
     display.displayOn();
    }else{
     display.displayOff();
    }
    i = ~i;
  }
  if(keys == MENU_KEY){
    if(upload == false){
      uiFrameIndex++;
      if(uiFrameIndex == 3)
       uiFrameIndex = 0;     
      ui.switchToFrame(uiFrameIndex);
    }else{
      ui.switchToFrame(3);
    }
  }
  if(keys == UPLOAD_KEY){
    if(upload == true){
      upload = false;
      ui.switchToFrame(uiFrameIndex);
    }else{
      upload = true;
      ui.switchToFrame(3);
    }
  }
}

void setup() {
  pinMode(D2,INPUT);
  pinMode(D3,INPUT);
  pinMode(D4,INPUT);
  Blynk.begin(auth, ssid, pass);
  rtc.begin();
  uiInit();
  adxl345Init();

  timer.setInterval(30,updateAdxl345);
  timer.setInterval(100,uploadToBlynk);
}

void loop() {
  int remainingTimeBudget = ui.update();
  static int testSum = 0;
  if((testSum < 100) || (upload == true)){
   Blynk.run();
   testSum++;
  }
  if (remainingTimeBudget > 0) {
    delay(remainingTimeBudget);
  }
  doKeysFunction();
  timer.run();
}
