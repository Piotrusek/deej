bool dev=0;
const int outputs = 2; //number of outputs to cycle trough when with autput controll
int sel_out = 0;

const int NUM_SLIDERS = 6;   //ilośc wirtualnych faderów
const int NUM_PHYSICAL_SLIDERS=2;       //number of physical faders
const int analogInputs[NUM_PHYSICAL_SLIDERS] = {A1, A2};   //adresses of physical faders

const int clk[NUM_PHYSICAL_SLIDERS] = {2,5};   //clk for encoders
const int dt[NUM_PHYSICAL_SLIDERS] = {3,6};   //dt pins for encoders
const int sw[NUM_PHYSICAL_SLIDERS] = {4,7};  //sw pins for encoders
const int mutebut[NUM_PHYSICAL_SLIDERS] = {8,9}; //mute buttons pins
const int mute_led[NUM_PHYSICAL_SLIDERS] = {10,11}; //pins of mute led's
const int output_controll_pin = 12;   //pin which button for controlling output is connected to

const int lcd_kolumny = 16;
const int lcd_wiersze = 2;

String channel_list[NUM_SLIDERS] = {"Master        ","Discord       ","Muzyka        ","Gry           ","Opera         ","System        "}; //make sure all sources have lcd_kolumny-2 znaków


#include <Encoder.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

byte lock_ico[] = {    //lock ico
  B01110,
  B10001,
  B10001,
  B11111,
  B11011,
  B11011,
  B11111,
  B01110
};

byte mute_ico[] = {   //mute ico
  B10000,
  B11000,
  B11100,
  B11111,
  B11111,
  B11100,
  B11000,
  B10000
};

byte chan_eq_ico[] = {    //equalization ico
  B00000,
  B10000,
  B01000,
  B11111,
  B00100,
  B11111,
  B00010,
  B00001
};

byte up_down_arrows_ico[] = {   //change ico
  B00100,
  B01010,
  B10001,
  B00000,
  B00000,
  B10001,
  B01010,
  B00100
};

bool migState = 0;      //variable responsible for blinking LED/screen 
unsigned long previousMillisblink = 0;  //zapisywanie czasu ostatniego mignięcia
const long blink_interval = 500;    //czas migania ikon ekranu/diod
const long reset_interval = 5000;   //czas po którym maja się wyzerować encodery po bezczynności
const long cooldown = 500;      //minimalny czas między kliknięciami
unsigned long currentMillis = millis();     //aktualny czas
unsigned long settimemillis = millis();     //zapisywanie czasu ostatniej aktywności
unsigned long previousclick = millis();     //zapisywanie czasu ostatniego kliknięcia jakiegokoliwke przycisku

LiquidCrystal_I2C lcd(0x27,lcd_kolumny,lcd_wiersze);     //definiwoanie ekranu (adres, kolumny, rzędy)

int analogSliderValues[NUM_SLIDERS];    //główna zmienna któa jest wysyłana cały czas i zapisuje wartości sliderów (wirtualnych)
bool SliderMutes[NUM_SLIDERS] = {0, 0, 0, 0, 0, 0};     //zmienna mówiąca czy slidery są wyciszone czy nie

int slider_sel[NUM_PHYSICAL_SLIDERS] = {0, 1};      //które wirtualne slidery są aktualnie wybrane do kontroli
int slider_new[NUM_PHYSICAL_SLIDERS] = {0, 1};      //zmienna z wartością nowowybranego slidera
bool zmiana_sel[NUM_PHYSICAL_SLIDERS] = {1, 1};     //zmienna mówiąca o tym czy nastąpiła jakaś zmiana
bool slider_lock[NUM_PHYSICAL_SLIDERS] = {0, 0};    //czy kanał fizyczny enkoder jest zablokowany

long oldPosition[NUM_PHYSICAL_SLIDERS]  = {-999,-999};  //enkoderowe zapisywanie porzedniego miejsca
long newPosition[NUM_PHYSICAL_SLIDERS];     //enkoderowe zapisywanie aktualnego miejsca

//definiwoanie enkoderów
Encoder Enc_1(clk[0], dt[0]);     
Encoder Enc_2(clk[1], dt[1]);


//definiowanie przyciksków (wartości czy jest kliknięty)
int em_stop = 0;
int vsw[NUM_PHYSICAL_SLIDERS] = {0,0};
int vbut[NUM_PHYSICAL_SLIDERS] = {0,0};
int vout = 0;

