/*
 * Project.c
 *
 * Created: 10/20/2020 6:12:36 AM
 * Author : Stephanie Taylor
 *
 * Simple Washing Machine Controller
 *
 * IO Board Connections
 *		Seven Segment Display A to G = AVR Port A, Pin 0 to Port A, Pin 6
 *		Seven Segment Display CC = AVR Port A, Pin 7
 *
 *		Switch S0	= AVR Port D, Pin 0 ()
 *		Switch S1	= AVR Port D, Pin 1 ()
 *		Switch S2	= AVR Port D, Pin 4 ()
 *
 *		Button B0	= AVR Port D, Pin 2 ()
 *		Button B1	= AVR Port D, Pin 3 ()
 *
 *		LED L0		= AVR Port B, Pin 0 ()
 *		LED L1		= AVR Port B, Pin 1 ()
 *		LED L2		= AVR Port B, Pin 2 ()
 *		LED L3		= AVR Port B, Pin 3 ()
 *
 *		LED L7		= AVR Port B, Pin 4 () (OC0B Pin)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 8000000UL	// 8 MHz


void start();
void reset();
void washing();
void rinsing();
void spinning();


/*
CONTROL VARIABLES
*/

/*
OPERATIONAL MODE
	0 = Normal
	1 = Extended
	
	Normal Mode:   Wash > Rinse > Spin
	Extended Mode: Wash > Rinse > Rinse > Spin
	
	Initially: Normal
*/
volatile uint8_t operational_mode = 0;

/*
INITIAL OPERATIONAL MODE
	0 = Normal
	1 = Extended
	
	Initially: Normal
*/
volatile uint8_t initial_operational_mode = 0;

/*
WATER LEVEL
	0 = Low
	2 = Medium
	1 = High
	3 = Error
	
	Initially: Low
*/
volatile uint8_t water_level = 0;

/*
INITIAL WATER LEVEL
	0 = Low
	2 = Medium
	1 = High
	3 = Error
	
	Initially: Low
*/
volatile uint8_t initial_water_level = 0;

/*
RUNNING
	1 = Running
	0 = NOT Running
	
	Initially: NOT Running
*/
volatile uint8_t running = 0;

/*
WASH
	1 = Washing
	0 = NOT Washing
	
	Initially: NOT Washing
*/
volatile uint8_t wash = 0;

/*
RINSE
	1 = Rinsing
	0 = NOT Rinsing
	
	Initially: NOT Rinsing
*/
volatile uint8_t rinse = 0;

/*
RINSE CYCLES COMPLETED
	Normal = 1
	Extended = 2
	
	Initially: 0
*/
volatile uint8_t rinse_cycles_completed = 0;

/*
SPIN
	1 = Spinning
	0 = NOT Spinning
	
	Initially: NOT Spinning
*/
volatile uint8_t spin = 0;

/*
FINISHED
	1 = Finished
	0 = NOT Finished
	
	Initially: NOT Finished
*/
volatile uint8_t finished = 0;

/*
DUTY CYCLE
*/
volatile double duty_cycle = 100; // 100% Less 0% (Inverting Mode)

/*
COUNT
Count Thousandths of Seconds
*/
volatile uint32_t count = 0;

/*
SEVEN SEGMENT DISPLAY (SSD) VALUES
							G F E D C B A
	Water Level: Low		0 0 0 1 0 0 0 = 8
	Water Level: Medium		1 0 0 0 0 0 0 = 64
	Water Level: High		0 0 0 0 0 0 1 = 1
	Water Level: Error		1 1 1 1 0 0 1 = 121
	Mode: Normal			1 0 1 0 1 0 0 = 84
	Mode: Extended			1 1 1 1 0 0 1 = 121
	Finished				0 1	1 1 1 1 1 = 63
*/
uint8_t ssd_values[7] = {8, 1, 64, 121, 84, 121, 63};

/*
SEVEN SEGMENT DISPLAY (SSD) DIGIT (CC)
	1 = LEFT
	0 = RIGHT
*/
volatile uint8_t ssd_cc = 0;



