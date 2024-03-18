// For ATMEGA8/8L/88 /168/328 P
// Osc 8MHz Internal 
// Baud rate 250000

#define F_CPU 8000000UL   //Internal Clock

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdbool.h>
#include "atProg.h"

uint8_t prgMode  = 0;

void system_Init() {
  DDRC  = 0xED;//0xb11101101;
  PORTC |= (HVPP_BUTTON | HVPP_RST);
  DDRD    = 0xFC;
  PORTD   = 0x00;
  DDRB  = 0xFF;
  DATAPORT  = 0x00;
}

void HVPP_startPrgMode() {
  PORTD &= ~(HVPP_PAGEL|HVPP_XA1|HVPP_XA0|HVPP_BS1);
  PORTC &= ~(HVPP_BS2);
  
  PORTC |= HVPP_VCC;
  PORTD |= HVPP_WR|HVPP_OE;
  _delay_ms(1);
  PORTC &= ~(HVPP_RST);
  _delay_ms(1);
}

void HVPP_stopPrgMode() {
  // Exit programming mode
  PORTC |= HVPP_RST;
  PORTC &= ~(HVPP_VCC|HVPP_BS2);
  PORTD &= ~(HVPP_PAGEL|HVPP_WR|HVPP_OE|HVPP_XA0|HVPP_XA1|HVPP_BS1);

  DATAPORT = 0x00;
}

void HVPP_CLOCK() {
  HVPP_CLK_H();
  _delay_us(5);
  HVPP_CLK_L();
  _delay_us(5); 
}

void HVPP_SendCMD(unsigned char cmd) {// A.
  PORTD |= HVPP_XA1;
  PORTD &= ~(HVPP_XA0|HVPP_BS1);
  
  DATAPORT = cmd;
  HVPP_CLOCK();
}

void HVPP_prgAct(uint8_t act,uint8_t data, bool bs1, bool bs2) {
  switch(act) {
    case ppLDCMD:
    case ppLDADDR:
    case ppLDDATA:
    case ppIDLE:

    bs1 ? HVPP_BS1_H() : HVPP_BS1_L();
    bs2 ? HVPP_BS2_H() : HVPP_BS2_L();
  }
  DATAPORT = data;
  HVPP_CLOCK();
}

void HVPP_LdAddrLByte(unsigned char addr) {// B.
  PORTD &= ~(HVPP_XA0|HVPP_XA1|HVPP_BS1);
  DATAPORT = addr;
  HVPP_CLOCK();
}

void HVPP_LdAddrHByte(unsigned char addr) {// G.
  PORTD &= ~(HVPP_XA0|HVPP_XA1);
  PORTD |= HVPP_BS1;
  DATAPORT = addr;
  HVPP_CLOCK();
}

void HVPP_LdDataLByte(unsigned char data) {// C.
  PORTD |= HVPP_XA0;
  PORTD &= ~(HVPP_XA1|HVPP_BS1);
  DATAPORT = data;
  HVPP_CLOCK();
}

void HVPP_LdDataHByte(unsigned char data) {// D.
  PORTD |= (HVPP_XA0|HVPP_BS1);
  PORTD &= ~HVPP_XA1;
  DATAPORT = data;
  HVPP_CLOCK();
}

void HVPP_LatchData() {// E.
  PORTD |= HVPP_BS1;
  _delay_us(5);

  HVPP_PAGEL_H();
  _delay_us(5);
  HVPP_PAGEL_L();
}

void HVPP_WRneg() {// H.
  HVPP_WR_L();
  _delay_us(5);
  HVPP_WR_H();
  while(!(PINC & HVPP_RDY)) ;
}

void HVPP_EndPage() {// J.
  PORTD |= HVPP_XA1;
  PORTD &= ~HVPP_XA0;
  DATAPORT = 0x00;
  _delay_us(5);

  HVPP_CLOCK();
}

