/*
 *    Conversor teclado ZX-spectrum 8x5 -> PS/2 del ZX-UNO by Quest
 *
 *    Combinaciones especiales: 
 *    - Reset = SYMBOL+CAPS+7
 *    - Reset Maaestro = SYMBOL+CAPS+8
 *    - NMI = SYMBOL+CAPS+5
 */

#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>
#include "keymaps.h"

#define LED_ON	PORTB |= (1<<5)		
#define LED_OFF	PORTB &= ~(1<<5)
#define LED_CONFIG	DDRB |= (1<<5) //Led en PB5 en Pro mini y similares

#define ZXUNO_F1 KEY_1 //tecla F1
#define ZXUNO_F2 KEY_2 //tecla F2
#define ZXUNO_F3 KEY_3 //tecla F3
#define ZXUNO_F4 KEY_4 //tecla F4
#define ZXUNO_F5 KEY_5 //tecla F5 para NMI junto a shift+symbol (5)
#define ZXUNO_F6 KEY_6 //tecla F6
#define ZXUNO_F7 KEY_7 //tecla F7 para el reseteo junto a shift+symbol (7)
#define ZXUNO_F8 KEY_8 //tecla F8 para el master reset junto a shift+symbol (8)
#define ZXUNO_F9 KEY_9 //tecla F9
#define ZXUNO_F10 KEY_0 //tecla F10
#define ZXUNO_F11 KEY_A //tecla F11
#define ZXUNO_F12 KEY_B //tecla F12

////////Pro Mini
#define PS2_PORT	PORTC
#define PS2_DDR		DDRC
#define PS2_PIN		PINC

#define PS2_DAT		PC4
#define PS2_CLK		PC5

#define HI 1
#define LO 0
#define _IN 1
#define _OUT 0

//Definicion de pines y puertos en arrays

///////////Pro Mini
//{PC1, PC0, PB4, PB3, PB2};
//uint8_t pinsC[COLS] =  {1, 0, 4, 3, 2};
//uint8_t bcdC[COLS] =   {3, 3, 2, 2, 2};

uint8_t pinsC[COLS] =  {2, 3, 4, 0, 1};
uint8_t bcdC[COLS] =   {2, 2, 2, 3, 3};

//{PD2, PD3, PD4, PD5, PD6, PD7, PB0, PB1};
uint8_t pinsR[ROWS] =  {2, 3, 4, 5, 6, 7, 0, 1};
uint8_t bcdR[ROWS] =   {4, 4, 4, 4, 4, 4, 2, 2};

//Teclas Pulsadas en el ultimo pase
uint8_t antkey[0xFF]={0}; //Estado anterior del codigo "key" 0 Sin pulsar / 1 Pulsado

//Caps Shift (Shift)
#define SHIFT_COL 4  
#define SHIFT_ROW 5  

//Symbol(control)
#define SYMBOL_COL 3   
#define SYMBOL_ROW 7   

//Comibinacion F1 (1)
#define F1_COL 4
#define F1_ROW 0

//Comibinacion F2 (2)
#define F2_COL 3
#define F2_ROW 0

//Comibinacion F3 (3)
#define F3_COL 2
#define F3_ROW 0

//Comibinacion F4 (4)
#define F4_COL 1
#define F4_ROW 0

//combinacion F5 NMI (5)
#define F5_COL 0
#define F5_ROW 0

//Comibinacion F6 cambio modo video (6)
#define F6_COL 0
#define F6_ROW 3

//combinacion F7 reset (7)
#define F7_COL 1
#define F7_ROW 3

//combinacion F8 master reset (8)
#define F8_COL 2
#define F8_ROW 3

//Comibinacion F9 (9)
#define F9_COL 3
#define F9_ROW 3

//Comibinacion F10 (0)
#define F10_COL 4
#define F10_ROW 3

//Comibinacion F11 (A)
#define F11_COL 4
#define F11_ROW 2

//Comibinacion F12 (B)
#define F12_COL 0
#define F12_ROW 7


void leds_debug(){
 	LED_ON;
	_delay_ms(60);
	LED_OFF;
	_delay_ms(30);
}

