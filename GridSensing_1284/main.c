/*
 * GridSensing_1284.c
 *
 * Created: 11/7/2016 11:58:40 AM
 * Author : Chandan
 */ 

#define F_CPU 8000000UL
#define BAUD 9600

#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <util/crc16.h>
#include "dht.h"
#include "DB18S20.h"
#include "UART_0.h"
#include "UART_1.h"
#include "i2c_master.h"

#define WRITEADD	0x30
#define READADD		0x31

#define DS2482_1	0x30

#define SHORTDETECT 0x04
#define PPD			0x02
#define STATUS_1WB	0x01

bool shortdet = false, ppdstatus = false;

#define SCL		100000L
#define TRUE	1
#define FALSE	0

uint8_t i=0,j=0;
uint16_t t=0;
char ch1[6],ch2[6],ch3[6],ch4[6],ch5[6],ch6[6],ch7[6],ch8[6],ch9[6];

uint8_t devicereset(){
	uint8_t rec;
	
	i2c_start(WRITEADD);
	i2c_write(0xF0);
	i2c_start(READADD);
	rec = i2c_read_nack();
	i2c_stop();
	
	if((rec & 0xF7) != 0x10){
		return 0;
	}
	else{
	return 1;}
}

uint8_t channelsel(int chan){
	uint8_t status, ch, ch_read;
	
	i2c_start(WRITEADD);
	i2c_write(0xC3);
	
	switch (chan)
	{
		default: case 0: ch = 0xF0; ch_read = 0xB8; break;
		case 1: ch = 0xE1; ch_read = 0xB1; break;
		case 2: ch = 0xD2; ch_read = 0xAA; break;
		case 3: ch = 0xC3; ch_read = 0xA3; break;
		case 4: ch = 0xB4; ch_read = 0x9C; break;
		case 5: ch = 0xA5; ch_read = 0x95; break;
		case 6: ch = 0x96; ch_read = 0x8E; break;
		case 7: ch = 0x87; ch_read = 0x87; break;
	};
	
	i2c_write(ch);
	i2c_start(READADD);
	status = i2c_read_nack();
	i2c_stop();
	
	if (status != ch_read)
	{
		return 0;
	}
	else{
		return 1;
	}
}

uint8_t writecfg(uint8_t config){
	uint8_t rec;
	
	i2c_start(WRITEADD);
	i2c_write(0xD2);
	i2c_write(config|(~(config)<<4));
	i2c_start(READADD);
	rec = i2c_read_nack();
	i2c_stop();
	
	if(rec != config){
		return 0;
	}
	else{
		return 1;
	}
}

uint8_t owire_reset(){
	uint8_t status, status1;
	int poll_count = 0, poll_limit = 20;
	
	i2c_start(WRITEADD);
	i2c_write(0xB4);
	i2c_start(READADD);
	status = i2c_read_ack();
	do
	{
		status = i2c_read_ack();
	} while ((status & 0x01) && (poll_count++ < poll_limit));
	
	status1 = i2c_read_nack();
	i2c_stop();
	
	if (poll_count >= poll_limit)
	{
		devicereset();
		return 0;
	}
	
	if (status1 & PPD)
	return TRUE;
	else
	return FALSE;
	
}

uint8_t owire_write(uint8_t sendbyte){
	uint8_t status, status1;
	int poll_count=0, poll_limit=16;
	
	i2c_start(WRITEADD);
	i2c_write(0xA5);
	i2c_write(sendbyte);
	i2c_start(READADD);
	status = i2c_read_ack();
	do
	{
		status = i2c_read_ack();
	} while ((status & 0x01) && (poll_count++ < poll_limit));
	
	status1 = i2c_read_nack();
	i2c_stop();
	
	if (poll_count >= poll_limit)
	{
		devicereset();
		return 0;
	}
	else{
		return 1;
	}
}

uint8_t owire_read(void){
	uint8_t rec, status, status1;
	int poll_count=0, poll_limit=16;
	
	i2c_start(WRITEADD);
	i2c_write(0x96);
	i2c_start(READADD);
	status = i2c_read_ack();
	do
	{
		status = i2c_read_ack();
	} while ((status & STATUS_1WB) && (poll_count++ < poll_limit));
	
	rec = i2c_read_nack();
	
	if (poll_count >= poll_limit)
	{
		devicereset();
		return 0;
	}
	
	i2c_start(WRITEADD);
	i2c_write(0xE1);
	i2c_write(0xE1);
	i2c_start(READADD);
	status1 = i2c_read_nack();
	i2c_stop();
	
	return status1;
}

