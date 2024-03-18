#include <stdbool.h>
#include <avr/io.h>

#define cmdChip_Erese		0x80 //0b10000000
#define cmdWrite_Fuse_bits	0x40 //0b01000000
#define cmdWrite_Lock_bits	0x20 //0b00100000
#define cmdWrite_Flash		0x10 //0b00010000
#define cmdWrite_EEPROM		0x11 //0b00010001
#define cmdRead_Signature	0x08 //0b00001000
#define cmdRead_Fuses_bits	0x04 //0b00000100	
#define cmdRead_Flash		0x02 //0b00000010	
#define cmdRead_EEPROM		0x03 //0b00000011

#define ppLDCMD				1
#define ppLDADDR			2
#define ppLDDATA			3
#define ppIDLE				4

#define CMD_WRITE_FLASH       	0b00010000

#define SII_LOAD_COMMAND         0b01001100
#define SII_LOAD_ADDRESS_LOW     0b00001100
#define SII_LOAD_ADDRESS_HIGH    0b00011100
#define SII_READ_LOW_BYTE        0b01101000
#define SII_READ_HIGH_BYTE       0b01111000
#define SII_WRITE_LOW_BYTE       0b01100100
#define SII_WRITE_HIGH_BYTE      0b01110100
#define SII_LOAD_LOW_BYTE        0b00101100
#define SII_LOAD_HIGH_BYTE       0b00111100
#define SII_WRITE_EXTENDED_FUSE  0b01100110
#define SII_PROGRAM_LOW_BYTE     0b01101101 
#define SII_PROGRAM_HIGH_BYTE    0b01111101
#define SII_READ_EEPROM          0b01101000
#define SII_OR_MASK              0b00001100

// PORTC
#define	HVPP_RST		_BV(PC0)//(1<<0)
#define	HVPP_RST_L()	PORTC &= ~HVPP_RST
#define	HVPP_RST_H()	PORTC |= HVPP_RST
#define	HVPP_BUTTON		_BV(PC1)//(1<<1)
#define	HVPP_BS2		_BV(PC2)//(1<<2)
#define	HVPP_BS2_L()	PORTC &= ~HVPP_BS2
#define	HVPP_BS2_H()	PORTC |= HVPP_BS2
#define	HVPP_CLK		_BV(PC3)//(1<<3)
#define	HVPP_CLK_L()	PORTC &= ~HVPP_CLK
#define	HVPP_CLK_H()	PORTC |= HVPP_CLK
#define	HVPP_RDY		_BV(PC4)//(1<<4)
#define	HVPP_VCC		_BV(PC5)//(1<<5)
#define	HVPP_VCC_L()	PORTC &= ~HVPP_VCC
#define	HVPP_VCC_H()	PORTC |= HVPP_VCC

//PORTD
#define	HVPP_OE			_BV(PD2)//(1<<2)
#define	HVPP_OE_L()		PORTD &= ~HVPP_OE
#define	HVPP_OE_H()		PORTD |= HVPP_OE
#define	HVPP_WR			_BV(PD3)//(1<<3)
#define	HVPP_WR_L()		PORTD &= ~HVPP_WR
#define	HVPP_WR_H()		PORTD |= HVPP_WR
#define	HVPP_BS1		_BV(PD4)//(1<<4)
#define	HVPP_BS1_L()	PORTD &= ~HVPP_BS1
#define	HVPP_BS1_H()	PORTD |= HVPP_BS1
#define	HVPP_XA0		_BV(PD5)//(1<<5)
#define	HVPP_XA0_L()	PORTD &= ~HVPP_XA0
#define	HVPP_XA0_H()	PORTD |= HVPP_XA0
#define	HVPP_XA1		_BV(PD6)//(1<<6)
#define	HVPP_XA1_L()	PORTD &= ~HVPP_XA1
#define	HVPP_XA1_H()	PORTD |= HVPP_XA1
#define	HVPP_PAGEL		_BV(PD7)//(1<<7)
#define	HVPP_PAGEL_L()	PORTD &= ~HVPP_PAGEL
#define	HVPP_PAGEL_H()	PORTD |= HVPP_PAGEL

#define DATAPORT		PORTB

#define	HVSP_RST		_BV(PC0)//(1<<0)
#define	HVSP_VCC		_BV(PC5)//(1<<5)
#define HVSP_SDO		_BV(PD2)
#define HVSP_SII		_BV(PD3)
#define HVSP_SDI		_BV(PD4)
#define HVSP_SCI4		_BV(PC3)
#define hvsp_sci4_h()	PORTC |= HVSP_SCI4
#define hvsp_sci4_l()	PORTC &= ~HVSP_SCI4
#define HVSP_SCI14		_BV(PC3)
#define hvsp_sci14_h()	PORTC |= HVSP_SCI4
#define hvsp_sci14_l()	PORTC &= ~HVSP_SCI4

