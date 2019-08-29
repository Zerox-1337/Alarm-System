/*
 * alarm_system.c
 *
 * Created: 2017-02-13 13:05:51
 * Author : Mattias Sonnerup & Adnan Mehmedagic
 */ 

#include <avr/io.h>
#define	F_CPU 1118000
#include<util/delay.h>
#include <stdio.h>
#include "hd44780.h" // Display lib.

#include "main.h"
#include <avr/interrupt.h>
#define row_one_start_index 40

int volatile state = 0;
int volatile larm_state = 0; // if the larm is triggered or not.
int volatile test_mode = 0; // Test mode variable. activates

char volatile code_entered_one = 'k'; // The code we enter. k = empty. 
char volatile code_entered_two = 'k';
char volatile code_entered_three = 'k';

const char code_correct_one = '1'; // The first number that needs to be entered.
const char code_correct_two = '5';
const char code_correct_three = '3';
int volatile minute_passed_ones = 3;
int volatile minute_passed_tens = 3;
int volatile hour_passed_ones = 6;
int volatile hour_passed_tens = 1;
/*******************************************************************
*
*						DISPLAY FUNCTIONS
*
********************************************************************/
void write_to_display(char *text, int line, int offset, int clear)
{
	PORTD |= _BV(PD7);// We are using A4-A5, D7 now for CS. Active low
	DDRB = 0xFF; // All B are outputs
	if (line > 0) {
		offset += 40;
	}
	if (clear == 1) lcd_init();
	lcd_goto(offset);
	lcd_puts(text);
	DDRB = 0x00; // All B are inputs again.
	PORTD &= ~_BV(PD7);// We are using A4-A5, D7 now for CS. Active low
}

void write_to_display_char(char text, int line, int offset, int clear)
{
	PORTD |= _BV(PD7);// We are using A4-A5, D7 now for CS. Active low
	DDRB = 0xFF; // All B are outputs
	if (line > 0) {
		offset += 40;
	}
	if (clear == 1) lcd_init();
	lcd_goto(offset);
	lcd_putc(text);
	DDRB = 0x00; // All B are inputs again.
	PORTD &= ~_BV(PD7);// We are using A4-A5, D7 now for CS. Active low
}
void display_time(void)
{
	write_to_display("Time: ", 1,0,0);
	write_to_display_char((char) (48 + hour_passed_tens), 1,6,0);
	write_to_display_char((char) (48 + hour_passed_ones), 1,7,0);
	write_to_display_char(':', 1, 8, 0);
	write_to_display_char((char) (48 + minute_passed_tens), 1,9,0);
	write_to_display_char((char) (48 + minute_passed_ones), 1,10,0);
}
void clear_display(int row)
{
	for (int i = 0; i < 16; i++)
	{
		write_to_display_char(' ', row, i, 0);
	}
}		
/*******************************************************************
*
*						LARM FUNCTIONS
*
********************************************************************/
void buzz() // For the siren
{
	PORTD |= _BV(PD4);
	_delay_ms(1);
	PORTD &= ~_BV(PD4);
	_delay_ms(1);
	
}

