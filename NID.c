#include <lpc214x.h>
#include "buttons.h"
#include "system.h"
#include "timer.h"
#include "lcd.h"
#include "spi.h"
#include "SD.h"

void initialize(void);
void next(void);

unsigned char img [2048];
unsigned char const MAX_INDEX = 12;
char cur_index = 0;
	
int main()
{
   initialize();
	 while(1)
   {
		 if(next_pending == 1){
			  next_pending = 0;
			  next();
		 }
   }
   return 0;
}

void next() {
	  cur_index = (cur_index + 1) % MAX_INDEX;
		if(SD_readImage(cur_index*4,img))     // if readImage executes correctly it will finally return 0
		{
			 draw_string(0,14, "memory card error"); // Display error message
		}
		else
		{          
				clrram();                         // Clear the screen
				disp_img(0,16,128, img);        // Display the bitmap
		}  
  
	
		if(cur_index < 10)
		{
				wr_xd((16),0x24);
				wr_comm(0xb0);
					 wr_auto(cur_index + 16);
				wr_comm(0xb2);
		}
		else
		{
				wr_xd((16),0x24);
				wr_comm(0xb0);
					 wr_auto(cur_index/10 + 16);
				wr_comm(0xb2);
			
				wr_xd((17),0x24);
				wr_comm(0xb0);
					 wr_auto(cur_index%10 + 16);
				wr_comm(0xb2);
		}
}

void initialize() 
{
   __disable_irq(); //disable all interrupts (while initializing)   

   clock_init();  //initialize sytem clock

   button_init();  //initialize buttons
	
	 lcd_init();

   SPI_init();   //initialize spi

   if(SD_init()){     // initialize sd card		 
	   draw_string(0,3, "error Initializing please restart with memory card properly inserted...\r\n"); // Display a string
		 while(1);
	}	 
	
	timer_init();	 
	
	__enable_irq();  //Now enable interrupts
}
#include "buttons.h"

volatile unsigned char Power = 1, next_pending = 1, slide_show = 1;

void button_init(void)
{
	 IO0DIR |= 1; // power led
	 IO0SET |= 1;
	
   PINSEL0 |= 1<<29;  //select external interrupt function on pin P0.14 (EINT 1)
	
   PINSEL1 |= 0x301;   //select external interrupt function on pin P0.16 (EINT 0) and pin P0.20 (EINT 3)
  
   EXTINT = 0x0F;        //clear any pending external interrupt requests
   
   EXTMODE = 0x0B;       //external interrupt 0, 1 and 3 set to edge triggered
  
   EXTPOLAR = 0x00;      //and falling edge is selected meaning interrupt is triggered on button release
  
   INTWAKE = 0x02;       //configure external interrupt 1 on P0.14 to wake up the MCU from sleep
  
	 
	 VICIntSelect &= ~(0x0B<<14);		//select Eint0,1,3 as IRQ
	
	
   VICVectAddr0 = (unsigned )power_button_ISR; //Pointer Interrupt Function (ISR)

	 VICVectCntl0 = 0x20|0x0f; // bit15 = 1 -> to enable Eint1					
	
	 VICVectAddr1 = (unsigned )play_button_ISR; //Pointer Interrupt Function (ISR)

	 VICVectCntl1 = 0x20 | 0x0E; // bit14 = 1 -> to enable Eint0
					
	 VICVectAddr2 = (unsigned)next_button_ISR; //Pointer Interrupt Function (ISR)

	 VICVectCntl2 = 0x20 | 0x11; // bit17 = 1 -> to enable Eint3
					
					
	 VICIntEnable |= 0x0B<<14; //Enable interrupts
	 EXTINT &= ~(0x0B);
}
__irq void play_button_ISR(void)
{  // clear the interrupt flag; future interrupts won't be recognized otherwise
		EXTINT |= 0x1;
	  if(Power){    // otherwise simulation shows continious warnings for trying to access T0TCR
					slide_show = slide_show ? 0 : 1;
					T0TCR = slide_show ? 0x01 : 0x02;   // if slide show enable timer else reset it
		}
		VICVectAddr = 0x00;			
}

