//-----------------------------------------------
// CMU800MIDI for Arduino
//    by Oh!Ishi / Nibbles lab.
//
// based on CMU-800 MIDI化KIT AVR Version by RJB
//      and CMU800MIDI for LUFA by morecat_lab
//
// Device   : Arduino UNO
// X'tal    : 16MHz
// Version  :  1.0     Jan. 11 2008 
//             2.2     Aug. 10 2011  (base version by RJB)
//             0.5     Apr. 23, 2012 : LUFA version
//             1.0     Apr. XX, 2019 : Arduino version
//-----------------------------------------------
// include

#include <MsTimer2.h>
#include <MIDI.h>

//-----------------------------------------------
// define

#define A8253CTL1 0x03
#define A8253CTL2 0x07
#define A8255PA   0x08
#define A8255PB   0x09
#define A8255PC   0x0A
#define A8255CTL  0x0B

// PIN assign
#define WR_DISABLE  digitalWrite(12, HIGH)
#define WR_ENABLE digitalWrite(12, LOW)

#define ON    1
#define OFF   0
#define INIT    2
#define RHYTHMOFF 0xFF

#define MAX_CH    16
#define MAX_NOTE_CNT  8
#define BUFSIZE   16    // リングバッファサイズ

#define MAX_POLY_VOICE  4
#define OFFSET_CHORD_CH 2

// MIDIメッセージ
#define MIDI_CC_Moduration    0x01
#define MIDI_CC_DataEntry   0x06
#define MIDI_CC_Control1    0x10 // MOD SHAPE
#define MIDI_CC_Control2    0x11 // MOD RATE
#define MIDI_CC_Control3    0x12 // MOD DEPTH
#define MIDI_CC_RPN_LSB     0x64
#define MIDI_CC_RPN_MSB     0x65

#define MIDI_MM_AllSoundOff   0x78
#define MIDI_MM_ResetAllControl   0x79
#define MIDI_MM_AllNoteOff    0x7B

//-----------------------------------------------
// グローバル変数

unsigned int  CkPhase;
bool  Setflg;

char ch = 0;

unsigned char CVBuf[MAX_CH];
unsigned char PBBuf[MAX_CH];
unsigned char MWBuf[MAX_CH];
unsigned char RateBuf[MAX_CH];
unsigned char ShapeBuf[MAX_CH];
unsigned char DepthBuf[MAX_CH];
unsigned char RPNMsbBuf[MAX_CH];
unsigned char RPNLSBBuf[MAX_CH];
unsigned char DetuneBuf[MAX_CH];
unsigned char NoteCnt[MAX_CH];
unsigned char NoteBuf[MAX_CH][MAX_NOTE_CNT];
unsigned char PlayNoteBuf[MAX_CH];

// コードチャンネル用(CH 9)
unsigned char PolyNoteBuf[4];
unsigned char PolyNoteOrder[4];
unsigned char PolyNoteOrderCnt;

unsigned char powerState = OFF;
unsigned int powerStateCounter = 0;

// 分周比設定用テーブル
// Master Oscillator = 2539732Hz  A=440Hz
const unsigned int  DivBase[12] = {38830, 36650, 34593, 32652, 30819, 29089, 27456, 25915, 24461, 23088, 21792, 20569};
const unsigned char DetuneBase[12] = {35, 33, 32, 30, 28, 26, 25, 24, 22, 21, 20, 19};
const unsigned char   BendBase[13] = {148, 139, 131, 124, 117, 110, 104, 98, 93, 88, 83, 78, 74};
const unsigned char   ModBase[12] = {9, 8, 8, 7, 7, 6, 6, 6,  5, 5, 5, 5};

MIDI_CREATE_DEFAULT_INSTANCE();

