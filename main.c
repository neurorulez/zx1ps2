/*
 *    Conversor teclado ZX-spectrum 8x5 -> PS/2 de Neuro (Codigo original de Quest)
 *
 *    Combinaciones especiales: 
 *	  - F-Keys = SYMBOL+CAOS+1..B (1..9=F1..F9, 0=F10, A=F11, B=F12)
 *    - Reset Maestro (Ctrl+Alt+Backspace) = SYMBOL+CAPS+Z
 *    - Reset (Ctrl+Alt+Del) = SYMBOL+CAPS+X
 *    - NMI (Ctrl+Alt+F5)= SYMBOL+CAPS+C
 *    - RGB/VGA (ScrollLock) = SYMBOL+CAPS+V
 */

#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h>
#include "keymaps.h"

#define LED_ON	PORTB |= (1<<5)		
#define LED_OFF	PORTB &= ~(1<<5)
#define LED_CONFIG	DDRB |= (1<<5) //Led en PB5 en Pro mini y similares

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
uint8_t pinsC[COLS] =  {2, 3, 4, 0, 1};
uint8_t bcdC[COLS] =   {2, 2, 2, 3, 3};

//{PD2, PD3, PD4, PD5, PD6, PD7, PB0, PB1};
uint8_t pinsR[ROWS] =  {2, 3, 4, 5, 6, 7, 0, 1};
uint8_t bcdR[ROWS] =   {4, 4, 4, 4, 4, 4, 2, 2};

//Teclas Pulsadas en el ultimo pase
uint8_t antkey[0xFF]={0}; //Estado anterior del codigo "key" 0 Sin pulsar / 1 Pulsado
uint8_t antcs=0;  //Estado anterior de caps shift
uint8_t antss=0;  //Estado anterior de symbol shift
uint8_t modoPC=0; //Modo teclado 0=ZX / 1=PC (con mapeo de tabla caps shift y symbol shift

//Teclas Modificadoras
unsigned char key_mod1 = KEY_LSHIFT; //Caps Shift
unsigned char key_mod2 = KEY_LCTRL; //Symbol Shift

//Caps Shift (key_mod1)
#define SHIFT_ROW 5  
#define SHIFT_COL 4  
//Tecla Z dentro del mismo ROW que Caps Shift (para intercambiar entre modo ZX y modo PC)
#define Fz_COL 3

//Symbol Shift (key_mod2)
#define SYMBOL_ROW 7   
#define SYMBOL_COL 3   




//Row F1..F5
#define F1_F5_ROW 0
//Cols F1..F5
#define F1_COL 4 //F1
#define F2_COL 3 //F2
#define F3_COL 2 //F3
#define F4_COL 1 //F4
#define F5_COL 0 //F5

//Row F6..F0
#define F6_F0_ROW 3
//Cols F6..F0
#define F6_COL 0 //F6
#define F7_COL 1 //F7
#define F8_COL 2 //F8
#define F9_COL 3 //F9
#define F0_COL 4 //F10

//Row Fq-Ft
#define Fq_Ft_ROW 1
//Cols Fq-Ft
#define Fq_COL 4 //F11
#define Fw_COL 3 //F12
#define Fe_COL 2 //NMI = Ctrl+Alt+F5
#define Fr_COL 1 //RESET = Ctrl+Alt+Del
#define Ft_COL 0 //Master Reset = Ctrl+Alt+Backsp

//Row Fy-Fp
#define Fy_Fp_ROW 4
//Cols Fy-Fp
#define Fy_COL 0 //RGB-VGA Swich
#define Fu_COL 1 //Shift+F12 para el BBC  
#define Fi_COL 2 //
#define Fo_COL 3 //
#define Fp_COL 4 ////Cambio Modo ZX/PC


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
//zxuno v2 test23: (x12) CK1 = 240, CK2 = 480. Uso normal: CK1 = 20, CK2 = 40 
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
void sendCode(unsigned char key, uint8_t shiftd, uint8_t symbold, uint8_t keyzx) 
{
//secuencia  
   if (shiftd  && !antcs && keyzx) {sendPS2(key_mod1); antcs=1;} //Si el modo es ZX envia la tecla PS2 para Caps Shift
   if (symbold && !antss && keyzx) {sendPS2(key_mod2); antss=1;} //Si el modo es ZX envia la tecla PS2 para Symbol Shift
   //if (symbold && !keyzx) {sendPS2(KEY_LSHIFT);} //Si esta en modo PC y con tecla Symbol Shift, accede a los simbolos del teclado sacandolos con shift
   if (key)
   {
    sendPS2(key);
    antkey[key]=1;
   } 
   //if (symbold && !keyzx) {sendPS2(0xF0);sendPS2(KEY_LSHIFT);} //Si esta en modo PC y con tecla Symbol Shift, envia para quitarla despues de sacar el simbolo
//fin secuencia
}

