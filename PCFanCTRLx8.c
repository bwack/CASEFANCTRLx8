// PCCaseFANCTRL 8x
// PIC16F687 MCU @ 8Mhz intosc
// EasyPIC5 Development board.

// Lowest speed of 2tach/round fans is 457.77 RPM, Prescaler is 1:2
// = 60s/min * ( 1 / (65535tick * 1us/tick) [1/s]) / 2 tach/round = 457.77 RPM
// Alternatively set prescaler to 1:4 to measure lowest speed 228.8 RPM.

#define INHIBIT PORTA.B5
#define TACH    PORTA.B2
//#define CTRLA   PORTB.B6
//#define CTRLB   PORTA.B1
//#define CTRLC   PORTB.B4
#define CTRLA   PORTA.B1
#define CTRLB   PORTB.B4
#define CTRLC   PORTB.B6


unsigned long dummy;
unsigned short messagestring[5], fantocheck;
unsigned int fanspeed[8];
unsigned short counter=0, lastread=0, commandbuffer[8], cmd_flag=0;
unsigned short pwmcounter=0, pwmwaveform[32], current[8];
unsigned short fan, adc, ranges, state;
//sbit CMD_bit at cmd.B7;
typedef struct bitfield {
  speed : 5;
  fan   : 3;
} cmd_struct;
cmd_struct cmd;

//unsigned long longjohn;                    34

void interrupt(void) {
  INTCON.GIE=0;
  if (INTCON.T0IF) {
    INTCON.T0IF=0;
    PORTC = pwmwaveform[pwmcounter];
    pwmcounter++;
    pwmcounter&=0x1F;
    TMR0=230;
  }
  if (PIR1.RCIF) {
    commandbuffer[counter]=UART1_Read();
    counter++;
    counter&=0x07;
    cmd_flag++;
    //TMR0 = 96;
  }
  
  INTCON.GIE=1;
}

void int2dec( unsigned short *array, unsigned int number,
              unsigned short factor, unsigned int offset) {
  unsigned short i=0;
  number=offset+number*factor;
//  number=offset+number*factor;

  while(number>=10000u) {
    number=number-10000u;
    i++;
  }
  array[0] = i+0x30;
  i=0;
  while(number>=1000u) {
    number=number-1000u;
    i++;
  }
  array[1] = i+0x30;
  i=0;
  while(number>=100u) {
    number=number-100u;
    i++;
  }
  array[2] = i+0x30;
  i=0;
  while(number>=10u) {
    number=number-10u;
    i++;
  }
  array[3] = i+0x30;
  array[4] = number+0x30;
  array[5]=0;
}

void stringclipleft( unsigned short *array ) {
  unsigned short i=0;
  while(array[i]!=0x00) {
    array[i]=array[i+1];
    i++;
  }
}

unsigned int measure_fanspeed(unsigned short prescaler, unsigned short _mask) {
  //if(fanspeed[fannumber]==0x00 && fantocheck!=fannumber) return 0x00;
  TMR1L=0; TMR1H=0;
  PIR1.TMR1IF=0;
  T1CON.T1CKPS0=prescaler;
  T1CON.T1CKPS1=prescaler>>1;

  T1CON.TMR1ON=1;
  while(TACH==0 && !PIR1.TMR1IF) {}
  //state=INTCON.GIE;
  //INTCON.GIE=0; // interrupts off
  while(TACH==1 && !PIR1.TMR1IF) {}
  TMR1L=0; TMR1H=0;
  while(TACH==0 && !PIR1.TMR1IF) {}
  while(TACH==1 && !PIR1.TMR1IF) {}
  T1CON.TMR1ON=0;
  //INTCON.GIE=state;
  PORTA.RA5=0;
  dummy=(TMR1H<<8) + TMR1L;
  delay_us(1000);
  
  ADCON0.ADON=1;
  delay_us(10);
  ADCON0.GO=1;
  while(ADCON0.GO==1) {}
  adc=ADRESL;
  delay_us(100);
  if(dummy>18750 || PIR1.TMR1IF) {
    ranges&=(~_mask);
  }
  else {
    ranges|=_mask;
  }
  if (PIR1.TMR1IF) {
    PIR1.TMR1IF=0;
    return 0x00;
  }


////  if (prescaler==0) { dummy=2400000/dummy; }
////  else if (prescaler==1) { dummy=1200000/dummy; }

//  myfloat=30000000.0/dummy;
  dummy = 30000000/dummy;
//  dummy=3000000/dummy;
/*  if (prescaler==1 && ranges&_mask )  {
    dummy=dummy-160;
    dummy=dummy>>1;
  }
  */
  //if (prescaler==1 && !ranges ) {  }
////  else if (prescaler==2) { dummy=600000/dummy; }
////  else if (prescaler==3) { dummy=300000/dummy; }
  return dummy;
}