void pinSet(pin, bcd, stat) //stat 1 = in, stat 0 = out
{
     switch(bcd){
	case 2:  if (stat) DDRB &= ~_BV(pin); else DDRB |= _BV(pin); break;
        case 3:  if (stat) DDRC &= ~_BV(pin); else DDRC |= _BV(pin); break;
	case 4:  if (stat) DDRD &= ~_BV(pin); else DDRD |= _BV(pin); break;	  
     }
} 

uint8_t pinStat(pin, bcd)
{
     switch(bcd){
	case 2:  if (!(PINB & (1<<pin))) return 1; else return 0; break;
        case 3:  if (!(PINC & (1<<pin))) return 1; else return 0; break;
	case 4:  if (!(PIND & (1<<pin))) return 1; else return 0; break;	  
     }
  return 0;
}

void pinPut(pin, bcd, stat) //stat 1 = HI, stat 0 = LO
{
     switch(bcd){
	case 2:  if (!stat) PORTB &= ~_BV(pin); else PORTB |= _BV(pin); break;
        case 3:  if (!stat) PORTC &= ~_BV(pin); else PORTC |= _BV(pin); break;
	case 4:  if (!stat) PORTD &= ~_BV(pin); else PORTD |= _BV(pin); break;	  
     }
} 

void ps2Mode(uint8_t pin, uint8_t mode) 
{
    if(mode) { //high
	PS2_DDR &= ~_BV(pin); //input (Truco DDR. Como input sin estado, se pone en modo Hi-Z)
        //PS2_PORT |= _BV(pin);  //high
    } else { //low
        PS2_DDR |= _BV(pin); //output (Truco DDR. Como output, se pone a 0v)
        //PS2_PORT &= ~_BV(pin);  //low
    }
}

void ps2Init() 
{
  //ponemos en alto ambas señales
  PS2_PORT &= ~_BV(PS2_DAT); //A 0
  PS2_PORT &= ~_BV(PS2_CLK); //A 0
  ps2Mode(PS2_DAT, HI);
  ps2Mode(PS2_CLK, HI);
}

uint8_t ps2Stat()
{
  if (!(PS2_PIN & (1<<PS2_CLK)))
    return 1;
  if (!(PS2_PIN & (1<<PS2_DAT)))
    return 1;
    
  return 0;
}

//En us, reloj y semireloj, para los flancos
//zxuno v2 test15: (x12) CK1 = 240, CK2 = 480. Uso normal: CK1 = 20, CK2 = 40 
//(revertir a normal cuando el core ps/2 del ZX-UNO se mejore)
#define CK1 240 
#define CK2 480

//envio de datos ps/2 simulando reloj con delays.
void sendPS2(unsigned char code)
{
LED_ON;
        //Para continuar las líneas deben estar en alto
	if (ps2Stat())
		return;   
 
 	unsigned char parity = 1;
	unsigned char i = 0;
        
        //iniciamos transmisión
        ps2Mode(PS2_DAT, LO);
        _delay_us(CK1);
        
        ps2Mode(PS2_CLK, LO); //bit de comienzo
        _delay_us(CK2);
        ps2Mode(PS2_CLK, HI);
        _delay_us(CK1);
        //enviamos datos
        for (i = 0; i < 8; ++i) 
	{
	    if ((0b00000001 & code))
	      ps2Mode(PS2_DAT, HI);		
	    else
	      ps2Mode(PS2_DAT, LO);    		
	    
	    _delay_us(CK1);
            ps2Mode(PS2_CLK, LO);
            _delay_us(CK2);
            ps2Mode(PS2_CLK, HI);
            _delay_us(CK1);

        //paridad
            if ((0b00000001 & code) == 0b00000001) 
            {
                if (!parity)
                    parity = 1;
                else
                    parity = 0;
            }
    	    code = code >> 1;
	}

	// Enviamos bit de paridad
	if (parity)
	    ps2Mode(PS2_DAT, HI);		
	else
	    ps2Mode(PS2_DAT, LO);
        
	 _delay_us(CK1);
         ps2Mode(PS2_CLK, LO);
         _delay_us(CK2);
         ps2Mode(PS2_CLK, HI);
         _delay_us(CK1);
            
         //Bit de parada
         ps2Mode(PS2_DAT, HI);
         _delay_us(CK1);
         ps2Mode(PS2_CLK, LO);
         _delay_us(CK2);
         ps2Mode(PS2_CLK, HI);
         _delay_us(CK1);
         
         _delay_us(CK2*3); //fin
        
LED_OFF;
}