bool HVPP_getSignature() {
  HVPP_startPrgMode();
  for(uint8_t i=0;i<3;i++) {
    HVPP_SendCMD(cmdRead_Signature);
    HVPP_LdAddrLByte(i);

    DDRB  = 0x00;
    PORTD &= ~(HVPP_OE | HVPP_BS1);
    _delay_us(5);
    TargetSignature[i] = PINB;
    HVPP_OE_H();
    DDRB  = 0xff;
  }
  while(!(PINC & HVPP_RDY)) ;
  HVPP_stopPrgMode();

  for(uint8_t device=0; pgm_read_byte(&targetCpu[device].signature[0])!=0 ; device++) {
    if (TargetSignature[0] == pgm_read_byte(&targetCpu[device].signature[0]) && 
      TargetSignature[1] == pgm_read_byte(&targetCpu[device].signature[1]) && 
      TargetSignature[2] == pgm_read_byte(&targetCpu[device].signature[2])) {
       return true;
    }
  }
  return false;
}

void HVPP_getFusebits() {
  HVPP_startPrgMode();
  HVPP_SendCMD(cmdRead_Fuses_bits);
  DDRB  = 0x00;
  for(uint8_t i=0;i<4;i++) {
    switch(i) {
      case 0:
        PORTD &= ~(HVPP_OE | HVPP_BS1);
        PORTC &= ~(HVPP_BS2);
      break;
      case 1:
        PORTD &= ~(HVPP_OE);
        PORTD |= (HVPP_BS1);
        PORTC |= (HVPP_BS2);
      break;
      case 2:
        PORTD &= ~(HVPP_OE | HVPP_BS1);
        PORTC |= (HVPP_BS2);
      break;
      case 3:
        PORTD &= ~(HVPP_OE);
        PORTD |= (HVPP_BS1);
        PORTC &= ~(HVPP_BS2);
      break;
    }

    _delay_us(5);
    TargetFusebits[i] = PINB;
    _delay_us(10);
  }

  PORTD |= HVPP_OE;
  DDRB  = 0xFF;
  while(!(PINC & HVPP_RDY)) ;
  HVPP_stopPrgMode();
}

bool HVPP_WriteDfFusebits() {
  if(HVPP_getSignature()) {
    _getDfFusebits();
    for(uint8_t i=0;i<3;i++) {
      TargetModFusebits[i] = TargetDefaultFusebits[i];
      HVPP_WriteModFusebits(i);
    }

    HVPP_getFusebits();
    if((TargetFusebits[0]==TargetDefaultFusebits[0]) &&
      (TargetFusebits[1]==TargetDefaultFusebits[1]) &&
      (TargetFusebits[2]==TargetDefaultFusebits[2])) {
      return true;
    }// else return false;
  }
  return false;
}

void HVPP_WriteModFusebits(uint8_t xfuse) {
  if(_getDfFusebits()) {
    HVPP_startPrgMode();

    bool avfuse = false;
    //A(cmdWrite_Fuse_bits);
    HVPP_SendCMD(cmdWrite_Fuse_bits);
    //C(TargetModFusebits[xfuse]);
    HVPP_LdDataLByte(TargetModFusebits[xfuse]);

    if(xfuse==0) {            //LFUSE
      avfuse = true;
    } else if(xfuse==1) {       //HFUSE
      HVPP_BS1_H();
      HVPP_BS2_L();
      avfuse = true;
    } else if((xfuse==2)&&(TargetModFusebits[2]!=0)) {  //EFUSE
      HVPP_BS1_L();
      HVPP_BS2_H();
      avfuse = true;
    } //else return false;

    if(avfuse) {
      HVPP_WR_L(); _delay_us(5);
      HVPP_WR_H(); _delay_us(100);
    }
    HVPP_stopPrgMode();
  }
}

void HVPP_WriteLockBit(uint8_t lb) {
  HVPP_startPrgMode();
  HVPP_SendCMD(cmdWrite_Lock_bits);
  HVPP_LdAddrLByte(lb);
  HVPP_WRneg();
  HVPP_stopPrgMode();
}

void HVPP_ChipErese() {
  HVPP_startPrgMode();
  HVPP_SendCMD(cmdChip_Erese);
  HVPP_WR_L();
  _delay_us(5);
  HVPP_WR_H();
  while(!(PINC & HVPP_RDY)) ;
  HVPP_stopPrgMode();
}

