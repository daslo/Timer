const uint8_t COUNT=8;
const uint32_t CODE=99999999;
/*
 * ARDUINO ----------- TM1638
 * GND     ----------- GND
 * 3.3V    ----------- VCC
 * 12      ----------- STB
 * 11      ----------- CLK
 * 10      ----------- DIO
 * 
 * ARDUINO ----------- ENCODER
 * GND     ----------- GND
 * 5V      ----------- +
 * 3       ----------- CLK
 * 4       ----------- DT
 * 5       ----------- SW
 */
const uint8_t DIO=10;
const uint8_t CLK1=11;
const uint8_t STB=12; //do TM1368

const uint8_t CLK2=3;
const uint8_t DT=4;
const uint8_t SW=5; //do encodera

//TRYBY PRACY
const uint8_t M_null=0; //nic
const uint8_t M_select=1; //wybór segmentu
const uint8_t M_set=2; //edycja segmantu
const uint8_t M_count=3; //odliczanie
const uint8_t M_end=4; //koniec odliczania
uint8_t MODE=M_null; //tryb

//STEROWANIE
const uint8_t S_null=0; //nic
const uint8_t S_right=1; //obrót w prawo
const uint8_t S_left=2; //obrót w lewo
const uint8_t S_long=3; //długie naciśnięcie
const uint8_t S_short=4; //krótkie naciśnięcie
uint8_t STEER=0; //sterowanie

const uint16_t TH_long=250; //próg długiego naciśnięcia
const uint16_t TH_flash=100; //próg migania

uint8_t lSegment=0; //edytowany/wybrany segment (numeracja o PRAWEJ DO LEWEJ)
//uint8_t tSegment[8]={0,0,0,0,0,0,0,0}; //wartości na segmentach (numeracja o PRAWEJ DO LEWEJ)
uint32_t lTime=0;
/*
 * potrzebne są wartości progów, przy których wartość czasu zostanie zmniejszona o 1
 * 
 * wg specyfikacji program ma dawać możliwość ustawienia szybkości odliczania i mają istnieć 2 skrajne stany
 * nie jest napisane, że szybkość ma być nastawiana w sposób ciągły, dlatego można wybrać arbitralną liczbę wartości szybkości
 * 
 * szybkość przyrastania licznika countdown istotnie zależy od długości pętli loop() i od COUNT
 * 
 * dlatego (moim zdaniem) zamiast pisać funkcję, która obliczy potrzebne wartości, uwzględniając powyższą uwagę
 * lepiej stworzyć tabelę z gotowymi wartościami zależnymi od COUNT i lSpeed
 * (w sensie matematycznym ta tabela też jest funkcją xD)
 * 
 * ^ TH_countdown
 * |
 * *1[s]
 * |   .
 * |        .
 * |             .
 * |                   .               
 * |                        .          
 * |                             .    
 * |                                  *60[s]/(10^COUNT - 1)
 * +----+----+----+----+----+----+----+-----> lSpeed
 * 0    1    2    3    4    5    6    7
 */
uint8_t lSpeed=0;
const uint8_t Speed_max=2;
const uint8_t Speed_min=0;

/*
 * dodatnia wartość oznacza próg, przy którym czas zostanie pomniejszony o 1
 * dla dużych COUNT nawet próg=1 nie zapewniał, że licznik przejdzie od 999...9 -> 0 w 60sek
 * dlatego wtedy z każdym wykonaniem loop() kilka razy wykonywana jest pętla countDown()
 * wartość ujemna -n oznacza, że countDown() ma być wykonane n razy w ciągu loop() (próg=1)
 * 
 * t = czas wykonywania jednej pętli loop():
 * t * SEK1 = 1s
 * 
 * dojście z wartości maksymalnej do 0 w 60s:
 * (1e(COUNT)-1) * t * K = 60s
 * K=60s / t*(1e(COUNT)-1)
 * 
 * 
 */

#define SEK1 356
const int16_t TH_count[8][Speed_max-Speed_min+1]={
  2372,1000, SEK1,    //COUNT=1, COUNT-1=0
  SEK1,300, 216,    //COUNT=2, COUNT-1=1
  SEK1,150, 21,
  SEK1,50,  2,
  SEK1,40,  -5,
  SEK1,10,  -55,
  SEK1,-100, -550,
  SEK1,-2000, -5500
};
#undef SEK1
/*
uint32_t E(uint8_t n){
  uint32_t ret=1;
  for(int i=0; i<n; i++) ret*=10; 
  return ret;
}
 */