//-----------------------------------------------
// CMU-800へ出力
void OutPort(unsigned char addr, unsigned char data) {
  noInterrupts();
  PORTB = (PORTB & 0xF0) | (addr & 0x0F); // address
  PORTC = (PORTC & 0xF0) | (data & 0x0F); // data low
  PORTD = (PORTD & 0x0F) | (data & 0xF0); // data high

  WR_ENABLE;
  WR_DISABLE;
  interrupts();
}

//-----------------------------------------------
// 8255初期化

void Init8255(void) {
  OutPort(A8255CTL, 0x88);
  OutPort(A8255PA, 0x80);
  OutPort(A8255PB, 0xFF);
  OutPort(A8255PC, 0xFF);
}

//-----------------------------------------------
// 8253初期化

void Init8253(void) {
  // 8253 #1 CH1-CH3
  OutPort(A8253CTL1, 0x36);
  OutPort(A8253CTL1, 0x76);
  OutPort(A8253CTL1, 0xB6);

  // 8253 #2 CH4-CH6
  OutPort(A8253CTL2, 0x36);
  OutPort(A8253CTL2, 0x76);
  OutPort(A8253CTL2, 0xB6);
}

//-----------------------------------------------
// 全GATE出力をOFF

void AllGateOff(void) {
  unsigned char ch;

  for(ch = 0; ch < 8; ch++) {
    OutPort(A8255PC, (ch << 1) + 1);
    OutPort(A8255PA, 0x80);
    
    OutPort(A8255PC, ch << 1);
    delayMicroseconds(50); // delayMicroseconds 50us
    OutPort(A8255PC, (ch << 1) + 1);
  }
}

//-----------------------------------------------
// CMU-800初期化

void initCMU800(void) {
  WR_DISABLE;
  Init8255();
  Init8253();
  AllGateOff();
}

//-----------------------------------------------
// CMU-800電源チェック

bool powerCheckOk(void) {
  char tmp;

  pinMode(14, INPUT);
  tmp = digitalRead(14);
  pinMode(14, OUTPUT);

  if (tmp == 1) {
    return true;
  } else {
    return false;
  }
}

//-----------------------------------------------
// タイマ2割り込み

void intTimer2(void) {
  
  CkPhase ++;
  Setflg = true;
  
  if (powerState == OFF) {
    if (powerCheckOk()) {
      if (powerStateCounter++ > 1000) { /* delayMicroseconds 1sec for initialize */
        initCMU800();  // CMU-800初期化
        powerState = INIT;
      }
    } else {
      powerStateCounter = 0;
    }
  } else if (powerState == INIT) {
    if (powerStateCounter++ > 3000) { /* delayMicroseconds 2sec before initialize finish */
      powerState = ON;
    }
  } else {      /* powerState == ON */
    if (! powerCheckOk()) {
      powerState = OFF;
      powerStateCounter = 0;
    }
  }

}

//-----------------------------------------------
// GATE出力

void Gate(unsigned char ch, unsigned char onoff) {
  if(ch > 0x07)
    return;

  OutPort(A8255PC, (ch << 1) + 1);

  if(onoff) {
    CVBuf[ch] &= ~0x80;
  } else {
    CVBuf[ch] |= 0x80;
  }
  OutPort(A8255PA, CVBuf[ch]);

  OutPort(A8255PC, ch << 1);
  delayMicroseconds(50); // Wait 50us
  OutPort(A8255PC, (ch << 1) + 1);
}

//-----------------------------------------------
// CV出力電圧セット

void SetCV(unsigned char ch) {
  unsigned char note;

  note = PlayNoteBuf[ch];
  if( note < 24) {
    note = 24;
  }
  note -= 24;

  while(note > 63) {
    note -= 12;
  }

  OutPort(A8255PC, (ch << 1) + 1);
  
  CVBuf[ch] = (CVBuf[ch] & 0x80) | (note & 0x3F);
  OutPort(A8255PA, CVBuf[ch]);

  OutPort(A8255PC, ch << 1);
  delayMicroseconds(50); // Wait 50us
  OutPort(A8255PC, (ch << 1) + 1);
}

