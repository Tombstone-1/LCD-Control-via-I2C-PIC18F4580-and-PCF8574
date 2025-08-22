/*
 * File:   main.c
 * Author: mohda
 * Created on August 12, 2025, 3:33 PM
 * 
 * TOPIC : LCD control with PCF8574.
 */


#include <xc.h>

#define _XTAL_FREQ 4000000
#define PCF_W_ADDR 0x40         // A0,A1,A2 -> logic low.
#define I2C_NACK    1
#define I2C_ACK     0
#define ERROR_L   LATDbits.LATD0  // error led 

/********************* Function prototype *************************/
void I2C_begin(void);
void I2C_ready(void);
uint8_t I2C_beginTransmission(uint8_t address);
uint8_t I2C_writeData(uint8_t data);
uint8_t I2C_endTransmission(void);

/********************* lcd ****************************************/
void lcd_init(void);
void lcd_clear(void);
void lcd_string(char *str);
void lcd_put_cursor(uint8_t row, uint8_t col);
void lcd_cmd(uint8_t cmd);
void lcd_print(uint8_t data);
void send_to_display(uint8_t *disp_arr);

/********************** main **************************************/
void main(void) {
    TRISDbits.RD0 = 0;     // error indication led output.
    I2C_begin();    
    __delay_ms(10);
    lcd_init();
    __delay_ms(10);
    
    while(1) {
        lcd_clear();
        lcd_put_cursor(0,0);
        lcd_string("Hello World !");
        lcd_put_cursor(1,7);
        lcd_string("PCF8574");
       
        __delay_ms(2000);
    }
}

/************************ User Definitions ************************/
void I2C_begin(void) {
    SSPSTATbits.SMP = 1;    // slew rate control for standard speed mode (100 KHz).
    SSPSTATbits.CKE = 0;    // disable SMBUS specific inputs.
    
    SSPCON1 = 0x28;     // enable serial port pins, I2C master mode -clock fosc/(4*sspadd+1)
    SSPCON2 = 0x00;     // cleared all bits.
    SSPADD = 9;     // master mode baud rate. when I2C clock at 100khz, formula clock -> FOSC /4 x SSPADD + 1.
    PIR1bits.SSPIF = 0;     // begin by clearing SSPBUF.
}

void I2C_ready(void) {
    // handle both bus and write collision her properly.
    if (PIR2bits.BCLIF || SSPCON1bits.WCOL) {   
        PIR2bits.BCLIF = 0;
        SSPCON1bits.WCOL = 0;
    }
    // wait for Transmit progress to complete.
    while(SSPSTATbits.BF || SSPSTATbits.R);
    // last clear SSPBUF regiter to Set proper flag.
    PIR1bits.SSPIF = 0;
}

uint8_t I2C_beginTransmission(uint8_t address) {
    I2C_ready();
    SSPCON2bits.SEN = 1;    // start I2C condition on bus.
    while(SSPCON2bits.SEN);    // wait for start condition to idle.
    
    if (!SSPSTATbits.S) {
        ERROR_L = 1;
        return 0;
    }
    I2C_writeData(address);
    ERROR_L = 0;
    return 1;
}

uint8_t I2C_writeData(uint8_t data) {
    I2C_ready();
    SSPBUF = data;
    while(!PIR1bits.SSPIF);
    PIR1bits.SSPIF = 0;
    
    if (SSPCON2bits.ACKSTAT) {
        return I2C_NACK;
    }else {
        return I2C_ACK;
    }
}

uint8_t I2C_endTransmission(void) {
    I2C_ready();
    SSPCON2bits.PEN = 1;    // enable stop condition on bus.
    while(SSPCON2bits.PEN);     // wait for stop condition to idle.
    
    if (!SSPSTATbits.P) {       // if stop bit is not detected.
        ERROR_L = 1;
        return 0;
    }
    ERROR_L = 0;
    return 1;
}

/********************** LCD ************************************/
void lcd_init(void) {
    // power up sequence.
    lcd_cmd(0x02);      // 4 bit mode
    lcd_cmd(0x28);      // use 5x8 font 4 bit mode
    lcd_cmd(0x01);      // clear screen
    lcd_cmd(0x0E);      // display on, steady cursor
    lcd_cmd(0x06);      // increment cursor.
}

void lcd_clear(void) {
    lcd_cmd(0x01);
    __delay_ms(10);
}

void lcd_string(char *str) {
    while(*str != '\0') {
        lcd_print(*str);
        str++;
    }
}

void lcd_put_cursor(uint8_t row, uint8_t col) {
    switch(row) {
        case 0 : col |= 0x80;
        break;
        
        case 1 : col |= 0xC0;
        break;
    }
    lcd_cmd(col);
}

void lcd_cmd(uint8_t cmd) {
    uint8_t cmd_arr[4], high_nib, low_nib;
    
    high_nib = (cmd & 0xF0);    // get 4 msb bits.
    low_nib = (uint8_t)((cmd & 0x0F) << 4);  // get 4 lsb bits and shift to msb position, PCF8574 connected that way.
    
    // strobe enable pin to send MSB and LSB. ( 0x0C and 0x08 are to control PCF itself)
    cmd_arr[0] = high_nib | 0x0C;   // // p3-led = 1, p2-en = 1, p1-rw = 0, p0-rs = 0 -> bxxxx 1100.
	cmd_arr[1] = high_nib | 0x08;	// p3-led = 1, p2-en = 0, p1-rw = 0, p0-rs = 0 -> bxxxx 1000.
	cmd_arr[2] = low_nib | 0x0C;	// p3-led = 1, p2-en = 1, p1-rw = 0, p0-rs = 0 -> bxxxx 1100.
	cmd_arr[3] = low_nib | 0x08;	// p3-led = 1, p2-en = 0, p1-rw = 0, p0-rs = 0 -> bxxxx 1000.
    
    send_to_display(cmd_arr);
}

void lcd_print(uint8_t data) {
    uint8_t data_arr[4], high_nib, low_nib;
    
    high_nib = (data & 0xF0);       // higher nibble
    low_nib = (uint8_t)((data & 0x0F) << 4);    // lower nibble shifted to higher side.
    
     // strobe enable pin to send MSB and LSB. ( 0x0D and 0x09 are to control PCF itself)
    data_arr[0] = high_nib | 0x0D;   // p3-led = 1, p2-en = 1, p1-rw = 0, p0-rs = 1 -> bxxxx 1101.
    data_arr[1] = high_nib | 0x09;  // p3-led = 1, p2-en = 0, p1-rw = 0, p0-rs = 1 -> bxxxx 1001.
    data_arr[2] = low_nib | 0x0D;  // p3-led = 1, p2-en = 1, p1-rw = 0, p0-rs = 1 -> bxxxx 1101.
    data_arr[3] = low_nib | 0x09; // p3-led = 1, p2-en = 0, p1-rw = 0, p0-rs = 1 -> bxxxx 1001.
    
    send_to_display(data_arr);
}

void send_to_display(uint8_t *disp_arr) {
    I2C_ready();
    I2C_beginTransmission(PCF_W_ADDR);
    
    for(int i=0; i<4; i++) {
        I2C_writeData(disp_arr[i]);
        __delay_ms(20);    // delay for enable pin strobing in lcd.
    }
    I2C_endTransmission();
}    