/*
unsigned short measure_current(void) {
  ADCON0.ADON=1;
  delay_us(10);
  ADCON0.GO=1;
  while(ADCON0.GO==1) {}
//  dummy=(TMR1H<<8) + TMR1L;
//  dummy=1200000/dummy;
  delay_us(100);
  return ADRESL;
}
  */
  
unsigned short setspeed(unsigned short fan, unsigned int speed) {
  unsigned short i, mask;
  //UART1_Write(fan);
  //UART1_Write(speed);
  mask=0x01;
  while(fan>0){ // move mask bit to correct fan bit
    mask<<=1;
    fan--;
  }
  i=0;
  while(i<speed) {
    // clear waveform bits
    pwmwaveform[i]&=(~mask);
    i++;
  }
  while(i<32) {
    // set waveform bits
    pwmwaveform[i]|=mask;
    i++;
  }
  return 0x00;
}

void main() {
  unsigned short i,j,mask;
  OSCCON = 0b1110001; // intosc, no prescaling.
  OPTION_REG=0x02; // Prescaler, Fosc/4, PS 1:16
  ANSEL = 0x01; //analog input on pin 0, digital io on the rest.
  ANSELH = 0x00;
  ADCON0=0b10000001;
  ADCON1=0b01010000; // conversion time Fosc/16
  CM1CON0=0x07;
  CM2CON0=0x00;
  TRISA=0b00001101;
  TRISB=0b10101111;
  TRISC=0b00000000;
  T1CON = 0x10; // Prescaler 1:2, intosc clock source, => 1us tick
  TMR1L=0; TMR1H=0;
  T1CON.TMR1ON=1;
  delay_ms(1);
  T1CON.TMR1ON=0;
  UART1_init(38400);
  PIE1.RCIE=1;   // UART Receive Interrupt Enable
  INTCON.T0IE=1; // TIMER0 Interrupt Enable
  INTCON.PEIE=1; // Peripherial interrupt enable
  INTCON.GIE=1;  // Global Interrupt enable
  setspeed(0,0x10);
  setspeed(1,0x10);
  setspeed(2,0x10);
  setspeed(3,0x10);
  setspeed(4,0x10);
  setspeed(5,0x10);
  setspeed(6,0x10);
  setspeed(7,0x10);
  fantocheck=0x01;

  while (1) {
    fan=0; mask=0x01;
    while(fan<8)     {
      if(!(fanspeed[fan]==0x00 && fantocheck!=fan)) {
        CTRLA=fan&0x01;
        CTRLB=(fan>>1)&0x01;
        CTRLC=(fan>>2)&0x01;
        INHIBIT=0;
        fanspeed[fan] = measure_fanspeed(1,mask);
//      current[fan] = measure_current();
        current[fan] = adc;
        INHIBIT=1;
      }
      fan++;
      mask=mask<<1;
    }
//    UART1_Write(0x0a);

    fan=0; mask=0x01;
    while(fan<8) {
      UART1_Write(',');
//      int2dec(&messagestring,fanspeed[fan],25);
/*      if (ranges&mask) {
        int2dec(&messagestring,fanspeed[fan],20,1600);
      }
      else {
        int2dec(&messagestring,fanspeed[fan],1,0);
      }
      */
      int2dec(&messagestring,fanspeed[fan],1,0);
      UART1_Write_Text(messagestring);
      fan++;
      mask=mask<<1;
    }

    for(fan=0; fan<8; fan++) {
      UART1_Write(',');
      int2dec(&messagestring,current[fan],5,0);
      stringclipleft(&messagestring);
      UART1_Write_Text(messagestring);
    }

    while(cmd_flag>0) {
      unsigned short _data;
      INTCON.GIE=0;
      _data = commandbuffer[(counter-cmd_flag)&0x07];
      cmd.fan = _data>>5;
      cmd.speed = _data&0b00011111;
      setspeed(cmd.fan,cmd.speed);
      INTCON.GIE=1;
      cmd_flag--;
    }

    UART1_Write(0x0a);
    delay_ms(250);
    fantocheck++;
    fantocheck&=0b00000111;
  }
}