//-----------------------------------------------
// DCO周波数セット

void SetDCO(unsigned char ch) {
  unsigned char note,ctn,val,ph;
  char lfo = 0;
  unsigned int m,n,div,bend;
  int mod,detune;

  if(ch > 0x05)
    return;
  note = PlayNoteBuf[ch];

  ctn = ch; 
  if( ctn > 2)
    ctn++;

  // 基本分周比
  if(note < 24)
    note = 24;
  note -= 24;
  m = note / 12;
  n = note % 12;
  div = DivBase[n] >> m;

  // デチューン量による分周比変化量
  detune = (DetuneBuf[ch] - 64) * DetuneBase[n];

  // モジュレーション周波数
  if(RateBuf[ch] < 3) {
    ph = CkPhase >> (3 - RateBuf[ch]);
  } else {
    ph = CkPhase << (RateBuf[ch] - 3); 
  }

  // モジュレーション波形
  switch (ShapeBuf[ch]) {
  case 0: // 三角波
    if (ph > 128) {
      lfo = 191 - ph;
    } else {
      lfo = ph - 64;
    }
    break;
  case 1: // ノコギリ波
    lfo = 63 - (ph >> 1);
    break;
  case 2: // 逆ノコギリ波
    lfo = (ph >> 1) - 64;
    break;
  case 3: // 矩形波  
    if(ph > 128) {
      lfo = -64;
    } else {
      lfo = 63;
    }
    break;
  }
  // モジュレーション量による分周比変化量
  mod = (MWBuf[ch] * ModBase[n] * lfo) >> (4 - DepthBuf[ch]);
  
  // ピッチベンド量を加味した最終分周比
  val = PBBuf[ch];
  if(val < 64) {
    bend = (BendBase[n] * (64 - val)) >> 1;
    div = (DivBase[n] - detune + bend + mod) >> m;
  } else {
    bend = (BendBase[n+1] * (val - 64)) >> 1;
    div = (DivBase[n] - detune - bend + mod) >> m;
  }
  
  // 分周比を8253にセット
  OutPort(ctn, (div & 0xFF)); // LSB
  OutPort(ctn, (div >> 8)); // MSB
} 

//-----------------------------------------------
// リズム音源トリガー

void RhythmTrig(unsigned char note) {
  unsigned char d;

  switch(note){
  case 35: d = 0x80; break; // ABD
  case 36: d = 0x80; break; // BD
  case 40: d = 0x40; break; // ESD
  case 38: d = 0x40; break; // SD
  case 41: d = 0x20; break; // LFT
  case 43: d = 0x20; break; // HFT
  case 45: d = 0x20; break; // LT
  case 47: d = 0x20; break; // LMT
  case 48: d = 0x10; break; // HMT
  case 50: d = 0x10; break; // HT
  case 49: d = 0x08; break; // CR1
  case 57: d = 0x08; break; // CR2
  case 46: d = 0x04; break; // OH
  case 42: d = 0x02; break; // CH
  default: d = 0x00; break;
  }
  OutPort(A8255PB, ~d);
}

//-----------------------------------------------
// ノートON処理(poly)
void PolyNoteON(byte Rch, byte note) {
  unsigned char i;
  unsigned char min = 0xff;

  // MIDI9ch以外は無視
  if(Rch != 0x08)
    return;

  // Find oldest Note
  for(i = 0; i < MAX_POLY_VOICE; i++) {
    if(PolyNoteOrder[i] < min) {
      min = PolyNoteOrder[i];
    }
  }

  for(i = 0; i < MAX_POLY_VOICE; i++) {
    if(PolyNoteOrder[i] == min)
      break;
  }

  if(min != 0) {
    Gate(i+2,OFF);  // ReTrigger
  }

  PolyNoteBuf[i] = note;
  PolyNoteOrder[i] = PolyNoteOrderCnt;
  PolyNoteOrderCnt++;

  PlayNoteBuf[i + OFFSET_CHORD_CH] = PolyNoteBuf[i];
  SetCV(i + OFFSET_CHORD_CH);
  SetDCO(i + OFFSET_CHORD_CH);
  Gate(i + OFFSET_CHORD_CH,ON);
}