byte HVPP_ReadFlash(uint16_t addr) {
  bool high = addr & 0x0001;   // set if high byte wanted
  addr >>= 1;                  // turn into word address
  HVPP_SendCMD(cmdRead_Flash);
  HVPP_LdAddrHByte((addr>>8)&0xFF);
  HVPP_LdAddrLByte(addr&0xFF);
  DDRB  = 0x00;
  HVPP_OE_L();
  _delay_us(10);
  HVPP_BS1_H();
  byte hDB = PINB;
  _delay_us(10);
  HVPP_BS1_L();
  byte lDB = PINB;
  HVPP_OE_H();
  DDRB  = 0xFF;
  return high ? lDB : hDB;
}

byte HVPP_WriteFlash(uint16_t addr) {

}

/*===================================================================*/
bool _getDfFusebits() {
  for(uint8_t device = 0; pgm_read_byte(&targetCpu[device].signature[0])!=0 ; device++) {
      if (TargetSignature[0] == pgm_read_byte(&targetCpu[device].signature[0]) && 
        TargetSignature[1] == pgm_read_byte(&targetCpu[device].signature[1]) && 
        TargetSignature[2] == pgm_read_byte(&targetCpu[device].signature[2])) {

        TargetDefaultFusebits[0] = pgm_read_byte(&targetCpu[device].fuseLowBits);
        TargetDefaultFusebits[1] = pgm_read_byte(&targetCpu[device].fuseHighBits);
        TargetDefaultFusebits[2] = pgm_read_byte(&targetCpu[device].fuseExtendedBits);
        
        return true;
      }
    }
    return false;
}
/*===================================================================*/

void hvsp_startPMode() {
    DDRD |= (HVSP_SII | HVSP_SDI | HVSP_SDO);
    DDRC |= (HVSP_SCI4 | HVSP_RST | HVSP_VCC);

    PORTD &= ~(HVSP_SII | HVSP_SDI | HVSP_SDO);
    PORTC |= HVSP_RST;
    PORTC &= ~(HVSP_SCI4 | HVSP_VCC);
    _delay_ms(50);      

    PORTC |= (HVSP_VCC);

    _delay_us(60);
    PORTC &= ~(HVSP_RST);

    _delay_us(20);
    DDRD  &= ~(HVSP_SDO);

    _delay_us(300);
}

void hvsp_stopPMode() {
    PORTD &= ~(HVSP_SII | HVSP_SDI);
    PORTC &= ~HVSP_SCI4;
    PORTC |= HVSP_RST;
    _delay_us(10);
    PORTC &= ~(HVSP_VCC);
}

void hvsp_Clock() {
  hvsp_sci4_h();
  _delay_us(5);
  hvsp_sci4_l();
}

void pollSDO(void) {
  // wait until SDO goes high
  while ((PIND & (HVSP_SDO))==0) ;
}

static uint8_t hvsp_transferByte(uint8_t data, uint8_t command) {
  uint8_t   byteRead = 0;

  PORTD &= ~(HVSP_SDI | HVSP_SII);
  hvsp_Clock();

  for(uint8_t i=0;i<8;i++) {
    byteRead <<= 1;
    
    ((PIND & HVSP_SDO)!=0) ?  (byteRead|=1):0;
    (data & 0x80)        ?  (PORTD |= (HVSP_SDI)):(PORTD &= ~(HVSP_SDI));
    (command & 0x80) ?  (PORTD |= (HVSP_SII)):(PORTD &= ~(HVSP_SII));
    hvsp_Clock();
    
    data <<= 1;
    command <<= 1;
  }

  /* Last two bits are zero */
  PORTD &= ~(HVSP_SDI | HVSP_SII);    
  hvsp_Clock();
  hvsp_Clock();

  return byteRead;
}