void pressKey(uint8_t r, uint8_t c, uint8_t shiftd, uint8_t symbold)
{  
uint8_t keyzx = 1; //1=Es una tecla PS2 conseguida con tecla/s originales del zx / 0=Es una tecla PS2 conseguida con un mapeo de shift/symbol mas tecla del ZX
uint8_t key = mapZX[r][c];

//if(shiftd  && modoPC && mapShift[r][c])  {key = mapShift[r][c];  keyzx=0;}
//if(symbold && modoPC && mapSymbol[r][c]) {key = mapSymbol[r][c]; keyzx=0;}
sendCode(key,shiftd,symbold,keyzx);
}

void releaseKey(uint8_t r, uint8_t c, uint8_t shiftd, uint8_t symbold)
{  
uint8_t key = mapZX[r][c];

if (key && antkey[key]) //Entra aqui si en el pase actuañ no esta pulsada, comprueba si antes si, y manda soltarla por ps2.
 {
  sendPS2(0xF0);
  sendPS2(key);
  antkey[key]=0;
 } 

if (!shiftd && antcs) //Si no esta pulsado Caps Shift y antes lo estaba, manda soltarlo por PS2
 {
  sendPS2(0xF0);
  sendPS2(key_mod1);
  antcs=0;
 }

 if (!symbold && antss) //Si no esta pulsado Symbol Shift y antes lo estaba, manda soltarlo por PS2
 {
  sendPS2(0xF0);
  sendPS2(key_mod2);
  antss=0;
 }
}

void pressExtm(uint8_t shiftd, uint8_t symbold)
{
 //Activar modo extendido O desactivarlo si esta activo.
 sendPS2(key_mod1);
 sendPS2(key_mod2);
 _delay_us(5); 
 sendPS2(0xF0);
 sendPS2(key_mod1);
 sendPS2(0xF0);
 sendPS2(key_mod2);
}

void pressFKey(unsigned char key, uint8_t preShift, uint8_t preCtrl, uint8_t preAlt)
{
  if(preCtrl)  sendPS2(KEY_LCTRL);
  if(preAlt)   sendPS2(KEY_LALT);
  if(preShift) sendPS2(KEY_LSHIFT);
  sendPS2(key);
  _delay_us(5);
  sendPS2(0xF0); sendPS2(key);
  if(preCtrl)  {sendPS2(0xF0); sendPS2(KEY_LCTRL);  preCtrl=0;}
  if(preAlt)   {sendPS2(0xF0); sendPS2(KEY_LALT);   preAlt=0;}
  if(preShift) {sendPS2(0xF0); sendPS2(KEY_LSHIFT); preShift=0;}  
}

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
  uint8_t preKey=0,preShift=0,preCtrl=0,preAlt=0,preMODO=0;
  uint8_t r, c;

