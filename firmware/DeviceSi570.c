//************************************************************************
//**
//** Project......: Firmware USB AVR Si570 controler.
//**
//** Platform.....: ATtiny45
//**
//** Licence......: This software is freely available for non-commercial 
//**                use - i.e. for research and experimentation only!
//**                Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
//**                Based on ObDev's AVR USB driver by Christian Starkjohann
//**
//** Programmer...: F.W. Krom, PE0FKO
//**                I like to thank Francis Dupont, F6HSI for checking the
//**                algorithm and add some usefull comment!
//**                Thanks to Tom Baier DG8SAQ for the initial program.
//** 
//** Description..: Calculations for the Si570 chip and Si570 program algorithme.
//**                Changed faster and precise code to program the Si570 device.
//**
//** History......: V15.1 02/12/2008: First release of PE0FKO.
//**                Check the main.c file
//**
//**************************************************************************

#include "main.h"

#ifdef INCLUDE_SI570

static	bool		SI570_online;		// Si570 loaded, i2c open collector line high
static	uint16_t	Si570_N;			// Total division (N1 * HS_DIV)
static	uint8_t		Si570_N1;			// The slow divider
static	uint8_t		Si570_HS_DIV;		// The high speed divider

#ifdef INCLUDE_SMOOTH
		uint32_t	FreqSmoothTune;		// The smooth tune center frequency
#endif

static	void		Si570Write(void);
static	void		Si570Load(void);
static	void		Si570FreezeNCO(void);
static	void		Si570UnFreezeNCO(void);
static	void		Si570NewFreq(void);

// Cost: 140us
// This function only works for the "C" & "B" grade of the Si570 chip.
// It will not check the frequency gaps for the "A" grade chip!!!
static bool
Si570CalcDivider()
{
	// Register finding the lowest DCO frequenty
	uint8_t		xHS_DIV;
	uint16_t	xN1;
	uint16_t	xN;

	// Registers to save the found dividers
	uint8_t		sHS_DIV=0;
	uint8_t		sN1=0;
	uint16_t	sN=0;					// Total dividing

	uint16_t	N0;						// Total divider needed (N1 * HS_DIV)
	sint32_t	Freq;

	Freq.dw = R.Freq;

	// Find the total division needed.
	// It is always one to low (not in the case reminder is zero, reminder not used here).
	// 16.0 bits = 13.3 bits / ( 11.5 bits >> 2)
	N0  = DCO_MIN * _2(3);
	N0 /= Freq.w1.w >> 2;

	sN = 11*128;
	for(xHS_DIV = 11; xHS_DIV > 3; --xHS_DIV)
	{
		// Skip the unavailable divider's
		if (xHS_DIV == 8 || xHS_DIV == 10)
			continue;

		// Calculate the needed low speed divider
		xN1 = N0 / xHS_DIV + 1;
//		xN1 = (N0 + xHS_DIV/2) / xHS_DIV;

		if (xN1 > 128)
			continue;

		// Skip the unavailable divider's
		if (xN1 != 1 && (xN1 & 1) == 1)
			xN1 += 1;

		xN = xHS_DIV * xN1;
		if (sN > xN)
		{
			sN		= xN;
			sN1		= xN1;
			sHS_DIV	= xHS_DIV;
		}
	}

	if (sHS_DIV == 0)
		return 0;

	Si570_N      = sN;
	Si570_N1     = sN1;
	Si570_HS_DIV = sHS_DIV;

	return true;
}