bool hvsp_getSignature() {
  hvsp_startPMode();
  for(uint8_t i=0;i<3;i++) {
    hvsp_transferByte(0x08, 0x4C);
    hvsp_transferByte(i, 0x0C);
    hvsp_transferByte(0x00, 0x68);
    TargetSignature[i] = hvsp_transferByte(0x00, 0x6C);
    _delay_us(10);
  }
  hvsp_stopPMode();

  for(uint8_t device = 0; pgm_read_byte(&targetCpu[device].signature[0])!=0 ; device++) {
      if (TargetSignature[0] == pgm_read_byte(&targetCpu[device].signature[0]) && 
        TargetSignature[1] == pgm_read_byte(&targetCpu[device].signature[1]) && 
        TargetSignature[2] == pgm_read_byte(&targetCpu[device].signature[2])) {

        return true;
      }
    }
    return false;
}

bool hvsp_getFuseBits(void) {
  if(hvsp_getSignature()) {
    _getDfFusebits();
    hvsp_startPMode();

    hvsp_transferByte(0x04, 0x4C);
    hvsp_transferByte(0x00, 0x68);
    TargetFusebits[0] = hvsp_transferByte(0x00, 0x6C);

    hvsp_transferByte(0x04, 0x4C);
    hvsp_transferByte(0x00, 0x7A);
    TargetFusebits[1] = hvsp_transferByte(0x00, 0x7E);

    if(TargetDefaultFusebits[2]!=0) {
      hvsp_transferByte(0x04, 0x4C);
      hvsp_transferByte(0x00, 0x6A);
      TargetFusebits[2] = hvsp_transferByte(0x00, 0x6E);
    } else TargetFusebits[2] = 0x00;

    hvsp_transferByte(0x04, 0x4C);
    hvsp_transferByte(0x00, 0x78);
    TargetFusebits[3] = hvsp_transferByte(0x00, 0x7C);

    hvsp_stopPMode();

    for(uint8_t device = 0; pgm_read_byte(&targetCpu[device].signature[0])!=0 ; device++) {
        if (TargetSignature[0] == pgm_read_byte(&targetCpu[device].signature[0]) && 
          TargetSignature[1] == pgm_read_byte(&targetCpu[device].signature[1]) && 
          TargetSignature[2] == pgm_read_byte(&targetCpu[device].signature[2]))
        {
          /* get default fuse bits for selected target */
          TargetDefaultFusebits[0] = pgm_read_byte(&targetCpu[device].fuseLowBits);
          TargetDefaultFusebits[1] = pgm_read_byte(&targetCpu[device].fuseHighBits);
          TargetDefaultFusebits[2] = pgm_read_byte(&targetCpu[device].fuseExtendedBits);
        }
      }
    return true;
  }
  return false;
}

bool hvsp_writeDfFuseBits() {
  if(hvsp_getSignature()) {
    _getDfFusebits();
    hvsp_startPMode();

    if(TargetFusebits[0]!=TargetDefaultFusebits[0]) {
      writeLowFuseBits(TargetDefaultFusebits[0]);
    }

    if(TargetFusebits[1]!=TargetDefaultFusebits[1]) {
      writeHighFuseBits(TargetDefaultFusebits[1]);
    }

    if((TargetDefaultFusebits[2]!=0) && (TargetFusebits[2]!=TargetDefaultFusebits[2])) {
      writeFuseExtendedBits(TargetDefaultFusebits[2]);
    }
    hvsp_stopPMode();
  }
  hvsp_getFuseBits();

  if((TargetFusebits[0]==TargetDefaultFusebits[0]) &&
    (TargetFusebits[1]==TargetDefaultFusebits[1]) &&
    (TargetFusebits[2]==TargetDefaultFusebits[2])) {
    return true;
  }
  return false;
}

void hvsp_pollSDO() {

}

void writeLowFuseBits(uint8_t code) {
  hvsp_transferByte(0x40, 0x4C);
  hvsp_transferByte(code, 0x2C);
  hvsp_transferByte(0x00, 0x64);
  hvsp_transferByte(0x00, 0x6C);
  pollSDO();
  hvsp_transferByte(0x00, 0x4C);
  pollSDO();
}