int main(void){
	/*
	Inputs:
		Switch S0 = AVR Port D, Pin 0 ()
		Switch S1 = AVR Port D, Pin 1 ()
		Switch S2 = AVR Port D, Pin 4 ()
	*/
	DDRD = (0 << 0) | (0 << 1) | (0 << 4); 
	
    /*
    Outputs:
	 	Seven Segment Display A to G = AVR Port A, Pin 0 to Port A, Pin 6
		Seven Segment Display CC = AVR Port A, Pin 7
		
		LED 0 = AVR Port B, Pin 0 ()
		LED 1 = AVR Port B, Pin 1 ()
		LED 2 = AVR Port B, Pin 2 ()
		LED 3 = AVR Port B, Pin 3 ()
		
		LED 7 = AVR Port B, Pin 4 ()
	*/
	DDRA = 0xFF;
	DDRB = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
   
   
	/*
	TIMER COUNTER 0
	Fast Pulse Width Modulation, Pin OC0B
	Set OC0B on Compare Match, Clear OC0B at BOTTOM (Inverting Mode)
	*/
	TCCR0A = (1 << COM0B1) | (1 << COM0B0) | (1 << WGM01) | (1 << WGM00);
	TCCR0B = (0 << CS02) | (0 << CS01) | (1 << CS00) | (0 << WGM02);
	
	// Enable Timer Counter Overflow Interrupt
	TIMSK0 = (1 << TOIE0);
	
	// Ensure Interrupt Flag is Cleared
	TIFR0 = (1 << TOV0);
	
	
    /*
    TIMER COUNTER 1
	Interrupt 1000 Times Per Second
	CS = CLK/8
    */
	OCR1A = 999;
    TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << WGM11) | (0 << WGM10);
    TCCR1B = (0 << CS12) | (1 << CS11) | (0 << CS10) | (0 << WGM13) | (1 << WGM12);
	
	// Enable Timer Counter Output Compare A Match Interrupt
	TIMSK1 = (1 << OCIE1A);
	
	// Ensure Interrupt Flag is Cleared
	TIFR1 = (1 << OCF1A);
	
	
	// Set Interrupt on FALLING Edge Pin D2 (START Button)
	EICRA = (1 << ISC01) | (0 << ISC00);
	EIMSK = (1 << INT0);
	EIFR = (1 << INTF0);

	// Set Interrupt on FALLING Edge Pin D3 (RESET Button)
	EICRA |= (1 << ISC11) | (0 << ISC10);
	EIMSK |= (1 << INT1);
	EIFR = (1 << INTF1);
	
	
	// Turn on Global Interrupts
	sei();
	
	
	while(1) {
		// Running
		if(running) {
			// Washing
			if(wash)  washing();
			
			// Rinsing
			if(rinse) rinsing();
			
			// Spinning
			if(spin)  spinning();
		}
		
		// NOT Running
		else {
			// L0-L3 OFF 
			PORTB = 0x0;
		}

		// Change to Operational Mode OR Water Level in the Operational State OR Finished State
		if((operational_mode != initial_operational_mode) | (water_level != initial_water_level)) {
			reset();
		}
	}
}


/*
Interrupt Handler for Event on Pin D2 (START Button)
External Interrupt 0
*/
ISR(INT0_vect) {
	start();
}


/*
Interrupt Handler for Event on Pin D3 (RESET Button) 
External Interrupt 1
*/
ISR(INT1_vect) {
	reset();
}


/*
Interrupt Handler 1000 Times Per Second
*/
ISR(TIMER1_COMPA_vect) {
	/*
	Increment Time
	*/
	if(running) count++;
	
	/*
	Input Variables (S0, S1)
	*/
	water_level = PIND & 0x03;
	operational_mode = PIND & 0x10;
	
	/* 
	Toggle Seven Second Display (SSD) Digit
	*/
	ssd_cc ^= 1;
	
	/* 
	Seven Second Display (SSD)
	*/
	if(ssd_cc) {
		// Display LEFT Digit
		
		// EXTENDED, Display E
		if(operational_mode) {
			PORTA = ssd_values[5];
		}
		// NORMAL, Display N
		else {
			PORTA = ssd_values[4];
		}
		
		// FINISHED, Display 0
		if(finished) {
			PORTA = ssd_values[6];
		}
		
		// Output LEFT Digit = 1
		PORTA |= (1 << 7);
	}
	
	else {
		// Display RIGHT Digit
		
		// FINISHED, Display 0
		if(finished) {
			PORTA = ssd_values[6];
		}
		
		// Otherwise Display Water Level
		else {
			PORTA = ssd_values[water_level];
		}
		
		// Output RIGHT Digit = 0
		PORTA &= ~(1 << 7);
	}
}


/*
Interrupt Handler PWM
*/
ISR(TIMER0_OVF_vect) {
	// Calculate Duty Cycle
	OCR0B = (duty_cycle / 100) * 255;
}



// START Function
void start(void) {
	// Water Level = Error
	if (water_level == 3) {
		// Do Nothing
	}
	
	// Running
	else if (running) {
		// Do Nothing
	}
	
	// NOT Running
	else {
		// Start Running, Start Washing
		running = 1;
		wash = 1;
		
		// Set Count to Zero
		count = 0;
		
		// NOT Finished
		finished = 0;
		
		// Rinse Cycles Completed
		rinse_cycles_completed = 0;
		
		// Store Initial Operational Mode, Initial Water Level
		initial_operational_mode = operational_mode;
		initial_water_level = water_level;
	}
}