// Cost: 140us
// frequency [MHz] * 2^21
static void
Si570CalcRFREQ(uint32_t freq)
{
	uint8_t		cnt;
	sint32_t	RFREQ;
	uint8_t		RFREQ_b4;
	uint32_t	RR;						// Division remainder
	uint8_t		sN1;

	// Convert divider ratio to SI570 register value
	sN1 = Si570_N1 - 1;
	Si570_Data.N1      = sN1 >> 2;
	Si570_Data.HS_DIV  = Si570_HS_DIV - 4;

	//============================================================================
	// RFREQ = freq * sN * 8 / Xtal
	//============================================================================
	// freq = F * 2^21 => 11.21 bits
	// xtal = F * 2^24 =>  8.24 bits

	// 1- RFREQ:b4 =  Si570_N * freq
	//------------------------------

	//----------------------------------------------------------------------------
	// Product_48 = Multiplicand_16 x Multiplier_32
	//----------------------------------------------------------------------------
	// Multiplicand_16:  N_MSB   N_LSB
	// Multiplier_32  :                  b3      b2      b1      b0
	// Product_48     :  r0      b4      b3      b2      b1      b0
	//                  <--- high ----><---------- low ------------->

	cnt = 32+1;                      // Init loop counter
	asm (
	"clr __tmp_reg__     \n\t"     // Clear Product high bytes  & carry
	"sub %1,%1           \n\t"     // (C = 0)

"L_A_%=:                 \n\t"     // Repeat

	"brcc L_B_%=         \n\t"     //   If(Cy -bit 0 of Multiplier- is set)

	"add %1,%A2          \n\t"     //   Then  add Multiplicand to Product high bytes
	"adc __tmp_reg__,%B2 \n\t"

"L_B_%=:                 \n\t"     //   End If

	                              //   Shift right Product
	"ror __tmp_reg__     \n\t"     //   Cy -> r0
	"ror %A1             \n\t"     //            ->b4
	"ror %D0             \n\t"     //                 -> b3
	"ror %C0             \n\t"     //                       -> b2
	"ror %B0             \n\t"     //                             -> b1
	"ror %A0             \n\t"     //                                   -> b0 -> Cy

	"dec %3              \n\t"     // Until(--cnt == 0)
	"brne L_A_%=         \n\t"

	// Output operand list
	//--------------------
	: "=r" (RFREQ.dw)               // %0 -> Multiplier_32/Product b0,b1,b2,b3
	, "=r" (RFREQ_b4)               // %1 -> Product b4
	
	// Input operand list
	//-------------------
	: "r" (Si570_N)                 // %2 -> Multiplicand_16
	, "r" (cnt)                     // %3 -> Loop_Counter
	, "0" (freq)

//	: "r0"                          // r0 -> Tempory register
	);

	// 2- RFREQ:b4 = RFREQ:b4 * 8 / FreqXtal
	//---------------------------------------------
	
	//---------------------------------------------------------------------------
	// Quotient_40 = Dividend_40 / Divisor_32
	//---------------------------------------------------------------------------
	// Dividend_40: RFREQ     b4      b3      b2      b1      b0
	// Divisor_32 : FreqXtal
	// Quotient_40: RFREQ     b4      b3      b2      b1      b0
	//---------------------------------------------------------------------------

	RR = 0;							// Clear Remainder_40
	cnt = 40+1+28+3;				// Init Loop_Counter
									// (28 = 12.28 bits, 3 = * 8)
	// DCO = freq * sN
	// if (RFREQ:b4 > DCO_MAX)
	//	return 0;

	asm (
	"clc                 \n\t"		// Partial_result = carry = 0
	
"L_A_%=:                 \n\t"		// Repeat
	"rol %0              \n\t"		//   Put last Partial_result in Quotient_40
	"rol %1              \n\t"		//   and shift left Dividend_40 ...
	"rol %2              \n\t"
	"rol %3              \n\t"
	"rol %4              \n\t"

	"rol %A6             \n\t"		//                      ... into Remainder_40
	"rol %B6             \n\t"
	"rol %C6             \n\t"
	"rol %D6             \n\t"

	"sub %A6,%A7         \n\t"		//   Remainder =  Remainder - Divisor
	"sbc %B6,%B7         \n\t"
	"sbc %C6,%C7         \n\t"
	"sbc %D6,%D7         \n\t"

	"brcc L_B_%=         \n\t"		//   If result negative
									//   Then
	"add %A6,%A7         \n\t"		//     Restore Remainder
	"adc %B6,%B7         \n\t"
	"adc %C6,%C7         \n\t"
	"adc %D6,%D7         \n\t"

	"clc                 \n\t"		//     Partial_result = 0
	"rjmp L_C_%=         \n\t"

"L_B_%=:                 \n\t"		//   Else
	"sec                 \n\t"		//     Partial_result = 1

"L_C_%=:                 \n\t"		//   End If
	"dec %5              \n\t"		// Until(--cnt == 0)
	"brne L_A_%=         \n\t"

	"adc %0,__zero_reg__ \n\t"		// Round by the last bit of RFREQ
	"adc %1,__zero_reg__ \n\t"
	"adc %2,__zero_reg__ \n\t"
	"adc %3,__zero_reg__ \n\t"
	"adc %4,__zero_reg__ \n\t"

"L_X_%=:               \n\t"

	// Output operand list
	//--------------------
	: "=r" (Si570_Data.RFREQ.w1.b1) // %0 -> Dividend_40
	, "=r" (Si570_Data.RFREQ.w1.b0) // %1        "
	, "=r" (Si570_Data.RFREQ.w0.b1) // %2        "
	, "=r" (Si570_Data.RFREQ.w0.b0) // %3        "     LSB
	, "=r" (RFREQ_b4)               // %4        "     MSB
	
	// Input operand list
	//-------------------
	: "r" (cnt)                     // %5 -> Loop_Counter
	, "r" (RR)                      // %6 -> Remainder_40
	, "r" (R.FreqXtal)              // %7 -> Divisor_32
	, "0" (RFREQ.w0.b0)
	, "1" (RFREQ.w0.b1)
	, "2" (RFREQ.w1.b0)
	, "3" (RFREQ.w1.b1)
	, "4" (RFREQ_b4)
	);

	// Si570_Data.RFREQ_b4 will be sent to register_8 in the Si570
	// register_8 :  76543210
	//               ||^^^^^^------< RFREQ[37:32]
	//               ^^------------< N1[1:0]
	Si570_Data.RFREQ_b4  = RFREQ_b4;
	Si570_Data.RFREQ_b4 |= (sN1 & 0x03) << 6;
}