static void writeHighFuseBits(uint8_t code) {
  hvsp_transferByte(0x40, 0x4C);
  hvsp_transferByte(code, 0x2C);
  hvsp_transferByte(0x00, 0x74);
  hvsp_transferByte(0x00, 0x7C);
  pollSDO();
  hvsp_transferByte(0x00, 0x4C);
  pollSDO();
}

static void writeFuseExtendedBits(uint8_t code) {
  hvsp_transferByte(0x40, 0x4C);
  hvsp_transferByte(code, 0x2C);
  hvsp_transferByte(0x00, 0x66);
  hvsp_transferByte(0x00, 0x6E);
  pollSDO();
  hvsp_transferByte(0x00, 0x4C);
  pollSDO();
}

static void writeLockBits(uint8_t code) {
  hvsp_startPMode();
  hvsp_transferByte(0x20, 0x4C);
  hvsp_transferByte(code, 0x2C);
  hvsp_transferByte(0x00, 0x64);
  hvsp_transferByte(0x00, 0x6C);
  pollSDO();
  hvsp_transferByte(0x00, 0x4C);
  pollSDO();
  hvsp_stopPMode();
}

static void hvsp_chipErase() {
  hvsp_startPMode();
  hvsp_transferByte(0x80, 0x4C);
  hvsp_transferByte(0x00, 0x64);
  hvsp_transferByte(0x00, 0x6C);
  pollSDO();
  hvsp_transferByte(0x00, 0x4C);
  pollSDO();
  hvsp_stopPMode();
}

byte hvsp_readFlash(uint16_t addr) {
  byte high = addr & 1;   // set if high byte wanted
  addr >>= 1;             // turn into word address
  hvsp_transferByte(0x02, 0x4C);
  hvsp_transferByte(addr & 0xFF, 0x0C);
  hvsp_transferByte((addr >> 8) & 0xFF, 0x1C);
  hvsp_transferByte(0x00, 0x68);
  byte LByte = hvsp_transferByte(0x00, 0x6C);
  hvsp_transferByte(0x00, 0x78);
  byte HByte = hvsp_transferByte(0x00, 0x7C);

  hvsp_transferByte(0x00, 0x4C);
  pollSDO();

  return high ? HByte : LByte;
}

byte hvsp_readEEROM(uint16_t addr) {
  byte high = addr & 1;   // set if high byte wanted
  addr >>= 1;             // turn into word address
  hvsp_transferByte(0x03, 0x4C);
  hvsp_transferByte(addr & 0xFF, 0x0C);
  hvsp_transferByte((addr >> 8) & 0xFF, 0x1C);
  hvsp_transferByte(0x00, 0x68);
  byte LByte = hvsp_transferByte(0x00, 0x6C);

  hvsp_transferByte(0x00, 0x4C);
  pollSDO();

  return LByte;
}

void hvsp_writeFlash (uint16_t addr, byte data) {
  byte high = addr & 1;
  addr >>= 1;
  byte lowData = 0xFF;

  if(!high) {
      lowData = data;
      return;
  }

  hvsp_transferByte(0x10,        0b01001100);
  hvsp_transferByte(addr & 0xFF, 0b00001100);
  hvsp_transferByte(lowData,     SII_LOAD_LOW_BYTE);
  hvsp_transferByte(0x00,        SII_PROGRAM_LOW_BYTE);     
  hvsp_transferByte(0x00,        SII_PROGRAM_LOW_BYTE | SII_OR_MASK);
  hvsp_transferByte(data,        SII_LOAD_HIGH_BYTE);
  hvsp_transferByte(0x00,        SII_PROGRAM_HIGH_BYTE);
  hvsp_transferByte(0x00,        SII_PROGRAM_HIGH_BYTE | SII_OR_MASK);
  lowData = 0xFF;
}

void hvsp_commitPage(uint16_t addr, uint16_t pSize) {
  addr >>= 1;  // turn into word address
  
  hvsp_transferByte(addr >> 8, SII_LOAD_ADDRESS_HIGH);
  hvsp_transferByte(0x00,      SII_WRITE_LOW_BYTE);
  hvsp_transferByte(0x00,      SII_WRITE_LOW_BYTE | SII_OR_MASK);
  
  pollSDO();
  for (unsigned int i = 0; i < pSize; i++)  //Clear Buffer
    hvsp_writeFlash (i, 0xFF);

  hvsp_transferByte(0x00, 0x4C);
  pollSDO();
}