#define LFUSE		0
#define HFUSE		1
#define EFUSE		2

#define A(x)	HVPP_SendCMD(x)
#define B(x)	HVPP_LdAddrLByte(x)
#define C(x)	HVPP_LdDataLByte(x)
#define D(x)	HVPP_LdDataHByte(x)
#define E(x)	HVPP_LatchData()
#define G(x)	HVPP_LdAddrHByte(x)
#define H()		HVPP_WRneg()
#define J()		HVPP_EndPage()	

uint8_t TargetSignature[3];
uint8_t TargetFusebits[4];
uint8_t TargetDefaultFusebits[] = {0,0,0};
uint8_t TargetModFusebits[] = {0,0,0};

unsigned int hexData[512];
unsigned int pageSize = 0;

uint8_t dataLByte;
uint8_t dataHByte;

typedef struct {
	uint8_t		signature[3];
	uint8_t		fuseLowBits;
	uint8_t		fuseHighBits;
	uint8_t		fuseExtendedBits;
//	String 		targetName;
} TargetCpuInfo_t;

static const TargetCpuInfo_t	PROGMEM	targetCpu[] = 
{
	{	// ATtiny13
		.signature	      = { SIGNATURE_0, 0x90, 0x07 },
		.fuseLowBits	    = 0x6A,
		.fuseHighBits	    = 0xFF,
		.fuseExtendedBits	= 0x00,
//		.targetName 		= "ATTINY13",
	},
	{	// ATtiny25
		.signature	      = { SIGNATURE_0, 0x91, 0x08 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATTINY25",
	},
	{	// ATtiny45
		.signature	      = { SIGNATURE_0, 0x92, 0x06 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATTINY45",
	},
	{	// ATtiny85
		.signature	      = { SIGNATURE_0, 0x93, 0x0B },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATTINY85",
	},
	{	// ATtiny24
		.signature	      = { SIGNATURE_0, 0x91, 0x0B },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATTINY24",
	},
	{	// ATtiny44
		.signature	      = { SIGNATURE_0, 0x92, 0x07 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATTINY44",
	},
	{	// ATtiny84
		.signature	      = { SIGNATURE_0, 0x93, 0x0C },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATTINY84",
	},
/*********************************************************/
	{	// ATmega48A
		.signature	      = { SIGNATURE_0, 0x92, 0x05 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA48A",
	},
	{	// ATmega48PA
		.signature	      = { SIGNATURE_0, 0x92, 0x0A },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA48PA",
	},
	{	// ATmega8(L)
		.signature	      = { SIGNATURE_0, 0x93, 0x07 },
		.fuseLowBits	    = 0xE1,
		.fuseHighBits	    = 0xD9,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA8(L)",
	},	
	{	// ATmega88A
		.signature	      = { SIGNATURE_0, 0x93, 0x0A },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xF9,
//		.targetName 		= "ATMEGA88A",
	},
	{	// ATmega88PA
		.signature	      = { SIGNATURE_0, 0x93, 0x0F },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xF9,
//		.targetName 		= "ATMEGA88PA",
	},
	{	// ATmega16
		.signature	      = { SIGNATURE_0, 0x94, 0x03 },
		.fuseLowBits	    = 0xE1,
		.fuseHighBits	    = 0x99,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA16",
	},
	{	// ATmega168A
		.signature	      = { SIGNATURE_0, 0x94, 0x06 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xF9,
//		.targetName 		= "ATMEGA168A",
	},
	{	// ATmega168PA
		.signature	      = { SIGNATURE_0, 0x94, 0x0B },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xF9,
//		.targetName 		= "ATMEGA168PA",
	},
	{	// ATmega32
		.signature	      = { SIGNATURE_0, 0x95, 0x02 },
		.fuseLowBits	    = 0xE1,
		.fuseHighBits	    = 0x99,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA32",
	},
	{	//ATmega328
		.signature	      = { SIGNATURE_0, 0x95, 0x14 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xD9,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA328",
	},
	{	//ATmega328P
		.signature	      = { SIGNATURE_0, 0x95, 0x0F },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xD9,
		.fuseExtendedBits	= 0xFF,
//		.targetName 		= "ATMEGA328P",
	},
	{	//ATmega328PB
		.signature	      = { SIGNATURE_0, 0x95, 0x16 },
		.fuseLowBits	    = 0x62,
		.fuseHighBits	    = 0xDF,
		.fuseExtendedBits	= 0xF7,
//		.targetName 		= "ATMEGA328P",
	},
	  // mark end of list
	{ { 0,0,0 }, 0, 0, 0, },
/*********************************************************/
};