//-----------------------------------------------
// ノートOFF処理(Poly)

void PolyNoteOFF(byte Rch, byte note) {
  unsigned char i;
  unsigned char flg = true;

  // MIDI9ch以外は無視
  if(Rch != 0x08)
    return;

  // バッファに登録されている場合は削除
  for(i = 0; i < MAX_POLY_VOICE; i++) {
    if(PolyNoteBuf[i] == note) {
      Gate(i + OFFSET_CHORD_CH,OFF);  // GATE OFF
      PolyNoteOrder[i] = 0;
    }
    if( PolyNoteOrder[i]) {
      flg = false;
    }
  }

  // PolyNoteOrderが全て0 
  if(flg)  {
    PolyNoteOrderCnt = 1;
  }
}

//-----------------------------------------------
// ノートON処理

void NoteON(byte Rch, byte note) {
  unsigned char i;
  unsigned char max = 0;

  // MIDI9ch以上は無視
  if(Rch > 0x07)
    return;

  // バッファに登録済のノートは無視
  for(i = 0; i < NoteCnt[Rch]; i++) {
    if(NoteBuf[Rch][i] == note) {
      return;
    }
  }

  // バッファがいっぱい？
  if(NoteCnt[Rch] == MAX_NOTE_CNT) {
    // 玉突き処理
    for(i = 0; i < (MAX_NOTE_CNT - 1); i++) {
      NoteBuf[Rch][i] = NoteBuf[Rch][i+1];
    }
    NoteBuf[Rch][MAX_NOTE_CNT - 1] = note;
  }
  else {
    NoteBuf[Rch][NoteCnt[Rch]] = note;
    NoteCnt[Rch]++;
  }

  // 最高音
  for(i = 0; i < NoteCnt[Rch]; i++) {
    if(max < NoteBuf[Rch][i]) {
      max = NoteBuf[Rch][i];
    }
  }
  PlayNoteBuf[Rch] = max;

  SetCV(Rch);
  SetDCO(Rch);
  Gate(Rch, ON);
}

//-----------------------------------------------
// ノートOFF処理

void NoteOFF(byte Rch, byte note) {
  unsigned char i;
  unsigned char max = 0;
  bool flg = false;

  // MIDI9ch以上は無視
  if(Rch > 0x07) return;

  // バッファに登録されている場合は削除
  for(i = 0; i < NoteCnt[Rch]; i++) {
    if(flg) {
      NoteBuf[Rch][i-1] = NoteBuf[Rch][i];
    }
    if(NoteBuf[Rch][i] == note) {
      flg = true;
    }
  }
  if(flg) NoteCnt[Rch]--;
  
  if(NoteCnt[Rch] == 0) {
    // バッファがカラの場合はGATEオフ
    Gate(Rch, OFF);
  } else {
    // 最高音
    for(i = 0; i < NoteCnt[Rch]; i++) {
      if(max < NoteBuf[Rch][i]) {
        max = NoteBuf[Rch][i];
      }
    }
    PlayNoteBuf[Rch] = max;
    
    SetCV(Rch);
    SetDCO(Rch);
  }
}

//-----------------------------------------------
// readコールバックハンドラー

// ノートオフ
void handleNoteOff(byte channel, byte note, byte velocity) {
  channel--;
  PolyNoteOFF(channel, note);
  NoteOFF(channel, note);
  RhythmTrig(RHYTHMOFF);
}

// ノートオン
void handleNoteOn(byte channel, byte note, byte velocity) {
  channel--;
  if(velocity == 0){
    PolyNoteOFF(channel, note);
    NoteOFF(channel, note);
    RhythmTrig(RHYTHMOFF);
  } else {
    PolyNoteON(channel, note);
    NoteON(channel, note);
    if(channel == 0x09) {
      RhythmTrig(note);
    }
  }
}