void larm_on(char larm_num){
	PORTA |= _BV(PA7); // diod on. 
	write_to_display("larm ", 0, 0, 0);
	write_to_display_char(larm_num, 0, 5, 0);
	write_to_display_char((char) (48 + hour_passed_tens), 0,7,0);
	write_to_display_char((char) (48 + hour_passed_ones), 0,8,0);
	write_to_display_char(':', 0, 9, 0);
	write_to_display_char((char) (48 + minute_passed_tens), 0,10,0);
	write_to_display_char((char) (48 + minute_passed_ones), 0,11,0);
	larm_state = 1; // The alarm is on. 
}
uint16_t adc_read()
{
	ADMUX = (ADMUX & 0xF8); //Selects PA0
	// start single convertion
    // write '1' to ADSC
	ADCSRA |= (1<<ADSC);
	// wait for conversion to complete
    // ADSC becomes '0' again
	// till then, run loop continuously
	while(ADCSRA & (1<<ADSC));
	return (ADC);
}
char check_larm(void) // REWRITE PORTS
{
	// ENABLE SIGNALS BUFFER ON others OFF.
	PORTA &= ~_BV(PA4);
	PORTA &= ~_BV(PA6);
	char larm = '0';
	uint16_t analog_level = adc_read();	
	if (test_mode == 1) // In testing mode you see the values for analog sensor
	{	  
		write_to_display_char((char) ((analog_level/100)+48), 1, 0, 0);
		write_to_display_char((char) (((analog_level % 100)/ 10) +48), 1, 1, 0);
		write_to_display_char((char) (((analog_level % 100)%10) + 48), 1, 2, 0);
	}
	if (PINB & (1<<PB0)) // If one of the digital sensor is on. 
	{
		larm = '1'; 
	}
	else if (PINB & (1<<PB1))
	{
		larm = '2';  
	}
	else if (PINB & (1<<PB2))
	{
		larm = '3'; 
	}
	else if (PINB & (1<<PB3))
	{
		PORTA |= _BV(PA7); 
		larm = '4'; 
	}
	else if (PINB & (1<<PB4))
	{
		larm = '5'; 
	}
	else if (PINB & (1<<PB5))
	{
		larm = '6'; 
	}
	else if (PINB & (1<<PB6))
	{
		larm = '7'; 
	}
	else if (PINB & (1<<PB7))
	{
		 larm = '8'; 
	}
	else if(analog_level < 580){
		larm = '9';
	}

	// DISABLE SIGNALS BUFFER off.
	PORTA |= _BV(PA4);
	PORTA |= _BV(PA6);
	return larm;
}	  
/*******************************************************************
*
*						KEYBOARD FUNCTIONS
*
********************************************************************/
char check_keyboard(void) // CHecks the keyboard and returns which key char it is.
{
	char key_pressed = 'k';	
	// A5 for keyboard enable. ~ flips other bits to 1
	if ((PIND & 0x40) == 0x40) // & ~ 0 If data available.
	{
		// ENABLE SIGNALS BUFFER ON others OFF.
		PORTA &= ~_BV(PA5); //0xdf; // 0b11011111
		if ((PINB & 0x0F) == 0x0F)
		{
			key_pressed = 'f';
		}		

		else if((PINB & 0x0F) == 0x0E)
		{
			key_pressed = 'e';
		}
		else if((PINB & 0x0F) == 0x0D)
		{
			key_pressed = 'd';
		}
		else if((PINB & 0x0F) == 0x0C)
		{
			key_pressed = 'c';
		}
		else if((PINB & 0x0F) == 0x0B)
		{
			key_pressed = 'b';
		}
		else if((PINB & 0x0F) == 0x0A)
		{
			key_pressed = 'a';
		}
		else if((PINB & 0x0F) == 0x09)
		{
			key_pressed = '9';
		}
		else if((PINB & 0x0F) == 0x08)
		{
			key_pressed = '8';
		}
		else if((PINB & 0x0F) == 0x07) 
		{
			key_pressed = '7';
		}
		else if((PINB & 0x0F) == 0x06)
		{
			key_pressed = '6';
		}
		else if((PINB & 0x0F) == 0x05)
		{
			key_pressed = '5';
		}
		else if((PINB & 0x0F) == 0x04)
		{
			key_pressed = '4';
		}
		else if((PINB & 0x0F) == 0x03)
		{
			key_pressed = '3';
		}
		else if((PINB & 0x0F) == 0x02)
		{
			key_pressed = '2';
		}
		else if((PINB & 0x0F) == 0x01)
		{
			key_pressed = '1';
		}
		else if ((PINB & 0x0F) == 0x00)
		{
			key_pressed = '0';
		}
		PORTA |= _BV(PA5); // disable keyboard				  				  				  				  				 
	}
	return key_pressed;	
}
/*******************************************************************
*
*						CLOCK FUNCTIONS
*
********************************************************************/
void enter_set_clock(void) // When going to the set clock state in the state machine.
{
	write_to_display("Clock settings", 0,0,0);
	write_to_display("Time: ", 1,0,0);
	display_time(); // Displays the time
}


