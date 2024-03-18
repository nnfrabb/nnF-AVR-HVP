
#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "atProg.h"

void system_Init() {
	DDRC 	= 0xED;//0xb11101101;
	PORTC	= (HVPP_BUTTON | HVPP_RST);
	DDRD    = 0xFC;
	PORTD   = 0x00;
	DDRB	= 0xFF;
	DATAPORT	= 0x00;
}

void HVPP_startPrgMode() {
	// Enter programming mode
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
	_delay_us(1);
	HVPP_CLK_L();
	_delay_us(1);	
}

void HVPP_SendCMD(unsigned char cmd) {// A.
	PORTD |= HVPP_XA1;
	PORTD &= ~(HVPP_XA0|HVPP_BS1);
	
	DATAPORT = cmd;
	_delay_us(1);

	HVPP_CLOCK();
}

void HVPP_LdAddrLByte(unsigned char addr) {// B.
	PORTD &= ~(HVPP_XA0|HVPP_XA1|HVPP_BS1);
	DATAPORT = addr;
	_delay_us(1);

	HVPP_CLOCK();
}

void hvpp_prgAct(uint8_t act,uint8_t data, bool bs1, bool bs2) {
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

void HVPP_LdAddrHByte(unsigned char addr) {// G.
	PORTD &= ~(HVPP_XA0|HVPP_XA1);
	PORTD |= HVPP_BS1;
	DATAPORT = addr;
	_delay_us(1);

	HVPP_CLOCK();
}

void HVPP_LdDataLByte(unsigned char data) {// C.
	PORTD |= HVPP_XA0;
	PORTD &= ~HVPP_XA1;
	DATAPORT = data;
	_delay_us(1);

	HVPP_CLOCK();
}

void HVPP_LdDataHByte(unsigned char data) {// D.
	PORTD |= (HVPP_XA0|HVPP_BS1);
	PORTD &= ~HVPP_XA1;
	DATAPORT = data;
	_delay_us(1);

	HVPP_CLOCK();
}

void HVPP_LatchData() {// E.
	PORTD |= HVPP_BS1;
	_delay_us(1);

	HVPP_PAGEL_H();
	HVPP_PAGEL_L();
	_delay_us(1);
}

void HVPP_WRneg() {// H.
	HVPP_WR_L();
	_delay_us(1);
	HVPP_WR_H();
	while(!(PINC & HVPP_RDY)) ;
}

void HVPP_EndPage() {// J.
	PORTD |= HVPP_XA1;
	PORTD &= ~HVPP_XA0;
	DATAPORT = 0x00;
	_delay_us(1);

	HVPP_CLOCK();
}

bool HVPP_getSignature() {
	HVPP_startPrgMode();
	for(uint8_t i=0;i<3;i++) {
		A(cmdRead_Signature);
		B(i);

		DDRB  = 0x00;
		PORTD &= ~(HVPP_OE | HVPP_BS1);
		_delay_us(1);
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

		_delay_us(1);
		TargetFusebits[i] = PINB;
		_delay_us(10);
	}

	PORTD |= HVPP_OE;
	DDRB  = 0xFF;
	while(!(PINC & HVPP_RDY)) ;
	//_delay_ms(500);
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
		A(cmdWrite_Fuse_bits);
		C(TargetModFusebits[xfuse]);

		if(xfuse==0) {						//LFUSE
			avfuse = true;
		} else if(xfuse==1) {				//HFUSE
			HVPP_BS1_H();
			HVPP_BS2_L();
			avfuse = true;
		} else if((xfuse==2)&&(TargetModFusebits[2]!=0)) {	//EFUSE
			HVPP_BS1_L();
			HVPP_BS2_H();
			avfuse = true;
		} //else return false;

		if(avfuse) {
			HVPP_WR_L(); _delay_us(1);
			HVPP_WR_H(); _delay_us(100);
		}
	
		HVPP_stopPrgMode();
	}
}

void HVPP_WriteLockBit(uint8_t lb) {
	HVPP_startPrgMode();
	A(cmdWrite_Lock_bits);
	B(lb);
	HVPP_WRneg();
	HVPP_stopPrgMode();
}

void HVPP_ChipErese() {
	HVPP_startPrgMode();
	HVPP_SendCMD(cmdChip_Erese);
	HVPP_WR_L();
	_delay_us(1);
	HVPP_WR_H();
	while(!(PINC & HVPP_RDY)) ;
	HVPP_stopPrgMode();
}

void HVPP_ReadFlash(uint8_t pSize) {

}