// コントロールチェンジ
void handleControlChange(byte channel, byte number, byte value) {
  channel--;
  switch(number) {
  case MIDI_CC_Moduration:
    if(channel == 8) {
      MWBuf[2] = value >> 2;
      MWBuf[3] = value >> 2;
      MWBuf[4] = value >> 2;
      MWBuf[5] = value >> 2;
    } else {
      MWBuf[channel] = value >> 2;
    }
    break;
  case MIDI_CC_DataEntry:
    if((RPNMsbBuf[channel] == 0) && (RPNLSBBuf[channel] == 1))
      DetuneBuf[channel] = value;
    break;
  case MIDI_CC_Control1:
    ShapeBuf[channel] = value >> 5;
    break;
  case MIDI_CC_Control2:
    RateBuf[channel] = value >> 4;
    break;
  case MIDI_CC_Control3:
    DepthBuf[channel] = value >> 5;
    break;
  case MIDI_CC_RPN_LSB:
    RPNLSBBuf[channel] = value;
    break;
  case MIDI_CC_RPN_MSB:
    RPNMsbBuf[channel] = value;
    break;
  case MIDI_MM_AllSoundOff:
    NoteCnt[channel] = 0;
    Gate(channel, OFF);
    break;
  case MIDI_MM_ResetAllControl:
    PBBuf[channel] = 64;
    MWBuf[channel] = 0;
    RateBuf[channel] = 4;
    ShapeBuf[channel] = 0;
    DepthBuf[channel] = 0;
    RPNMsbBuf[channel] = 127;
    RPNLSBBuf[channel] = 127;
    DetuneBuf[channel] = 64;
    break;
  case MIDI_MM_AllNoteOff:
    NoteCnt[channel] = 0;
    Gate(channel, OFF);
    break;
  }
}

// ピッチベンド
void handlePitchBend(byte channel, int bend) {
  bend = bend / 128 - 64;
  if(channel == 9) {
    PBBuf[2] = bend;
    PBBuf[3] = bend;
    PBBuf[4] = bend;
    PBBuf[5] = bend;
  } else {
    PBBuf[channel - 1] = bend;
  }
}

// システムリセット
void handleSystemReset(void) {
  RhythmTrig(RHYTHMOFF);
  AllGateOff();
}

void setup() {
  unsigned char i;

  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);

  WR_DISABLE;

  if (powerCheckOk()) {
    initCMU800();  // CMU-800初期化
    delay(300);
    powerState = ON;
  }

  // バッファ変数初期化
  for(i = 0; i < MAX_CH; i++) {
    CVBuf[i] = 0x80;
    PBBuf[i] = 64;
    MWBuf[i] = 0;
    RateBuf[i] = 4;
    ShapeBuf[i] = 0;
    DepthBuf[i] = 0;
    NoteCnt[i] = 0;
    RPNMsbBuf[i] = 127;
    RPNLSBBuf[i] = 127;
    DetuneBuf[i] = 64;
  }

  for(i = 0; i < 4; i++)  {
    PolyNoteBuf[i] = 0;
    PolyNoteOrder[i] = 0;
  }
  PolyNoteOrderCnt = 1;

  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.setHandleSystemReset(handleSystemReset);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();

  MsTimer2::set(1, intTimer2);
  MsTimer2::start();
}

//-----------------------------------------------
// メイン

void loop() {
  char i;

  if (MIDI.read() == false) {
    if (Setflg) {
      Setflg = false;
      // CV電圧リチャージ
      SetCV(ch & 0x03);      // 1-4ch
      SetCV((ch & 0x03) + 0x04); // 5-8ch
      ch++;
      for(i = 0; i < 6; i++) {
        SetDCO(i);
      }
    }
  }
}

/* EOF */