void clock_init() // Config clock 
{
	// CTR
	TIMSK = _BV(OCIE1A); // Interrupt when the timer value for timer 1 is the same as A
	TCCR1A = _BV(WGM01); // Clears timer register on compare.
	TCNT1 = 0; // Reset the timer register for counting. 
	TCCR1B |= (1 << WGM12) | (1 << CS12) | (1 << CS10); // prescaling 1024
	OCR1A = 58688; // compare value
	sei(); // Enables everything for interrupts
}
ISR(TIMER1_COMPA_vect) 
{
	minute_passed_ones ++;	
	if (minute_passed_ones >= 10) // NOTE: '>=' used instead of '=='
	{
		minute_passed_ones = 0;
		minute_passed_tens ++;
		if (minute_passed_tens >= 6)
		{
			minute_passed_tens = 0;
			hour_passed_ones ++;
			if (hour_passed_ones >= 10 && hour_passed_tens < 2)
			{
				hour_passed_ones = 0;
				hour_passed_tens ++;
			}
			else if (hour_passed_ones >= 4 && hour_passed_tens >= 2)
			{
				hour_passed_tens = 0; // reset time
				hour_passed_ones = 0; // reset time.
			}
		}

	}

}
/*******************************************************************
*
*						STATE FUNCTIONS
*
********************************************************************/
void larm_off(void)
{
	code_entered_one = 'k';
	code_entered_two = 'k';
	code_entered_three = 'k';
	test_mode = 0;
	larm_state = 0; // The alarm gets turned off.
	PORTA &= ~_BV(PA7); // led turned off.
	clear_display(0);
	clear_display(1);
}
/*******************************************************************
*
*						MAIN 
*
********************************************************************/

   
   // Analog signals: A0-A3
   // Data bus: B0-B7. (Digitala signaler in, Display out, Knappsats in 4 bits)
   // CS : A4 - A7 (out). A4 = Buffer enable, A5 = keyboard enable. D7 = Display Enable
   // DA : D6 (in).
   // Diod: A7 (out)
   // Display extra: D5 (out)
   // Larm central: D0 (in), D1 (out)
   // Siren: D4 (out)
   // latch enable buffer: A6 (out)


	   