/*===================================================================*/
bool _getDfFusebits() {
//	HVPP_getSignature();

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
	_delay_us(1);
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
		(data & 0x80)        ? 	(PORTD |= (HVSP_SDI)):(PORTD &= ~(HVSP_SDI));
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
			writeFuseLowBits(TargetDefaultFusebits[0]);
		}

		if(TargetFusebits[1]!=TargetDefaultFusebits[1]) {
			writeFuseHighBits(TargetDefaultFusebits[1]);
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

static void writeFuseLowBits(uint8_t code) {
	hvsp_transferByte(0x40, 0x4C);
	hvsp_transferByte(code, 0x2C);
	hvsp_transferByte(0x00, 0x64);
	hvsp_transferByte(0x00, 0x6C);
	pollSDO();
	hvsp_transferByte(0x00, 0x4C);
	pollSDO();
}

static void writeFuseHighBits(uint8_t code) {
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

byte hvsp_readFlash(uint32_t addr) {
	byte high = addr & 1;   // set if high byte wanted
	addr >>= 1;				// turn into word address
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

void hvsp_writeFlash (unsigned long addr, const byte data) {
	byte high = addr & 1;
	addr >>= 1;
	static byte lowData = 0xFF;

	if (!high) {
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

void hvsp_commitPage(uint32_t addr, uint32_t pSize) {
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
	Serial.begin(250000);
	Serial.println(">>Hello World."); 

	for(;;) {
		system_Init();
		String prgsMode = "";
		String inputCMD = ">>";
		String arg0     = "";
		String arg1     = "";
		String arg2     = "";
		
		uint8_t prgMode  = 0;
		uint8_t btCount	 = 0;
		bool cmdComplete = false;
		bool btLoop		 = true;
		bool wrDfFuse 	 = false;
		bool sinfo 	     = true;
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
				wrDfFuse = true;
				btLoop = false;
			}
		}
		
		//Serial.println(inputCMD);
		arg0 = inputCMD.substring(2,9);
		arg1 = inputCMD.substring(9,11);

		uint8_t arg1i = arg1.toInt();
		arg0.toUpperCase();

		while(!(PINC & HVPP_BUTTON)) { _NOP(); }

		_delay_ms(50);
		HVPP_CLK_L();
		HVPP_VCC_L();
		_delay_ms(300);

		if(HVPP_getSignature()) { 
			prgMode = 1;
			HVPP_getFusebits();
			_getDfFusebits();
			prgsMode = "HVPP";

		} else if(hvsp_getSignature()) {
			prgMode = 2;
			hvsp_getFuseBits();
			_getDfFusebits();
			prgsMode = "HVSP";
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

		bool tinyUplf = false;
		bool tinyUphf = false;
		bool megaUplf = false;
		bool megaUphf = false;
		bool pfinfo   = false;

		if((arg0=="+FCTRST")&&(prgMode!=0)) {
			wrDfFuse = true;
		} else if((arg0=="+MAXCLK")&&(prgMode!=0)) {
			TargetModFusebits[0] = arg1i;
			//tiny85
			if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x0B)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(7);
			//tiny45
			} else if((TargetSignature[1]==0x92)&&(TargetSignature[2]==0x06)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(7);
			//tiny13
			} else if((TargetSignature[1]==0x90)&&(TargetSignature[2]==0x07)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(4);
			//tiny25
			} else if((TargetSignature[1]==0x91)&&(TargetSignature[2]==0x08)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(4);
			//tiny24
			} else if((TargetSignature[1]==0x91)&&(TargetSignature[2]==0x0B)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(4);
			//tiny44
			} else if((TargetSignature[1]==0x92)&&(TargetSignature[2]==0x07)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(4);
			//tiny84
			} else if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x0C)) {
				TargetModFusebits[0] = TargetFusebits[0] | _BV(4);

			//mega48
			} else if((TargetSignature[1]==0x92)&&(TargetSignature[2]==0x05)) {
				TargetModFusebits[0] = 0xE2;
			//mega8
			} else if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x07)) {
				TargetModFusebits[0] = 0xE4;
			//mega88
			} else if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x0A)) {
				TargetModFusebits[0] = 0xE2;
			//mega168
			} else if((TargetSignature[1]==0x94)&&(TargetSignature[2]==0x06)) {
				TargetModFusebits[0] = 0xE2;
			//mega328P
			} else if((TargetSignature[1]==0x95)&&(TargetSignature[2]==0x0F)) {
				TargetModFusebits[0] = 0xE2;
			//mega328
			} else if((TargetSignature[1]==0x95)&&(TargetSignature[2]==0x14)) {
				TargetModFusebits[0] = 0xE2;
			}

			if(TargetModFusebits[0] != 0x00) {
				if(prgMode==1) megaUplf = true;
				if(prgMode==2) tinyUplf = true;
				//pfinfo = true;
			}
		} else if((arg0=="+RSTDIS")&&(prgMode!=0)) {
			TargetModFusebits[1] = arg1i;
			//tiny85
			if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x0B)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//tiny45
			} else if((TargetSignature[1]==0x92)&&(TargetSignature[2]==0x06)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//tiny13
			} else if((TargetSignature[1]==0x90)&&(TargetSignature[2]==0x07)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(0);
			//tiny25
			} else if((TargetSignature[1]==0x91)&&(TargetSignature[2]==0x08)) {
				TargetModFusebits[1] = TargetFusebits[1] | _BV(7);
			//tiny24
			} else if((TargetSignature[1]==0x91)&&(TargetSignature[2]==0x0B)) {
				TargetModFusebits[1] = TargetFusebits[1] | _BV(7);
			//tiny44
			} else if((TargetSignature[1]==0x92)&&(TargetSignature[2]==0x07)) {
				TargetModFusebits[1] = TargetFusebits[1] | _BV(7);
			//tiny84
			} else if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x0C)) {
				TargetModFusebits[1] = TargetFusebits[1] | _BV(7);

			//mega48
			} else if((TargetSignature[1]==0x92)&&(TargetSignature[2]==0x05)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//mega8
			} else if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x07)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//mega88
			} else if((TargetSignature[1]==0x93)&&(TargetSignature[2]==0x0A)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//mega168
			} else if((TargetSignature[1]==0x94)&&(TargetSignature[2]==0x06)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//mega328P
			} else if((TargetSignature[1]==0x95)&&(TargetSignature[2]==0x0F)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			//mega328
			} else if((TargetSignature[1]==0x95)&&(TargetSignature[2]==0x14)) {
				TargetModFusebits[1] = TargetFusebits[1] & ~_BV(7);
			}

			if(TargetModFusebits[1] != 0x00) {
				if(prgMode==1) megaUphf = true;
				if(prgMode==2) tinyUphf = true;
				//pfinfo = true;
			}
		} else if((arg0=="+LCKBIT")&&(prgMode!=0)) {
			if(prgMode==1) HVPP_WriteLockBit(0xFC);
			if(prgMode==2) writeLockBits(0xFC);
		} else if((arg0=="+ERCHIP")&&(prgMode!=0)) {//+erchip
			if(prgMode==1) HVPP_ChipErese();
			if(prgMode==2) hvsp_chipErase();
		} else if((arg0=="+RDFLSH")&&(prgMode!=0)) {//+rdflsh
			bool rm = true;
			uint8_t dCt = 0;
			uint32_t addr = 0;
			uint32_t msize = (uint32_t)(arg1i * 1024) - 1;
			
			if(prgMode==1) {
				for(addr=0;addr<msize;addr+=2){
					;
				}
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
			sinfo = false;
		}

		if(tinyUplf) {
			hvsp_startPMode();
			writeFuseLowBits(TargetModFusebits[0]);
			hvsp_stopPMode();
			hvsp_getFuseBits();
		} else if(tinyUphf) {
			hvsp_startPMode();
			writeFuseHighBits(TargetModFusebits[1]);
			hvsp_stopPMode();
			hvsp_getFuseBits();
		}  else if(megaUplf) {
			HVPP_WriteModFusebits(0);
			HVPP_getFusebits();
		}else if(megaUphf) {
			HVPP_WriteModFusebits(1);
			HVPP_getFusebits();
		}

		if(wrDfFuse) {
			if(prgMode==1) {
				HVPP_ChipErese();
				if(HVPP_WriteDfFusebits()) 
					HVPP_getFusebits();
			}

			if(prgMode==2) {
				hvsp_chipErase();
				if(hvsp_writeDfFuseBits())
					hvsp_getFuseBits();
			}
		}

		if(sinfo) {
			if(prgMode!=0) {
				if(prgMode==1) HVPP_getFusebits();
				if(prgMode==2) hvsp_getFuseBits();

				Serial.println(prgsMode);
				for(uint8_t i=0;i<3;i++) {
					if(TargetSignature[i]<0x10)  Serial.print(0);
					Serial.print(TargetSignature[i],HEX);
				}
				Serial.println();

				for(uint8_t i=0;i<4;i++) {
					if(TargetFusebits[i]<0x10)  Serial.print(0);
					Serial.print(TargetFusebits[i],HEX);
				}
				Serial.println();

				for(uint8_t i=0;i<3;i++) {
					if(TargetDefaultFusebits[i]<0x10)  Serial.print(0);
					Serial.print(TargetDefaultFusebits[i],HEX);
				}
				Serial.println();				
			} else {
				HVPP_VCC_H();
				for(uint8_t i=0;i<6;i++) {
					PORTC ^= HVPP_CLK;
					_delay_ms(500);
				}
				HVPP_VCC_L();
			}
		} 
	Serial.flush();
	}
	return 0;
}