__irq void next_button_ISR(void)
{  
	 EXTINT |= 0x8;
	   next_pending = 1;
		 if(slide_show) { // if slide show restart timer
			 T0TCR = 0X02;  // Reset timer
	     T0TCR = 0x01; //Enable timer
		 }
	 VICVectAddr = 0x00;
}

__irq void power_button_ISR(void)
{  
	 EXTINT |= 0x2;
	if(Power){
		  Power = 0;
		  IO0CLR |= 1 | (1<<21);   // turn off power led and lcd
      PCONP = 0;   // disable all timers, UART, I2C, SPI, ADC, etc modules
      PCON = 0x2;  //go to power down mode
	}
	else
	{
		PCONP |= 0X102; // ENABLE TIMER 0 and SPI 0
		Power = 1;
		IO0SET |= 1 | (1<<21);
	}
	 VICVectAddr = 0x00;
}

#include "lcd.h"
void lcd_init (void)
{    
	 IO0DIR |= power;
	 IO0SET |= power;
   delay(1000);
	
   IO1DIR |=  0x3fff0000; 
   IO1SET |= rst;
   delay(1000);
   IO1CLR |= ce;
   IO1SET |= wr;
   IO1SET |= rd;
   wr_xd(addr_w,0x40);                   // Set text display buffer base address
   wr_xd(addr_t,0x42);                   // Set graphics display buffer base address
   wr_td(width,0x00,0x41);               // Set text display width
   wr_td(width,0x00,0x43);               // Set graphics display width
   wr_comm(0x81);                        // Set display mode: text xor graphics
   wr_td(0x56,0x00,0x22);                // Set CGRAM offset address
   wr_comm(0x9c);                        // Set text and graphic display on
}

void wr_od (unsigned char data,unsigned char command)       //write one byte data and one byte command
{
   wr_data(data);
   wr_comm(command);
}

void wr_td (unsigned char data_l,unsigned char data_h,unsigned char command)  //write two bytes data and one byte command
{ 
   wr_data(data_l);
   wr_data(data_h);
   wr_comm(command);
}

void wr_xd (unsigned short data,unsigned short command)       //write a 16 bits data and one byte command
{ 
   wr_data(data);
   wr_data(data>>8);
   wr_comm(command);
}

void wr_auto (unsigned char data)               //Auto write
 { 
   chk_busy (1);
   IO1CLR |= cd;
   IO1SET |= rd;
   IO1PIN=(IO1PIN&0XFF00FFFF)|(data<<16);
   IO1CLR |= wr;
   IO1SET |= wr;
 }

void wr_comm (unsigned char command)       //write command
 { 
   chk_busy (0);
   IO1SET |= cd;
   IO1SET |= rd;
   IO1PIN=(IO1PIN&0XFF00FFFF)|(command<<16);
   IO1CLR |= wr;
   IO1SET |= wr;
 }

void wr_data (unsigned char data)       //write data
 { 
   chk_busy (0);
   IO1CLR |= cd;
   IO1SET |= rd;
   IO1PIN=(IO1PIN&0XFF00FFFF)|(data<<16);
   IO1CLR |= wr;
   IO1SET |= wr;
 }

void chk_busy (unsigned char autowr)    //test busy flag
{ 
   IO1SET |= 0x00FF0000;
   IO1SET |= cd;
   IO1SET |= wr;
   IO1DIR &= 0xFF00FFFF;
   IO1CLR |= rd;
   if(autowr)
    { while((IO1PIN&bf3)==0);
    }
   else
    { while((IO1PIN&bf0)==0||(IO1PIN&bf1)==0);
    }
   IO1SET |= rd;
   IO1DIR |= 0x00FF0000;
}

void clrram (void)
{ 
   unsigned char i,j;
   wr_xd(addr_w,0x24);
   wr_comm(0xb0);
   for(j=0;j<144;j++)
    { 
     for(i=0;i<width;i++)
         wr_auto(0x00);
    }
   wr_comm(0xb2);
}