uint32_t E[9]={ 
  1,
  10,
  100,
  1000,
  10000,
  100000,
  1000000,
  10000000,
  100000000};

//cyfry zapisane kodem abcdefg[dp]
const uint8_t DIGITS[11]={0b11111100, 0b01100000, 0b11011010, 0b11110010, 0b01100110, 0b10110110, 0b10111110, 0b11100000, 0b11111110, 0b11110110, 0b00000000};

uint16_t i=0, j=0;
uint16_t pulseLen=0, flashLen=0, countLen=0;//liczniki: długości naciśnięcia, migania, odliczania

bool flash=false; //miganie segmentami, true=swiecenie, false=wygaszenie
bool n = LOW; //CLK2
bool m = HIGH; //SW
bool n_last = LOW;
bool m_last = HIGH;

//adresy wyświetlaczy
const uint8_t addresses[8]={0xce, 0xcc, 0xca, 0xc8, 0xc6, 0xc4, 0xc2, 0xc0};
uint8_t digit(uint8_t n){
  //jaka cyfra na wyswietlaczu (numeracja OD PRAWEJ DO LEWEJ)
  if (n>=COUNT) return 10;
  else return (lTime%E[n+1])/E[n];
}
void writeOut(){

  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK1, LSBFIRST, 0x44);// set single address
  digitalWrite(STB, HIGH);
  /*
   * można dać pętlę for(i=0; i<COUNT; i++), ale wtedy czas wykonywania się zależy od COUNT -> miganie i liczenie odbywa się z różną częstotliwościa
   * aby to ominąć wymuszamy jednakowy czas wykonywania pętli
   */
  for(int i=0; i<8;i++){
    digitalWrite(STB, LOW);
    shiftOut(DIO, CLK1, LSBFIRST, addresses[i]);
    /*
     * JEŻELI
     * flash==1 -> zmienna do migania
     * ORAZ
     * w tym trybie miganie jest
     * ORAZ
     *    odliczanie się skończyło
     *    LUB
     *    wybrany segment jest edytowany
     * TO
     * wygaś dany 7seg
     * 
     */
    if((((MODE==M_set || MODE==M_select) && i==lSegment) || MODE==M_end) && flash) shiftOut(DIO, CLK1, MSBFIRST, DIGITS[10]);
    
    else shiftOut(DIO, CLK1, MSBFIRST, DIGITS[digit(i)]);
    
    digitalWrite(STB, HIGH);//koniec przesyłania komendy
  }
}
void setup() {
  //Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(CLK1, OUTPUT);
  pinMode(STB, OUTPUT);
  pinMode(DIO, OUTPUT); //ustaw piny TM1638 jako wyjścia
  pinMode(CLK2, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT); //ustaw piny encodera jako wejścia

  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK1, LSBFIRST, 0x8f);// activate and set brightness to max
  digitalWrite(STB, HIGH);
  
  digitalWrite(STB, LOW);
  shiftOut(DIO, CLK1, LSBFIRST, 0x40);// set auto increment mode
  digitalWrite(STB, HIGH);
  /*
  for(i=0; i<COUNT; i++) tSegment[i]=0; 
  for(i=COUNT; i<8; i++) tSegment[i]=10; 
  */
  lTime=CODE;
  //for(int i=0; i<COUNT;i++) tSegment[i]=CODE[COUNT-1-i]-'0'; //wpisz CODE do Segmentów, które będą używane
  //for(int i=COUNT; i<8; i++) tSegment[i]=10;//i NULL do tych, które nie będą
  
  writeOut();
  STEER=S_null;
  MODE=M_null; //zeruj tryb i sterowanie
}

