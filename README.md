# pedometer-ESP32
This is a smart pedometer controlled by Firebeetle Board-ESP32


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