//Combinaciones especiales
  //pulsacion shift
  pinSet(pinsR[SHIFT_ROW],bcdR[SHIFT_ROW],_OUT);  
 _delay_us(5);
   if(pinStat(pinsC[SHIFT_COL], bcdC[SHIFT_COL])) shiftd = 1;
  pinSet(pinsR[SHIFT_ROW],bcdR[SHIFT_ROW],_IN);
 _delay_us(5);

  //pulsacion symbol  
  pinSet(pinsR[SYMBOL_ROW],bcdR[SYMBOL_ROW],_OUT);  
 _delay_us(5);
  if(pinStat(pinsC[SYMBOL_COL], bcdC[SYMBOL_COL])) symbold = 1;  
 _delay_us(5);
  pinSet(pinsR[SYMBOL_ROW],bcdR[SYMBOL_ROW],_IN);

  //ver si es un tecla de Funcion F1-F12 que son todas con combos de shiftd y sybold o solo es para entrar en el Extend Mode
  if (shiftd && symbold) {
    pinSet(pinsR[F1_F5_ROW],bcdR[F1_F5_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F1_COL], bcdC[F1_COL])) preKey = KEY_F1;
    if(pinStat(pinsC[F2_COL], bcdC[F2_COL])) preKey = KEY_F2;  
    if(pinStat(pinsC[F3_COL], bcdC[F3_COL])) preKey = KEY_F3;
    if(pinStat(pinsC[F4_COL], bcdC[F4_COL])) preKey = KEY_F4;
    if(pinStat(pinsC[F5_COL], bcdC[F5_COL])) preKey = KEY_F5;
    pinSet(pinsR[F1_F5_ROW],bcdR[F1_F5_ROW],_IN);
    _delay_us(5);

    pinSet(pinsR[F6_F0_ROW],bcdR[F6_F0_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[F6_COL], bcdC[F6_COL])) preKey = KEY_F6;
    if(pinStat(pinsC[F7_COL], bcdC[F7_COL])) preKey = KEY_F7;  
    if(pinStat(pinsC[F8_COL], bcdC[F8_COL])) preKey = KEY_F8;
    if(pinStat(pinsC[F9_COL], bcdC[F9_COL])) preKey = KEY_F9;
    if(pinStat(pinsC[F0_COL], bcdC[F0_COL])) preKey = KEY_F10;
    pinSet(pinsR[F6_F0_ROW],bcdR[F6_F0_ROW],_IN);
    _delay_us(5);
 
    pinSet(pinsR[Fq_Ft_ROW],bcdR[Fq_Ft_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[Fq_COL], bcdC[Fq_COL])) preKey = KEY_F11;
    if(pinStat(pinsC[Fw_COL], bcdC[Fw_COL])) preKey = KEY_F12;  
    if(pinStat(pinsC[Fe_COL], bcdC[Fe_COL])) {preKey = KEY_F5;preCtrl=1;preAlt=1;}     //NMI
    if(pinStat(pinsC[Fr_COL], bcdC[Fr_COL])) {preKey = KEY_DELETE;preCtrl=1;preAlt=1;} //RESET
    if(pinStat(pinsC[Ft_COL], bcdC[Ft_COL])) {preKey = KEY_BACKSP;preCtrl=1;preAlt=1;} //MASTER RESET
    pinSet(pinsR[Fq_Ft_ROW],bcdR[Fq_Ft_ROW],_IN);
    _delay_us(5);

    pinSet(pinsR[Fy_Fp_ROW],bcdR[Fy_Fp_ROW],_OUT);
    _delay_us(5);
    if(pinStat(pinsC[Fy_COL], bcdC[Fy_COL])) preKey = KEY_SCRLCK;                      //RGB-VGA Swich
    if(pinStat(pinsC[Fu_COL], bcdC[Fu_COL])) {preKey = KEY_F12; preShift=1;}           //Shift+F12 para el BBC  
    if(pinStat(pinsC[Fi_COL], bcdC[Fi_COL])) {preKey = KEY_F5;preCtrl=1;preAlt=1;}     //NMI   (Repetido, aun pdte de poner algo)
    if(pinStat(pinsC[Fo_COL], bcdC[Fo_COL])) {preKey = KEY_DELETE;preCtrl=1;preAlt=1;} //RESET (Repetido, aun pdte de poner algo)
    if(pinStat(pinsC[Fp_COL], bcdC[Fp_COL])) {if(modoPC) {modoPC=0; preMODO=1;} else {modoPC=1; preMODO=1;}} //Cambio Modo ZX/PC (Pdte de hacer cuando el modoZX este perfecto)
    pinSet(pinsR[Fy_Fp_ROW],bcdR[Fy_Fp_ROW],_IN);
    _delay_us(5);	
	
    if (preKey) pressFKey(preKey,preShift,preCtrl,preAlt);
    else if(!preMODO) {pressExtm(shiftd,symbold); preMODO=1;} //extended mode, si no se ha entrado en cambio de modo de teclado
  }//Fin del control de teclas de Funcion o extended mode

//Escaneo de filas
if(!preKey && !preMODO) //Si no se ha pulsado una tecla de Funcion... No se ha entrado en modo Extendido y no se ha cambiado de Modo el teclado.
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