void setup() {    //do przerobienia
  //screen init:
  lcd.init();
  lcd.createChar(0, lock_ico);
  lcd.createChar(1, chan_eq_ico);
  lcd.createChar(2, up_down_arrows_ico);
  lcd.createChar(3, mute_ico);
  lcd.backlight();
  
  lcd.setCursor(5,0);
  lcd.print("MixPad");


  for (int i = 0; i < NUM_PHYSICAL_SLIDERS; i++) {  //asign fizycznych sliderów do portów A1, A2, ...
    pinMode(analogInputs[i], INPUT);
  }
  pinMode(clk[0], INPUT);  //CLK encoder 1
  pinMode(dt[0], INPUT);  //DT encoder 1
  pinMode(sw[0], INPUT_PULLUP);  //SW encoder 1
  pinMode(clk[1], INPUT);  //CLK encoder 2
  pinMode(dt[1], INPUT);  //DT encoder 2
  pinMode(sw[1], INPUT_PULLUP); //SW encoder 2
  pinMode(mutebut[0], INPUT);  //mute button 1
  pinMode(mutebut[1], INPUT);  //mute button 2
  pinMode(mute_led[0], OUTPUT); //mute led 1
  pinMode(mute_led[1], OUTPUT); //mute led 2
  pinMode(output_controll_pin, INPUT);
  Serial.begin(9600);


  delay(2500);
  /*
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Values you see");
  lcd.setCursor(0,1);
  lcd.print("are NOT accurate");
  while(digitalRead(sw[0]) != LOW && digitalRead(sw[1]) != LOW && digitalRead(mutebut[0]) == LOW && digitalRead(mutebut[1]) == LOW) {}  //wait for anything to bo clicked
  delay(1000);
  //*/
  lcd.clear();
}
  
void loop() {
  currentMillis = millis();         //ustawianie czasu  
  updateSliderValues();             //zczytanie wartości ze sliderów fizycznych
  sendSliderValues();               // Actually send data (all the time)
  screenupdate();                   //aktualizacja ekranu
  //printSliderValues();        // For debug
  canceling_change_function();      //Jeśłi nie zatwierdzono zmiany to reset enkoderów
  button_spam();                    //Wywoałanie przycisków
  delay(10);                        //small delay so that arduino could rest a while
} 

void button_spam(){                 //ochrona przed spamowaniem
  if (currentMillis - previousclick >= cooldown){   //jeśli czas od osytatniego klikniecia za mały to żeby nie wywoływało funckji
      buttons();
  }
}

void Enc_write(int i, int x){   //writing something to enmcoder encoder czegoś (if you change amount of physical slideers you have to add new IF)
  if(i==0){
    Enc_1.write(x);
  }
  if(i==1){
    Enc_2.write(x);
  }
}

void Enc_res(){    //reset of all encoders
  Enc_write(0,0);
  Enc_write(1,0);
}

void canceling_change_function(){               //anulowanie zmiany jeśłi nie było akcji żandej przez pewien czas
  if (currentMillis - settimemillis >= reset_interval && currentMillis - settimemillis <= reset_interval+500) { 
    Enc_res();
  }
}

void screen_icon_loader(){          //sprawdzanie któe ikonki mają się wyświetlać
  if (currentMillis - previousMillisblink >= blink_interval) { //led blinking 
    previousMillisblink = currentMillis;    //czas od ostatniego mignięcia
    if (migState == 0) {      //zmiana trybu między mignięciami
      migState = 1;
    } else {
      migState = 0;
    }
  }
  for(int i=0;i<NUM_PHYSICAL_SLIDERS;i++){
    lcd.setCursor(lcd_kolumny-2,i);
    if(zmiana_sel[i]==1){
      if(migState==1){
        lcd.write(byte(1));
      }else{
        lcd.print(" ");
      }
    }else{
      lcd.print(" ");
    }
    lcd.setCursor(lcd_kolumny-1,i);
    if(slider_lock[i]==1){
      lcd.write(byte(0));
    }else{
      if(slider_sel[i]!=slider_new[i]){
        lcd.write(byte(2));
      }else{
        lcd.print(" ");
      }
    }
    if(SliderMutes[slider_sel[i]]==1){
      lcd.setCursor(lcd_kolumny-3,i);
      lcd.write(byte(3));
    }
  }
}

void  updateEncod(){              //aktualizacja enkoderów
  newPosition[0] = Enc_1.read();
  newPosition[1] = Enc_2.read();
  for(int i=0;i<NUM_PHYSICAL_SLIDERS;i++){
    if(slider_lock[i]==1){
    Enc_write(i,0);
    }else{
      if(newPosition[i] != oldPosition[i]){
        settimemillis=millis();
        oldPosition[i] = newPosition[i];
        slider_new[i]=(slider_sel[i]+(newPosition[i]/-4))%NUM_SLIDERS;
        if(slider_new[i]<0){
          slider_new[i]+=NUM_SLIDERS;
        }
      }
    } 
  }

}