//codifica envio de caracteres ps/2 
void sendCode(unsigned char key, uint8_t shiftd, uint8_t symbold) 
{
  
  unsigned char key_mod = KEY_LSHIFT; //si es shift suelto = shift, si es para reset = ralt.
  unsigned char key_mod2 = KEY_LCTRL; //si es lcontrol suelto = lcontrol, si es para reset = rcontrol.

//secuencia  
   if (shiftd)  sendPS2(key_mod);  
   if (symbold) sendPS2(key_mod2);

//  if (extn) 
//   sendPS2(0xE0);
   
  if (key)
   {
   sendPS2(key);
   antkey[key]=1;
   }
  
//  if (extn)
//    sendPS2(0xE0);
  
  if (key && (shiftd || symbold)) //Cuando es solo Key lo paso al modulo releaseKey
   {
    sendPS2(0xF0);
    sendPS2(key);
	antkey[key]=0;
   }
  
  if (symbold) {
    sendPS2(0xF0);
    sendPS2(key_mod2);
  }    
  
  if (shiftd) {
    sendPS2(0xF0);
    sendPS2(key_mod);
  }
  
//fin secuencia
}

void pressKey(uint8_t r, uint8_t c, uint8_t shiftd, uint8_t symbold)
{  
uint8_t key = mapZX[r][c];
sendCode(key,shiftd,symbold);
}

void releaseKey(uint8_t r, uint8_t c, uint8_t shiftd, uint8_t symbold)
{  
uint8_t key = mapZX[r][c];

if (key && antkey[key])
 {
  sendPS2(0xF0);
  sendPS2(key);
  antkey[key]=0;
 } 
}

void pressFKey(unsigned char key)
{
  sendPS2(key);
  _delay_us(5);
  sendPS2(0xF0);
  sendPS2(key);
}

//void releaseFKey(unsigned char key)
//{
//  sendPS2(0xF0);
//  sendPS2(key);
//  antkey[key]=0;
//}

//preparamos matriz teclado.
void matrixInit()
{
   uint8_t c, r;
    
  LED_CONFIG;
  LED_OFF;

  for (c=0;c<COLS;c++)
  { 
    pinSet(pinsC[c],bcdC[c],_IN);
    pinPut(pinsC[c], bcdC[c], HI);
  }

  for (r=0;r<ROWS;r++)
    pinSet(pinsR[r],bcdR[r],_IN);
}