void steer(){
  //ustaw zmienną STEER na podstawie sygnałów z encodera
    n = digitalRead(CLK2);
    m = digitalRead(SW); //odczytaj wejścia z encodera, będą porównywane z poprzednimi wartościami
    /*
     *              v zbocze narast.  v
     * CLK2 _________-------__________-----------
     * DT   _________________________----------
     *             obr. w prawo      obr. w lewo
     * 
     */
    if ((n_last == LOW) && (n == HIGH)) {//zbocze narastające na CLK2
      if (digitalRead(DT) == LOW) STEER=S_right; //obrót w prawo
      else STEER=S_left;//obrót w lewo
    }
    /*
     *SW(t)  -----------__________--------------___________ m
     *SW(t-1)------------__________--------------__________ m_last
     *                    pressd  ^
     */
    if(m_last ==LOW && m==LOW){ //przycisk był wciśnięty i dalej jest wciśnięty
      if(pulseLen<TH_long) pulseLen++;
      //zliczaj dopóki przycisk jest wciśnięty aż do osiągnięcia TRESHOLD
      //dalsze zliczanie jest niepotrzebne, a mogłoby przepełnić int-a
    }
    if(m_last == LOW && m==HIGH){ //przycisk był wciśnięty, a teraz nie
       if(pulseLen==TH_long) STEER=S_long;//było długie przycisnięcie
      else STEER=S_short;
      //Serial.println(pulseLen);//krótkie
      pulseLen=0; //wyzeruj licznik wciśnięcia
    }
    n_last = n;
    m_last = m;//obecne wejścia z enkodera- będą porównane z następnymi
}
void countDown(uint32_t n){
  if(lTime>n){
    lTime-=n;
  }
  else{
    lTime=0;
    MODE=M_end;
  }
}
void exec(){
  /*
     * Reakcja na sygnał dla poszczególnych trybów
     *            obrót        długie prz.      krótkie prz.
     * MODE     S_right/S_left     S_long          S_short
     * ------------------------------------------------
     * M_null         ---        ->M_select      ->M_count
     * M_select   SEGMENT+-=1    ->M_null        ->M_set
     * M_set      WARTOSC+-=1    ->M_null        ->M_select
     * M_count    SPEED+-=1        ---          ->M_null
     * 
     * ->...  zmiana trybu
     * ...+-= zmiana wartości
     */
  switch(MODE){
      case M_null:
        if(STEER==S_long) MODE=M_select; //zmiana trybu, wg tabeli wyżej
        if(STEER==S_short) MODE=M_count;
        break;
      case M_select:
        if(STEER==S_long) MODE=M_null;
        if(STEER==S_short) MODE=M_set;
        if(STEER==S_left) {
          lSegment==COUNT-1 ? lSegment=0 : lSegment++;
        } 
        /* Obrót pokrętła w lewo spowoduje zmianę lSegment na wyższy--segmenty są numerowane od PRAWEJ DO LEWEJ
         * wyrażenie warunkowe zapewnia, że końce przedziału [0; COUNT-1] są sklejone-> ...,8,1,2,...,8,1,...
         */
        if(STEER==S_right) { 
          lSegment==0 ? lSegment=COUNT-1 : lSegment--;
        } 
        break;
      case M_set:
        if(STEER==S_long) MODE=M_null;
        if(STEER==S_short) MODE=M_select;
        if(STEER==S_right) {
          //jeżeli dana pozycja nie jest graniczną (0 lub 9) to zwiększ/zmniejsz lTime
          if(!(digit(lSegment)==9))  lTime+=E[lSegment];
        }
        if(STEER==S_left) {
          if(!(digit(lSegment)==0))  lTime-=E[lSegment];
        }
        break;
      case M_count:
        if(STEER==S_short) MODE=M_null;
        if(STEER==S_right) {
          if(!(lSpeed==Speed_max)) lSpeed+=1;
          countLen=0;
        }
        
        if(STEER==S_left) {
          if(!(lSpeed==Speed_min)) lSpeed-=1;
          countLen=0;
        }
        //Serial.println(lSpeed);
        break;
      case M_end:
        if(STEER==S_short) {
        setup();
        }
      default:
        break;
    }
  
}
void loop() {
  steer(); //ustaw STEER
  exec(); //zareaguj na STEER
  writeOut(); //wypisz wartości na wyświetlacz

  if(MODE==M_count){ //jeżeli tryb odliczania, to odliczaj
  countLen++;
    if(TH_count[COUNT-1][lSpeed]>0){
      if(countLen==TH_count[COUNT-1][lSpeed]){//dodatnia wartość oznacza próg
        countDown(1);
        countLen=0;
      }
    }
    if(TH_count[COUNT-1][lSpeed]<0){//ujemna wartość oznacza mnożnik
      countDown(0-TH_count[COUNT-1][lSpeed]);
      countLen=0;
    }

  }
  if(MODE==M_set || MODE==M_select || MODE==M_end){ //miganie w trybie wyboru, edycji i po skończeniu odliczania
    flashLen++;
    if(flashLen==TH_flash){
      flashLen=0;
      flash=!flash;
    }
  }

  STEER=S_null;
}