void buttons(){                   //aktualizacja przycisków
  vsw[0] = digitalRead(sw[0]);
  vsw[1] = digitalRead(sw[1]);
  vbut[0] = digitalRead(mutebut[0]);
  vbut[1] = digitalRead(mutebut[1]);
  vout = digitalRead(output_controll_pin);
  updateEncod();
  if(vout==HIGH){
    sel_out++;
    if(sel_out>=outputs){
      sel_out=0;
    }
  }
  if(vsw[0]==LOW){
    if(slider_sel[0]==slider_new[0]){
      if(slider_lock[0]==0){
        slider_lock[0]=1;
      }else{
        slider_lock[0]=0;
      }
    }else{
      if(slider_new[0]==slider_sel[1]){
        Enc_res();
        lcd.setCursor(0,0);
        lcd.print("                ");
        lcd.setCursor(5,0);
        lcd.print("ERROR");
        delay(1000);
      }else{
        slider_sel[0]=slider_new[0];
        zmiana_sel[0]=1;
        Enc_res();
      }
    }
    previousclick=millis();
  }
  if(vsw[1]==LOW){
    if(slider_sel[1]==slider_new[1]){
      if(slider_lock[1]==0){
        slider_lock[1]=1;
      }else{
        slider_lock[1]=0;
      }
    }else{
      if(slider_new[1]==slider_sel[0]){
        Enc_res();
        lcd.setCursor(0,1);
        lcd.print("                ");
        lcd.setCursor(5,1);
        lcd.print("ERROR");
        delay(1000);
      }else{
        slider_sel[1]=slider_new[1];
        zmiana_sel[1]=1;
        Enc_res();
      }
    }
    previousclick=millis();
  }
  if(vbut[0]==HIGH){
    if(SliderMutes[slider_sel[0]]==0){
      SliderMutes[slider_sel[0]]=1;
    }else{
      SliderMutes[slider_sel[0]]=0;
    }
    previousclick=millis();
  }
  if(vbut[1]==HIGH){
    if(SliderMutes[slider_sel[1]]==0){
      SliderMutes[slider_sel[1]]=1;
    }else{
      SliderMutes[slider_sel[1]]=0;
    }
    previousclick=millis();
  }
}

void screenupdate(){              //wypisanie na ekran
  for(int i=0;i<NUM_PHYSICAL_SLIDERS;i++){
    lcd.setCursor(0,i);
    lcd.print(channel_list[slider_new[i]]);
  }
  
  for(int i=0;i<NUM_PHYSICAL_SLIDERS;i++){          //wypisanie aktualnej głośności
    int v1 = map(analogSliderValues[slider_sel[i]], 10, 1020, 0, 100);
    lcd.setCursor(lcd_kolumny-5,i);
    if(SliderMutes[slider_sel[i]]==0){
      if(v1==100){
        lcd.print("100");
      }else{
        if(v1<10){
          lcd.print("  ");
          lcd.print(v1);
        }else{
            lcd.print(" ");
            lcd.print(v1);
        }
      }
    }else{
      lcd.print("  ");
    }
  }
  screen_icon_loader();
}

int can_unmute(int i){            //sprawdzanie czy można zwócić kontrolę danemu suwakowi
  if(analogSliderValues[slider_sel[i]]-30<analogRead(analogInputs[i])&&analogSliderValues[slider_sel[i]]+30>analogRead(analogInputs[i])){
    return 1;
  }else{
    return 0;
  }
}

void updateSliderValues() {  //update    suwaków
//*
  for(int s=0;s<NUM_PHYSICAL_SLIDERS;s++){
    if(SliderMutes[slider_sel[s]]==0){
      digitalWrite(mute_led[s], LOW);
      if(zmiana_sel[s]==0){
        analogSliderValues[slider_sel[s]] = analogRead(analogInputs[s]);
      }else{
        if(analogSliderValues[slider_sel[s]]-30<analogRead(analogInputs[s])&&analogSliderValues[slider_sel[s]]+30>analogRead(analogInputs[s])==1){
          zmiana_sel[s]=0;
        }
      }
    }else{
      analogSliderValues[slider_sel[s]] = 0;
      digitalWrite(mute_led[s], HIGH);
    }
  }
}

void sendSliderValues() {   //sending results to go part of deej
  String builtString = String("");
  for(int i=0; i<outputs;i++){
    if(i==sel_out){
      builtString += String((int)analogSliderValues[0]);
    }else{
      builtString+=String((int)0);
    }
    builtString += String("|");
  }
  for (int i = 1; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }
  
  Serial.println(builtString);
}

void printSliderValues() {      //drukowanie co jest (do debugowania)
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": Muted?:")+ String(SliderMutes[i]) + String(": ") + String(analogSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
  delay(100);
}