#ifdef INCLUDE_SMOOTH

static bool
Si570_Small_Change(uint32_t current_Frequency)
{
	uint32_t delta_F, delta_F_MAX;
	sint32_t previous_Frequency;

	// Get previous_Frequency   -> [11.21]
	previous_Frequency.dw = FreqSmoothTune;

	// Delta_F (MHz) = |current_Frequency - previous_Frequency|  -> [11.21]
	delta_F = current_Frequency - previous_Frequency.dw;
	if (delta_F >= _2(31)) delta_F = 0 - delta_F;

	// Delta_F (Hz) = (Delta_F (MHz) * 1_000_000) >> 16 not possible, overflow
	// replaced by:
	// Delta_F (Hz) = (Delta_F (MHz) * (1_000_000 >> 16)
	//              = Delta_F (MHz) * 15  (instead of 15.258xxxx)
	// Error        = (15 - 15.258) / 15.258 = 0.0169 < 1.7%

	delta_F = delta_F * 15;          // [27.5] = [11.21] * [16.0]

	// Compute delta_F_MAX (Hz)= previous_Frequency(MHz) * 3500 ppm
	delta_F_MAX = (uint32_t)previous_Frequency.w1.w * R.SmoothTunePPM;
	//   [27.5] =                          [11.5] * [16.0]

	// return TRUE if output changes less than �3500 ppm from the previous_Frequency
	return (delta_F <= delta_F_MAX);

} // end Si570_Small_Change

#endif

void
SetFreq(uint32_t freq)		// frequency [MHz] * 2^21
{
	R.Freq = freq;

#ifdef INCLUDE_SMOOTH

	if ((R.SmoothTunePPM != 0) && Si570_Small_Change(freq))
	{
		Si570CalcRFREQ(freq);
		Si570Write();
	}
	else
	{
		FreqSmoothTune = freq;

		if (Si570CalcDivider())
		{
			Si570CalcRFREQ(freq);
			Si570Load();
		}
	}

#else

	if (Si570CalcDivider())
	{
		Si570CalcRFREQ(freq);
		Si570Load();
	}

#endif

#ifdef INCLUDE_ABPF
	SetFilter();
#endif
}

void
DeviceInit(void)
{
	// Check if Si570 is online and intialize if nessesary
	// SCL Low is now power on the SI570 chip in the Softrock V9
	if ((I2C_PIN & _BV(BIT_SCL)) == 0)
	{
		SI570_online = false;
	} 
	else if (!SI570_online)
	{
#ifdef INCLUDE_SMOOTH
		FreqSmoothTune = 0;				// Next SetFreq call no smoodtune
#endif
		SetFreq(R.Freq);

		SI570_online = !I2CErrors;
	}
}

static bool
Si570CmdStart(uint8_t cmd)
{
	I2CSendStart();
	I2CSendByte(R.ChipCrtlData<<1);		// send device address 
	if (!I2CErrors)
	{
		I2CSendByte(cmd);				// send Byte Command
		return true;
	}
	return false;
}

void
Si570CmdReg(uint8_t reg, uint8_t data)
{
	if (Si570CmdStart(reg))
	{
		I2CSendByte(data);
	}
	I2CSendStop();
}

static void
Si570NewFreq(void)
{
	Si570CmdReg(135, 0x40);
}

static void
Si570FreezeNCO(void)
{
	Si570CmdReg(137, 0x10);
}

static void
Si570UnFreezeNCO(void)
{
	Si570CmdReg(137, 0x00);
}

// write all registers in one block.
static void
Si570Write(void)
{
	if (Si570CmdStart(7))				// send Byte address 7
	{
		uint8_t i;
		for (i=0;i<6;i++)				// all 5 registers
			I2CSendByte(Si570_Data.bData[i]);// send data 
	}
	I2CSendStop();
}

// read all registers in one block to replyBuf[]
uint8_t
GetRegFromSi570(void)
{
	if (Si570CmdStart(7))				// send Byte address 7
	{
		uint8_t i;
		I2CSendStart();	
		I2CSendByte((R.ChipCrtlData<<1)|1);
		for (i=0; i<5; i++)
			((uint8_t*)replyBuf)[i] = I2CReceiveByte();
		((uint8_t*)replyBuf)[5] = I2CReceiveLastByte();
	}
	I2CSendStop(); 

	return I2CErrors ? 0 : sizeof(Si570_t);
}

static void
Si570Load(void)
{
	Si570FreezeNCO();
	if (!I2CErrors)
	{
		Si570Write();
		Si570UnFreezeNCO();
		Si570NewFreq();
	}
}

#endif