uint8_t s2hex(String str) {
  uint8_t nb = str.length();
  char cBuff[nb];
  str.toCharArray(cBuff,4);

  uint8_t x = 0;
  for(uint8_t i=0;i<nb;i++) {
      if (str[i] >= '0' && str[i] <= '9') {
        x *= 16;
        x += str[i] - '0'; 
    } else if (str[i] >= 'A' && str[i] <= 'F') {
      x *= 16;
      x += (str[i] - 'A') + 10; 
    }
  }
  return x;
}

int main() {
  sei();
  Serial.begin(38400);
  Serial.println(">>Power On."); 

  for(;;) {
    system_Init();
    String prgsMode = "";
    String inputCMD = ">>";
    String arg0     = "";
    String arg1     = "";
    String arg2     = "";
    
    uint8_t btCount  = 0;
    bool cmdComplete = false;
    bool btLoop    = true;
    bool wrDfFuse    = false;
    bool sinfo       = true;
    uint8_t pmode = 0;

    inputCMD.reserve(22);

    while((PINC&HVPP_BUTTON) && (!cmdComplete)) { 
      while (Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar != '\n') inputCMD += inChar;
        else cmdComplete = true;
      }
    }

    _delay_ms(50);
    while(!(PINC & HVPP_BUTTON) && btLoop) { 
      _delay_ms(30);
      btCount++;
      if(btCount>50) {
        HVPP_VCC_H();
        HVPP_CLK_H();
        inputCMD = ">>+fctrst";
        btLoop = false;
      }
    }
    
    arg0 = inputCMD.substring(2,9);
    arg0.toUpperCase();

    if(arg0=="+FCTRST")      pmode = 1;  //Factory reset
    else if(arg0=="+ERCHIP") pmode = 1;  //Eraese Flash
    else if(arg0=="+WRFUSE") pmode = 3;  //Write Fuse bit
    else if(arg0=="+LCKBIT") pmode = 4;  //Write Lock bit
    else if(arg0=="+RDFLSH") pmode = 5;  //Read Flash
    else if(arg0=="+WRFLSH") pmode = 6;  //Write Flash
    else if(arg0=="+CHINFO") pmode = 7;  //Chip Info
    else if(arg0=="+GETSIG") pmode = 8;  //Get Signature
    else if(arg0=="+WREEPM") pmode = 9;  //Read EEPROM
    else if(arg0=="+RDEEPM") pmode = 10; //Write EEPROM
    else pmode = 0;

    while(!(PINC & HVPP_BUTTON)) { _NOP(); }

    _delay_ms(50);
    HVPP_CLK_L();
    HVPP_VCC_L();
    _delay_ms(300);

    if(HVPP_getSignature()) { 
      prgMode = 1;
      HVPP_getFusebits();
      _getDfFusebits();
      //prgsMode = "HVPP";

    } else if(hvsp_getSignature()) {
      prgMode = 2;
      hvsp_getFuseBits();
      _getDfFusebits();
      //prgsMode = "HVSP";
    } 

    if((arg0=="+GETSIG") && (prgMode!=0)) {
      Serial.print(TargetSignature[0],HEX);
      if(TargetSignature[1]<0x10)
        Serial.print("0");
      Serial.print(TargetSignature[1],HEX);
      if(TargetSignature[2]<0x10)
        Serial.print("0");
      Serial.println(TargetSignature[2],HEX);
      Serial.flush();
      sinfo = false;
    } else if((arg0=="+GETSIG") && (prgMode==0)) {
      Serial.println("000000");
      Serial.flush();
      sinfo = false;
    }

    switch(pmode) {
        case 1: 
          _FCTRST();
          break;
        //case 2: 
        //  _ERCHIP();
        //  break;
        case 3: 
          _WRFUSE();
          break; 
        case 4: 
          _LCKBIT();
          break;
        case 5: 
          arg1 = inputCMD.substring(9,11);
          memSize = arg1.toInt();
          _RDFLSH();
          break;
        case 6:
          arg1 = inputCMD.substring(9,14);
          hexSize = arg1.toInt();
          arg2 = inputCMD.substring(14,17);
          pageSize = arg1.toInt();
          _WRFLSH();
          break;
        case 7:
          _CHINFO();
          break;
        case 8:
          _GETSIG();
          break;
        case 9:
          _WREEPM();
          break;
        case 10:
          _RDEEPM();
          break;
        //default:
    }
    Serial.flush();
  }
  return 0;
}