// RESET Function
void reset(void) {
	// NOT Running
	running = 0;

	// NOT Washing, Rinsing, Spinning
	wash = 0; rinse = 0; rinse_cycles_completed = 0; spin = 0;

	// Set Duty Cycle
	duty_cycle = 100; // 100% Less 0% = 100% (Inverting Mode)

	// NOT Finished
	finished = 0;
}


// WASHING Function
void washing(void) {
	// Set Duty Cycle
	duty_cycle = 90; // 100% Less 10% (Inverting Mode)
				
	/*
	Running Pattern RIGHT to LEFT for 3s
	L0-L3 ON for 3s
	*/
				
	// L0 ON
	if(count < 350) {				// 0 ms to 350 ms
		PORTB = (1 << 0);
	}
	// L1 ON, L0 OFF
	else if(count < 700) {			// 350 ms to 700 ms
		PORTB = (0 << 0);
		PORTB = (1 << 1);
	}
	// L2 ON, L1 OFF
	else if(count < 1050) {			// 700 ms to 1050 ms
		PORTB = (0 << 1);
		PORTB = (1 << 2);
	}
	// L3 ON, L2 OFF
	else if(count < 1400) {			// 1050 ms to 1400 ms
		PORTB = (0 << 2);
		PORTB = (1 << 3); 
	}
	// L0-L3 OFF
	else if(count < 1500) {			// 1400 ms to 1500 ms
		PORTB = 0x0;
	}
				
	// L0 ON, L3 OFF
	else if(count < 1850) {			// 1500 ms to 1850 ms
		PORTB = (0 << 3);
		PORTB = (1 << 0);
	}
	// L1 ON, L0 OFF
	else if(count < 2200) {			// 1850 ms to 2200 ms
		PORTB = (0 << 0);
		PORTB = (1 << 1);
	}
	// L2 ON, L1 OFF
	else if(count < 2550) {			// 2200 ms to 2550 ms
		PORTB = (0 << 1);
		PORTB = (1 << 2);
	}
	// L3 ON, L2 OFF
	else if(count < 2900) {			// 2550 ms to 2900 ms
		PORTB = (0 << 2);
		PORTB = (1 << 3); 
	}
	// L0-L3 OFF
	else if(count < 3000) {			// 2900 ms to 3000 ms
		PORTB = 0x0;
	}
				
	// L0-L3 0N
	else if(count < 6000) {			// 3000 ms to 6000 ms
		PORTB = 0xF;
	}
				
	else {
		// L0-L3 OFF
		PORTB = 0x00;
					
		// Stop Washing, Start Rinsing
		wash = 0;
		rinse = 1;
					
		// Set Count to Zero
		count = 0;
	}
}


// RINSING Function
void rinsing(void) {
	// Set Duty Cycle
	duty_cycle = 50; // 100% Less 50% (Inverting Mode)

	/*
	Running Pattern LEFT to RIGHT for 3s
	L0-L3 Blinking ON/OFF for 3s
	*/
	
	// L3 ON
	if(count < 350) {				// 0 ms to 350 ms
		PORTB = (1 << 3);
	}				
	// L2 ON, L3 OFF
	else if(count < 700) {			// 350 ms to 700 ms
		PORTB = (0 << 3);
		PORTB = (1 << 2);
	}
	// L1 ON, L2 OFF
	else if(count < 1050) {			// 700 ms to 1050 ms
		PORTB = (0 << 2);
		PORTB = (1 << 1);
	}
	// L0 ON, L1 OFF
	else if(count < 1400) {			// 1050 ms to 1400 ms
		PORTB = (0 << 1);
		PORTB = (1 << 0);
	}
	// L0-L3 OFF
	else if(count < 1500) {			// 1400 ms to 1500 ms
		PORTB = 0x0;
	}
				
	// L3 ON, L0 OFF
	else if(count < 1850) {			// 1500 ms to 1850 ms
		PORTB = (0 << 0);
		PORTB = (1 << 3);
	}
	// L2 ON, L3 OFF
	else if(count < 2200) {			// 1850 ms to 2200 ms
		PORTB = (0 << 3);
		PORTB = (1 << 2);
	}
	// L1 ON, L2 OFF
	else if(count < 2550) {			// 2200 ms to 2550 ms
		PORTB = (0 << 2);
		PORTB = (1 << 1);
	}				
	// L0 ON, L1 OFF
	else if(count < 2900) {			// 2550 ms to 2900 ms
		PORTB = (0 << 1);
		PORTB = (1 << 0);
	}
	// L0-L3 OFF
	else if(count < 3000) {			// 2900 ms to 3000 ms
		PORTB = 0x0;
	}

	// Blinking Rate = 500 ms
	// L0-L3 Blinking 0N
	else if(count < 3500) {			// 3000 ms to 3500 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 4000) {			// 3500 ms to 4000 ms
		PORTB = 0x0;
	}
	// L0-L3 Blinking 0N
	else if(count < 4500) {			// 4000 ms to 4500 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 5000) {			// 4500 ms to 5000 ms
		PORTB = 0x0;
	}
	// L0-L3 Blinking 0N
	else if(count < 5500) {			// 5000 ms to 5500 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 6000) {			// 5500 ms to 6000 ms
		PORTB = 0x0;
	}
				
	else {
		// Increment Rinse Cycles Completed
		rinse_cycles_completed += 1;

		if(operational_mode) {
			// Extended Mode
			// Rinse Cycle Completed = 1
			if(rinse_cycles_completed == 1) {
				// Rinse Again
				// Set Count to Zero
				count = 0;
			}
			// Rinse Cycles Completed = 2
			if(rinse_cycles_completed == 2) {
				// Stop Rinsing, Start Spinning
				rinse = 0;
				spin = 1;
							
				// Set Count to Zero
				count = 0;
			}
		}
					
		else {
			// Normal Mode
			// Stop Rinsing, Start Spinning
			rinse = 0;
			spin = 1;
						
			// Set Count to Zero
			count = 0;
		}
	}
}