int main(void)
{
	DDRA = 0b00000000;
	DDRB = 0b00000000;
	DDRD = 0b00001010;
	
	PORTC = 0x00;
	PORTD = 0x00;
	PORTB = 0x00;
	
	uint8_t		rec=true;
	
	UART_0_init();
	UART_1_init();
	i2c_init();
	sei();
	
	_delay_ms(2000);

	int16_t		t_room=0,t_ambi=0,TES1=0,TES2=0;
	uint16_t	h_room=0,h_ambi=0;
	uint8_t		D=0,W=0,G=0,tes_count=0;
	uint8_t		temperature[9];
	uint16_t	retd = 0;
	//char ch1[4],ch2[4],ch3[4],ch4[4];//,ch5[4],ch6[4];
	
	rec = devicereset();
	if (!rec)
	{
		UART_1_puts("Error in device reset");
	}
	
	rec = writecfg(0x01);
	if (!rec)
	{
		UART_1_puts("Error configuring");
	}
	
	rec = channelsel(3);
	if (!rec)
	{
		UART_1_puts("Error selecting channel");
	}
	
	while(1)
	{
		
		// Room Temperature and Humidity
		if(dht_gettemperaturehumidity(&t_room, &h_room, PA1) != -1)
		{
			
		}
		
		// Ambient temperature and Humidity
		if(dht_gettemperaturehumidity(&t_ambi, &h_ambi, PA0) != -1)
		{
			
		}
		
		for (tes_count=0;tes_count<20;tes_count++)
		{
			if(therm_read_temperature(PB1, &TES1) != -1)
			{
				break;
				}else{
			}
		}
		
		for (tes_count=0;tes_count<20;tes_count++)
		{
			if(therm_read_temperature(PB0, &TES2) != -1)
			{
				break;
				}else{
			}
		}
		
		// Door Sensor
		if( PINB & (1<<PINB2) )
		{
			D=10;
		}else
		{
			D=0;
		}
		
		// Water Sensor
		if( PINB & (1<<PINB3) )
		{
			W=10;
		}else
		{
			W=0;
		}
		
		// Grid Sensing
		if( (PINA & (1<<PINA5)) && (PINA & (1<<PINA6) && (PINA & (1<<PINA7))))
		{
			G=10;
			}else{
			G=0;
		}
		
		rec = owire_reset();
		if (!rec)
		{
			UART_1_puts("Error 1-wire reset");
		}
		
		rec = owire_write(0xCC);
		if (!rec)
		{
			UART_1_puts("Error 1-wire write");
		}
		
		rec = owire_write(0x44);
		if (!rec)
		{
			UART_1_puts("Error 1-wire write");
		}
		
		_delay_ms(1000);
		
		rec = owire_reset();
		if (!rec)
		{
			UART_1_puts("Error 1-wire reset");
		}
		
		rec = owire_write(0xCC);
		if (!rec)
		{
			UART_1_puts("Error 1-wire write");
		}
		
		rec = owire_write(0xBE);
		if (!rec)
		{
			UART_1_puts("Error 1-wire write");
		}
		
		temperature[0]=owire_read();
		temperature[1]=owire_read();
		temperature[2]=owire_read();
		temperature[3]=owire_read();
		temperature[4]=owire_read();
		temperature[5]=owire_read();
		temperature[6]=owire_read();
		temperature[7]=owire_read();
		temperature[8]=owire_read();
		
		owire_reset();
		
		uint8_t crc = 0x00;
		uint8_t b=0;
		
		for (b=0; b<8; b++)
		{
			crc = _crc_ibutton_update(crc, temperature[b]);
		}
		
		if ((temperature[8] == crc) && (crc != 0xFF))
		{

			float f_temp = ( (temperature[1] << 8) + temperature[0] )*0.0625;
			retd = ceil(f_temp*10);
		}
		
		
		UART_0_putc(0xFF);
		UART_0_putc(0xFE);
		UART_0_putc(0xFD);
		UART_0_putc(t_room);		//0
		UART_0_putc(t_room>>8);	//1
		
		UART_0_putc(h_room);		//2
		UART_0_putc(h_room>>8);	//3
		UART_0_putc(t_ambi);		//4
		UART_0_putc(t_ambi>>8);	//5
		
		UART_0_putc(h_ambi);		//66
		UART_0_putc(h_ambi>>8);	//7
		UART_0_putc(retd);		//8
		UART_0_putc(retd>>8);		//9
		UART_0_putc(TES2);		//10
		UART_0_putc(TES2>>8);		//11
		UART_0_putc(D);			//12
		UART_0_putc(0x00);		//13
		UART_0_putc(G);			//14
		UART_0_putc(0x00);		//15
		UART_0_putc(W);			//16
		UART_0_putc(0x00);		//17
		
		_delay_ms(1000);

	}
	
	return(0);
}