int main(void)
{
	clock_init(); // Configs everything with the 1 minute timer
	lcd_init(); //
	   
	DDRA = 0xF0; 
    /*A0 - A3 = Analoga in. 
    A4 = Enable buffer out, 
    A5= Enable Keyboard out, 
    D7 = enable display out
    */
	DDRB = 0x00; // All data buss ports are inputs as default.
		 
	DDRD |= (1 << PD7)  | (1 << PD5) | (1 << PD4) | (1 << PD1); 
    /* D7 = Display EN 
    D5 = Display RS. 
    D4= Siren. 
    D1= Larm central ut. 
    D0 = Larm central in
    */
    
	PORTA |= (1 << PA4);
	PORTA |= (1 << PA5);
	PORTD &= ~_BV(PD7);
	ADMUX = (1<<REFS0);
	ADMUX = ADMUX & 0xf8;
	ADCSRA = (1<<ADEN) | (1<<ADPS2);

	while(1)
	{	
		if (test_mode == 1) write_to_display_char('T', 1, 15, 0);
		else write_to_display_char('N', 1, 15, 0);
			
		char volatile larm_num = '0'; // By default. 
        /* The alarm has not been triggered for the first time,since start or off code enter. 
        State 0 doesn't need to check the alarm either. 
        State 9 and up does not trigger the alarm either */
		if (larm_state == 0 && state != 0 && state <= 8) 
		{
			larm_num = check_larm();
		}
		if (larm_num != '0' && state != 0 && larm_state == 0)
		{
			clear_display(0); // clears row 0
			larm_on(larm_num);
		}
        if (larm_state == 1 && test_mode == 0) 
		{  // If the alarm is triggered and we are not testing.
			buzz(); // Activates the siren
		}
		char volatile key_num =check_keyboard();	
		switch (state)
		{
			case 0: // OFF
				write_to_display("OFF STATE", 0,0,0);
				if (key_num == 'c') // On button
				{
					state = 1; // On state.
				}
				else if (key_num == 'e'){ // test button
					state = 1;
					test_mode = 1; // Siren won't run
				}
				else if (key_num == 'f') // Set clk
				{
					enter_set_clock(); // Sets the text and displays the time.
					state = 9;
				}
				break;
			case 1: // state on
				if (larm_state == 0) write_to_display("Alarm enabled", 0,0,0); 

				if (larm_num != '0') // If alarm on. 
				{
					state = 2;
				}
				else if (key_num == 'd') // off button
				{
					clear_display(1); // clears row 0
					write_to_display("code: ", 1, 0, 0);
					if (test_mode == 0) state = 3; // GO to enter keys.
					else
					{
						state = 0;
						larm_off();
					}
				}
				break;
					
			case 2: // Larm is on state
				if (key_num == 'd') // off button
				{
					clear_display(1); // clears row 0
					write_to_display("code: ", 1, 0, 0);
					if (test_mode == 0) state = 3; // GO to enter keys.
					else 
					{
						state = 0;
						larm_off();
					}
				}
				break;
					
			case 3: // HOld state
				if (key_num == 'k'){
					state = 4;
				}					
				break;
					
			case 4: // key 1 wait
				if(key_num >= '0' && key_num <= '9'){ // 0-0
						
					code_entered_one = key_num;
					write_to_display_char(key_num, 1, 6, 0);
					state = 5;
				}
				break;
					
			case 5: // key 1 hold
				if (key_num == 'k'){
					state = 6;
				}
				break;
					
			case 6: // key 2 wait
				if(key_num >= '0' && key_num <= '9'){
					code_entered_two = key_num;
					write_to_display_char(key_num, 1, 7, 0);
					state = 7;
				}
				break;
					
			case 7: // key 2 hold
				if (key_num == 'k'){
					state = 8;
				}
				break;
				
			case 8: // key 3 wait
				if(key_num >= '0' && key_num <= '9'){
					code_entered_three = key_num;
					write_to_display_char(key_num, 1, 8, 0);
					if (code_entered_one == code_correct_one 
                    	&& code_entered_two == code_correct_two
                    	&& code_entered_three == code_correct_three )
					{
						state = 0;
						larm_off();
					}
					else
					{
						clear_display(1);
						write_to_display("Wrong code", 1,0,0);
						if (larm_state == 1) state = 2; // The alarm is running so back to state 2.
						else state = 1; // The alarm has not yet been triggered. Going back. 
					}
				}					
				break;
			case 9: // Hold set clk button
				if (key_num == 'k'){
					state = 10;
				}
				break;
			case 10: // Set clk hour (tens)
				if (key_num >= '0' && key_num <= '2')
				{
					hour_passed_tens = (int) (key_num - '0');
					display_time();
					state = 11;
				}
				break;
					
			case 11: // Hold
				if (key_num == 'k'){
					state = 12;
				}
			break;
				
			case 12: // set clk hour (ones)
				if ((hour_passed_tens < 2 && key_num >= '0' && key_num <= '9') 
                	|| (hour_passed_tens == 2 && key_num >= '0' && key_num <= '3' ))
				{
					hour_passed_ones = (int) (key_num - '0');
					clear_display(1);
					display_time();
					state = 13;
						
				}				
				break;
					
			case 13: // Hold
				if (key_num == 'k'){
					state = 14;
				}
			break;
				
			case 14: // Set clk minutes (tens)
				if (key_num >= '0' && key_num <= '5')
				{
					minute_passed_tens = (int) (key_num - '0');
					clear_display(1);
					display_time();
					state = 15;
						
				}					
				break;
					
			case 15: // Hold
				if (key_num == 'k'){
					state = 16;
				}
				break;
					
			case 16: // set clk minutes (ones)
				if (key_num >= '0' && key_num <= '9')
				{
					minute_passed_ones = (int) (key_num - '0');
					clear_display(0);
					clear_display(1);
					state = 0;
				}										
				break;				
		}
	}
}