// SPINNING Function
void spinning(void) {
	// Set Duty Cycle
	duty_cycle = 10; // 100% Less 90% (Inverting Mode)
				
	/*
	Running Pattern LEFT to RIGHT and RIGHT to LEFT for 3s
	L0-L3 Blinking ON/OFF for 3s (Double Rate)
	*/
				
	// L3 ON
	if(count < 375) {				// 0 ms to 350 ms
		PORTB = (1 << 3);
	}
	// L2 ON, L3 OFF
	else if(count < 750) {			// 350 ms to 700 ms
		PORTB = (0 << 3);
		PORTB = (1 << 2);
	}
	// L1 ON, L2 OFF
	else if(count < 1125) {			// 700 ms to 1050 ms
		PORTB = (0 << 2);
		PORTB = (1 << 1);
	}
	// L0 ON, L1 OFF
	else if(count < 1350) {			// 1050 ms to 1400 ms
		PORTB = (0 << 1);
		PORTB = (1 << 0);
	}
	// L0-L3 OFF
	else if(count < 1500) {			// 1400 ms to 1500 ms
		PORTB = 0x0;
	}
				
	// L0 ON
	else if(count < 1875) {			// 1500 ms to 1850 ms
		PORTB = (0 << 3);
		PORTB = (1 << 0);
	}
	// L1 ON, L0 OFF
	else if(count < 2250) {			// 1850 ms to 2200 ms
		PORTB = (0 << 0);
		PORTB = (1 << 1);
	}
	// L2 ON, L1 OFF
	else if(count < 2625) {			// 2200 ms to 2550 ms
		PORTB = (0 << 1);
		PORTB = (1 << 2);
	}
	// L3 ON, L2 OFF
	else if(count < 2850) {			// 2550 ms to 2900 ms
		PORTB = (0 << 2);
		PORTB = (1 << 3);
	}
	// L0-L3 OFF
	else if(count < 3000) {			// 2900 ms to 3000 ms
		PORTB = 0x0;
	}

	// Blinking Rate = 250 ms
	// L0-L3 Blinking 0N
	else if(count < 3250) {			// 3000 ms to 3250 ms
		PORTB = 0xF;
	}					
	// L0-L3 Blinking OFF
	else if(count < 3500) {			// 3250 ms to 3500 ms
		PORTB = 0x0;
	}					
	// L0-L3 Blinking 0N
	else if(count < 3750) {			// 3500 ms to 3750 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 4000) {			// 3750 ms to 4000 ms
		PORTB = 0x0;
	}
	// L0-L3 Blinking 0N
	else if(count < 4250) {			// 4000 ms to 4250 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 4500) {			// 4250 ms to 4500 ms
		PORTB = 0x0;
	}
	// L0-L3 Blinking 0N
	else if(count < 4750) {			// 4500 ms to 4750 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 5000) {			// 4750 ms to 5000 ms
		PORTB = 0x0;
	}
	// L0-L3 Blinking 0N
	else if(count < 5250) {			// 5000 ms to 5250 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 5500) {			// 5250 ms to 5500 ms
		PORTB = 0x0;
	}
	// L0-L3 Blinking 0N
	else if(count < 5750) {			// 5500 ms to 5750 ms
		PORTB = 0xF;
	}
	// L0-L3 Blinking OFF
	else if(count < 6000) {			// 5750 ms to 6000 ms
		PORTB = 0x0;
	}

	else {
		// Stop Spinning, Stop Running
		spin = 0;
		running = 0;
					
		// Finished
		finished = 1;
					
		// Set Duty Cycle
		duty_cycle = 100; // 100% Less 10% (Inverting Mode)
	}
}