int StrToHex(char str[]) {
  return (int) strtol(str, 0, 16);
}

void _GETSIG() {
  if(prgMode!=0) {
    Serial.print(TargetSignature[0],HEX);
    if(TargetSignature[1]<0x10)
      Serial.print("0");
    Serial.print(TargetSignature[1],HEX);
    if(TargetSignature[2]<0x10)
      Serial.print("0");
    Serial.println(TargetSignature[2],HEX);
    Serial.flush();
  } else {
    Serial.println("000000");
    Serial.flush();
  }
}

void _FCTRST() {
  if(prgMode==1) {
      HVPP_ChipErese();
      HVPP_WriteDfFusebits();
  } else if(prgMode==2) {
      hvsp_chipErase();
      hvsp_writeDfFuseBits();
  }
}

bool _MAXCLK() {
  _delay_ms(100);
}

bool _WRFUSE() {
  _delay_ms(100);
}

void _RSTDIS() {
  _delay_ms(100);
}

void _WREEPM() {
  _delay_ms(100);
}

void _RDEEPM() {
  _delay_ms(100);
}
/*
void _ERCHIP() {
  if(prgMode==1) {
      HVPP_ChipErese();
      HVPP_WriteDfFusebits();
  } else if(prgMode==2) {
      hvsp_chipErase();
      hvsp_writeDfFuseBits();
  }
}
*/
void _LCKBIT() {
  _delay_ms(100);
}

void _CHINFO() {
  for(uint8_t i=0;i<4;i++) {
    if(TargetFusebits[i]<0x10)  Serial.print(0);
    Serial.print(TargetFusebits[i],HEX);
  }
  Serial.println();
}

void _RDFLSH() {
  uint16_t addr = 0;
  uint32_t msize = (uint32_t)(memSize * 1024)-1;
    
  if(prgMode==1) {
    HVPP_startPrgMode();
    for(uint32_t i=0;i<=msize;i++) {
      addr++;
      byte dB = HVPP_ReadFlash(addr-1);
      if(dB<0x10) Serial.print(0);
      Serial.print(dB, HEX);
      if((addr%0x10)==0) Serial.println();
    }
    HVPP_stopPrgMode();
  } else if(prgMode==2) {
    hvsp_startPMode();
    for(uint32_t i=0;i<=msize;i++) {
      addr++;
      byte dB = hvsp_readFlash(addr-1);
      if(dB<0x10) Serial.print(0);
      Serial.print(dB, HEX);
      if((addr%0x10)==0) Serial.println();
    }
    hvsp_stopPMode();
  }
}

void _WRFLSH() {
  uint16_t addr = 0;
  uint8_t page = 0;
  unsigned int hex = hexSize;
  String hexDat;

  if(prgMode==1) {
    HVPP_startPrgMode();
    for(uint32_t i=0;i<=hexSize;i++) {
      addr++;
      byte dB = HVPP_ReadFlash(addr-1);
      if(dB<0x10) Serial.print(0);
      Serial.print(dB, HEX);
      if((addr%0x10)==0) Serial.println();
    }
    HVPP_stopPrgMode();
  } else if(prgMode==2) {
    hvsp_startPMode();

    while(true) {
      while(Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar != '\n') hexDat += inChar;
      }
      for(int i=0;i<hexSize;i++) {
        addr++;
        //hvsp_writeFlash (addr-1, data);
      }
    }

    hvsp_stopPMode();
  }
}

