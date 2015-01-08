/***********************************************
 * Sega Graphics Board Proto v2.0 Reproduction *
 * Firmware for ATTiny48                       *
 * Rev. 0.1 by Jeff "ishiyakazuo" Stenhouse    *
 * 7 January 2015                              *
 ***********************************************/

#include <avr/io.h>
//#include <util/delay.h>

// Buttons are PB0, PB1, PB2, PB7, so this shifts the data into the lower nybble and inverts them.
#define GET_BUTTONS(a) (( (a & 0x07) | ( (a & 0x80) >> 4) ) ^ 0x0F)

//#define delay(a) _delay_loop_2(a << 3)
unsigned char touchdata[6] = {0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00};

int xZero;
int xMax;
int yZero;
int yMax;
unsigned char flipXY;

void setupADC(void)
{
  // Set up ADC clock to be between 50 and 200kHz
  ADCSRA = 0x06; // Divide clock by 2^6 (64) to make 8MHz system clock into 125kHz ADC clock
  ADMUX = (1 << REFS0); // Use AVcc as reference
  ADCSRA |= (1 << ADEN); // Enable the ADC block to be ready to read analog signals
}

int readADC(char adcNum)
{
  ADMUX = (1 << REFS0) | adcNum;
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC)) {;} // Wait for conversion to complete
  return ADCW;
}

// ADC (touchscreen) is PC0-PC3
char senseTouch(void)
{
  DDRC |= (1 << 2); // Set pin PC2 as output
  PORTC |= (1 << 3); // Enable PC3 pullup
  PORTC &= ~((1 << 0) | (1 << 1) | (1 << 2)); // Set PC2, PC1 and PC0 low (and/or disable any pullups)
  DDRC &= ~((1 << 0) | (1 << 1) | (1 << 3)); // Set pins PC0, PC1 and PC3 as inputs
  if (readADC(3) < 800) return 1; // Read PC3 (ADC3).  If it's low, you've got a touch!
  return 0;
}

int readX(void)
{
  DDRC |= (1 << 2) | (1 << 0); // Set pins PC2 and PC0 as outputs
  DDRC &= ~((1 << 3) | (1 << 1)); // Set pins PC3 and PC1 as inputs
  PORTC &= ~((1 << 3) | (1 << 1) | (1 << 0)); // Set PC3, PC1 and PC0 low (and/or disable any pullups)
  PORTC |= 1 << 2; // Set PC2 high
  return readADC(3); // Read PC3 (ADC3)
}

int readY(void)
{
  DDRC |= (1 << 3) | (1 << 1); // Set pins PC3 and PC1 as outputs
  DDRC &= ~((1 << 2) | (1 << 0)); // Set pins PC2 and PC0 as inputs
  PORTC &= ~((1 << 3) | (1 << 2) | (1 << 0)); // Set PC3, PC2 and PC0 low (and/or disable any pullups)
  PORTC |= 1 << 1; // Set PC1 high
  return readADC(2); // Read PC2 (ADC2)
}

void getTouchValue(void)
{
  long workingVar;
  // Read X value
  if (flipXY)
  {
    // read from the opposite touch pair
    workingVar = readY();
  }
  else
  {
    // normal read
    workingVar = readX();
  }
  // Scale X value to 0-255 SMS space
  workingVar -= xZero;
  workingVar *= 510; // 255*2, for rounding;
  workingVar /= (xMax-xZero);
  // Round to the nearest whole by integer math here
  workingVar += 1;
  workingVar /= 2;
  if (workingVar < 0) workingVar = 0;
  if (workingVar > 255) workingVar = 255;
  // Write the nybbles into touchdata[2] and touchdata[3] (high, then low)
  touchdata[2] = (workingVar >> 4) & 0x0F;
  touchdata[3] = workingVar & 0x0F;
	
  // Read Y value
  if (flipXY)
  {
    // read from the opposite touch pair
    workingVar = readX();
  }
  else
  {
    // normal read
    workingVar = readY();
  }
  // Scale Y value to 0-255 SMS space
  workingVar -= yZero;
  workingVar *= 510; // 255*2, for rounding;
  workingVar /= (yMax-yZero);
  // Round to the nearest whole by integer math here
  workingVar += 1;
  workingVar /= 2;
  if (workingVar < 0) workingVar = 0;
  if (workingVar > 255) workingVar = 255;
  // Write the nybbles into touchdata[4] and touchdata[5] (high, then low)
  touchdata[4] = (workingVar >> 4) & 0x0F;
  touchdata[5] = workingVar & 0x0F;
}

int main( void )
{
  unsigned char prevTH = 0x08;
  unsigned char* nextValue; // Pointer to the next nybble to send out on D0-D3 when touch data is available.

  // Data lines D0-D3 are PD0-PD3 (coincidence?  Nope.)
  // TL is PD4
  // By default, let's set TL/data 0-3 high, so no buttons are being pressed and no data is showing.
  DDRD = 0x1F; // Sets pins 0-4 of PD as outputs
  PORTD = 0x10;  // Sets pins 0-3 of PD low, and PD4 (TL) high

  // TH is PA3
  // TR is PA2
  // Configure TH/TR as inputs.
  DDRA &= ~((1 << 2) | (1 << 3)); // Sets PA2,3 as inputs
  PORTA |= ((1 << 2) | (1 << 3)); // Eables pullups on pins PA2,3 (just in case)
  
  // Button pin configuration. (Buttons are PB0, PB1, PB2, PB7)
  DDRB &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 7)); // Sets PB0,1,2,7 as inputs
  PORTB |= ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 7)); // Eables pullups on pins PB0,1,2,7
  // To read buttons as a single nybble, call GET_BUTTONS(PINB), which will shift PB7 down to bit 3 for you
  
  setupADC();
  
  while (1) // Loop forever!
  {
    // Read the buttons and write them to D0-D3.
    PORTD = 0x10 | GET_BUTTONS(PINB); // TL will be high here because we don't have a touch value (TL is included in the 0xF0)
	
	if (senseTouch())
	{ // Welcome to the fantasy zone.  Get ready...
	  getTouchValue();
	  prevTH = 0x08; // TH will be high here, so we want "prevTH" to be high so it doesn't see a toggle.
	  nextValue = &(touchdata[0]); // Reset our pointer to the first nybble of touch data
	  PORTD &= 0xEF; // Set TL low.
	  while (PINA & 0x04); // Wait for TR to go low...
	  while ((PINA & 0x04) == 0) // Now that it's low...
	  {
	    if (prevTH ^ (PINA & 0x08)) // Maybe not the easiest way to check for a change on TH?
		{ // TH toggled, write out the next value and prepare for the next toggle.
		  PORTD = *nextValue;
		  nextValue++;
		  prevTH ^= 0x08;
		}
	  }
	}
  }
  
  return 0;
}