void matrixScan()
{
 
  uint8_t shiftd = 0;
  uint8_t symbold = 0;
  //uint8_t pref1=0, pref2=0, pref3=0, pref4=0, pref5=0, pref6=0, pref7=0, pref8=0, pref9=0, pref10=0, pref11=0, pref12=0;
  uint8_t preKey=0;
  uint8_t r, c;
  //uint8_t keyPressed = 0;

//Combinaciones especiales
  //pulsacion shift
  pinSet(pinsR[SHIFT_ROW],bcdR[SHIFT_ROW],_OUT);  
 _delay_us(5);
   if(pinStat(pinsC[SHIFT_COL], bcdC[SHIFT_COL]))
    shiftd = 1;
  pinSet(pinsR[SHIFT_ROW],bcdR[SHIFT_ROW],_IN);
 _delay_us(5);

  //pulsacion symbol  
  pinSet(pinsR[SYMBOL_ROW],bcdR[SYMBOL_ROW],_OUT);  
 _delay_us(5);
  if(pinStat(pinsC[SYMBOL_COL], bcdC[SYMBOL_COL])) 
    symbold = 1;  
 _delay_us(5);
  pinSet(pinsR[SYMBOL_ROW],bcdR[SYMBOL_ROW],_IN);

  //ver si es un tecla de Funcion F1-F12 que son todas con combos de shiftd y sybold o solo es para entrar en el Extend Mode
  if (shiftd && symbold) {
    pinSet(pinsR[F1_ROW],bcdR[F1_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F1_COL], bcdC[F1_COL])) 
      preKey = KEY_F1;
    pinSet(pinsR[F1_ROW],bcdR[F1_ROW],_IN);

 _delay_us(5);

    pinSet(pinsR[F2_ROW],bcdR[F2_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F2_COL], bcdC[F2_COL]))
      preKey = KEY_F2;  
    pinSet(pinsR[F2_ROW],bcdR[F2_ROW],_IN);

 _delay_us(5);

    pinSet(pinsR[F3_ROW],bcdR[F3_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F3_COL], bcdC[F3_COL]))
      preKey = KEY_F3;
    pinSet(pinsR[F3_ROW],bcdR[F3_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F4_ROW],bcdR[F4_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F4_COL], bcdC[F4_COL]))
      preKey = KEY_F4;
    pinSet(pinsR[F4_ROW],bcdR[F4_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F5_ROW],bcdR[F5_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F5_COL], bcdC[F5_COL]))
      preKey = KEY_F5;
    pinSet(pinsR[F5_ROW],bcdR[F5_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F6_ROW],bcdR[F6_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F6_COL], bcdC[F6_COL]))
      preKey = KEY_F6;
    pinSet(pinsR[F6_ROW],bcdR[F6_ROW],_IN);

 _delay_us(5);

    pinSet(pinsR[F7_ROW],bcdR[F7_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F7_COL], bcdC[F7_COL]))
      preKey = KEY_F7;
    pinSet(pinsR[F7_ROW],bcdR[F7_ROW],_IN);

 _delay_us(5);

    pinSet(pinsR[F8_ROW],bcdR[F8_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F8_COL], bcdC[F8_COL]))
      preKey = KEY_F8;
    pinSet(pinsR[F8_ROW],bcdR[F8_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F9_ROW],bcdR[F9_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F9_COL], bcdC[F9_COL]))
      preKey = KEY_F9;
    pinSet(pinsR[F9_ROW],bcdR[F9_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F10_ROW],bcdR[F10_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F10_COL], bcdC[F10_COL]))
      preKey = KEY_F10;
    pinSet(pinsR[F10_ROW],bcdR[F10_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F11_ROW],bcdR[F11_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F11_COL], bcdC[F11_COL]))
      preKey = KEY_F11;
    pinSet(pinsR[F11_ROW],bcdR[F11_ROW],_IN);

 _delay_us(5);
 
    pinSet(pinsR[F12_ROW],bcdR[F12_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F12_COL], bcdC[F12_COL]))
      preKey = KEY_F12;
    pinSet(pinsR[F12_ROW],bcdR[F12_ROW],_IN);

      if (preKey) pressFKey(preKey);
      else        sendCode(0,shiftd,symbold); //extended mode
  }//Fin del control de teclas de Funcion o extended mode

//Escaneo de filas
if(!preKey) //Si no se ha pulsado una tecla de Funcion...
{
  for (r=0;r<ROWS;r++)
  {
    //activar fila
    pinSet(pinsR[r],bcdR[r],_OUT);
    pinPut(pinsR[r], bcdR[r], LO);
    _delay_us(5);
    for (c=0;c<COLS;c++)
    { 
        if((pinStat(pinsC[c], bcdC[c]))) 
        {
	   _delay_us(10); //debounce
	   if((pinStat(pinsC[c], bcdC[c]))) 
	   {	
	     //keyPressed = 1;          
	     pressKey(r, c, shiftd, symbold);      
	   } else releaseKey(r, c, shiftd, symbold);
    	} else releaseKey(r, c, shiftd, symbold);
   }
    //desact. fila
    pinSet(pinsR[r],bcdR[r],_IN);
  }
 //fin escaneo de filas

  pinPut(pinsR[SHIFT_ROW], bcdR[SHIFT_ROW], LO);
  pinPut(pinsR[SYMBOL_ROW], bcdR[SYMBOL_ROW], LO);

}//Fin del escaneo de Filas
 
 _delay_ms(100);

}

int main() 
{
	matrixInit();
	ps2Init();
	while(1) {
	  matrixScan();
	}
}