void disp_img (unsigned char addr,unsigned char xl,unsigned char yl,unsigned char const *img)
{ 
    unsigned char i,j;
    for(j=0;j<yl;j++)
    { 
       for(i=0;i<xl;i++)
       { 
         wr_xd(addr+0X100+j*width+i,0x24);
         wr_od(img[j*xl+i],0xc0);
       }
    }
}

void draw_string(unsigned int x,unsigned int y, char *str)
{ 
   char c;
   wr_xd((addr_w+16*y+x),0x24);
   wr_comm(0xb0);
   while(*str!='\0')
   { 
      c = (*str);
      wr_auto(c-32);
      str++;
   }
   wr_comm(0xb2);
}

void delay(unsigned int limit){
int m;  
      for(m=0; m<limit; m++);
}

#include "sd.h"
#include "spi.h"
#include <stdlib.h>
char SD_init(void) {
    char j, response;
    unsigned int i;

    for (j = 0; j < 10; j++) //at least 74 clocks to take card in to SPI mode
    {
        SPI_write(0xff);
    }

    i = 0;

    do {
        response = SD_sendCommand(GO_IDLE_STATE, 0); //send 'reset & go idle' command untill card becomes ready
    } while (response != 0x01 && i++ < 0xfffe);

    if (i > 0xfffe) {
        return 1;     //loop was exited due to timeout, card is not responding (too bad)
    }
    SD_CS_DEASSERT;              //deselect card
    SPI_write(0xff);
    SPI_write(0xff);

    i = 0;

    do {
        response = SD_sendCommand(SEND_OP_COND, 0); //activate card's initialization process
        response = SD_sendCommand(SEND_OP_COND, 0); //same command sent again for compatibility with some cards 
    } while (response != 0x00 && i++ < 0xfffe);

    if (i > 0xfffe) {
        return 1;                      // loop was exited due to timeout (not again!)
    }
    SD_sendCommand(SET_BLOCK_LEN, 512); //set block size to 512

    SD_CS_DEASSERT;                     //deselect card

    return 0;                          //Yay! Successfully initialized
}
char SD_sendCommand(char cmd, unsigned long arg)
{
    char response;
    unsigned long retry = 0;
    SD_CS_ASSERT;             //select card (activate bus)

    SPI_write(cmd | 0x40);    //send command, first two bits always '01'
    SPI_write(arg >> 24);       //send command argument (32 bit) a byte at a time starting from MSB
    SPI_write(arg >> 16);
    SPI_write(arg >> 8);
    SPI_write(arg);
    SPI_write(0x95);          //pre-calculated checksum(see SD Card manual)

    while (((response = SPI_read()) == 0xff) && (retry++ < 0x0fffffff));  //wait response

    SPI_read();                                                          //extra 8 CLK

    return response;                                                     //return card response
}


char SD_readImage(unsigned long startBlock,unsigned char *img)
{
    char response;
    unsigned long  retry = 0;
    unsigned int max, i, start;

    for (start = 0; start < 2048; start += 512) {
        response = SD_sendCommand(READ_SINGLE_BLOCK, startBlock * 512 + start); //read a Block command

        if (response != 0x00) //check for SD status: 0x00 - OK (No flags set)
        {
            return response;             //at least one Error flag is up, return error
        }
        retry = 0;

        while ((SPI_read() != 0xfe) && retry++ < 0x00ffffff);       //wait for start block token 
        if (retry > 0x00ffffff) {
            SD_CS_DEASSERT;
            return 1;
        }        //return error if time-out
        max = start + 512;
        for (i = start; i < max; i++) //read 512 bytes in to buffer
        {
            img[i] = SPI_read();
        }

        SPI_read();                    //receive incoming CRC (16-bit), evenif CRC is ignored
        SPI_read();

        SPI_read();                    //extra 8 clock pulses
    }
    SD_CS_DEASSERT;                //deselect card
    return 0;                      //512 bytes read successfully
}
#include <lpc214x.h>
unsigned int SPI_init(void)
{
  unsigned int status;
  
  PINSEL0 |= 1<<8;    //SPI SCK function selected on pin 4
  PINSEL0 |= 1<<10;    //SPI MISO function selected on pin 5
  PINSEL0 |= 1<<12;    //SPI MOSI function selected on pin 6
  
  IO0DIR |= 1<<11;      //make SSEL output
  IO0SET |= 1<<11;      //and drive it high (disable slave)
  
  S0SPCR = 0x20;           //Master mode, clock active-high, data 8-bit MSB first, no interrupt
  
  status = S0SPSR;         //reading the status register will clear it
  
  //S0SPCCR = 8;            //SPI_CLK = PCLK/8 = 48M/8 = 6MHz (i.e SPI data rate = 6Mbps)
  
  S0SPCCR = 128;            //SPI_CLK = PCLK/128 = 48M/128 = 375KHz (for compatibility with MMC initialization)
  
  return status;            //dummy
  
} 
char SPI_write(char data) 
{       
    S0SPDR = data;                  //start transfer
    while((S0SPSR&(1<<7)) == 0);    //wait for transfer complete
       
    return S0SPDR;                  //dummy: read the data register to clear the status flag
} 
char SPI_read(void) 
{   
    S0SPDR = 0xff;                  //dummy write
    while((S0SPSR&(1<<7)) == 0);    //wait for data to arrive
       
    return S0SPDR;                  //read the data register to clear the status flag
} 
#include <lpc214x.h>
#include "system.h"
		
void clock_init(void)
{
	
	setupPLL0();
	feedSeq(); //sequence for locking PLL to desired freq.
	connectPLL0();
	feedSeq(); //sequence for connecting the PLL as system clock
	
	//SysClock is now ticking @ 48Mhz!
       
	VPBDIV = 0x01; // PCLK is same as CCLK i.e 48Mhz

}

void setupPLL0(void)
{
    PLL0CON = 0x01; // PPLE=1 & PPLC=0 so it will be enabled 
                    // but not connected after FEED sequence
    PLL0CFG = 0x23; // set the multipler to 4 (i.e actually 3) 
                    // i.e 12x4 = 48 Mhz (M - 1 = 4)!!!
                    // Set P=2 since we want FCCO in range!!!
                    // So , Assign PSEL =01 in PLL0CFG as per the table.
}

void feedSeq(void)
{
	PLL0FEED = 0xAA;
	PLL0FEED = 0x55;
}

void connectPLL0(void)
{
    // check whether PLL has locked on to the  desired freq by reading the lock bit
    // in the PPL0STAT register

    while( !( PLL0STAT & PLOCK ));

	// now enable(again) and connect
	PLL0CON = 0x03;
}
#include "buttons.h"  // for importing slide_show and next_pending flags
#include "timer.h"
#include <lpc214x.h>

void timer_init()
{
	T0CTCR = 0x0;									   	
	
	T0PR = PRESCALE-1; //(Value in Decimal!) - Increment T0TC at every 48000 clock cycles
                     //Count begins from zero hence subtracting 1
                     //48000 clock cycles @48Mhz = 1 mS
	
	T0MR0 = DELAY_MS-1; //(Value in Decimal!) Zero Indexed Count - hence subtracting 1
	
	T0MCR = MR0I | MR0R; //Set bit0 & bit1 to High which is to : Interrupt & Reset TC on MR0  

	//----------Setup Timer0 Interrupt-------------

  	
	VICVectAddr3 = (unsigned )T0ISR; //Pointer Interrupt Function (ISR)

	VICVectCntl3 = 0x20 | 4; //0x20 (i.e bit5 = 1) -> to enable Vectored IRQ slot
				  //4 is for timer0
	
	VICIntEnable |= 0x02; //Enable timer0 int
	
	T0TCR = 0x02; //Reset Timer
	T0TCR = 0x01; //Enable timer
}

__irq void T0ISR(void)
{
	T0IR = 0xff;

        next_pending = 1;
	
	VICVectAddr = 0x00; //This is to signal end of interrupt execution
}