/*
 *    Conversor teclado ZX-spectrum 8x5 -> PS/2 de Neuro (Codigo original de Quest)
 *
 *    Combinaciones especiales con "SYMBOL_SHIFT" + "CAPS_SHIFT" + Tecla
 *    - Reset (Ctrl+Alt+Del) = 7
 *    - Reset Maestro (Ctrl+Alt+Backspace) = 8
 *    - RGB/VGA (ScrollLock) = 6
 *    - NMI (Ctrl+Alt+F5)= 5
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
//{PB2=10, PB3=11, PB4=12, PC0=A0, PC1=A1};
uint8_t pinsC[COLS] =  {2, 3, 4, 0, 1};
uint8_t bcdC[COLS] =   {2, 2, 2, 3, 3};

//{PD2=2, PD3=3, PD4=4, PD5=5, PD6=6, PD7=7, PB0=8, PB1=9};
uint8_t pinsR[ROWS] =  {2, 3, 4, 5, 6, 7, 0, 1};
uint8_t bcdR[ROWS] =   {4, 4, 4, 4, 4, 4, 2, 2};

//Swiches {PC2=A2 / PC3=A3}; 
uint8_t pinsSW[NUMSW] =  {2, 3};
uint8_t bcdSW[NUMSW] =   {3, 3};

//Teclas Pulsadas en el ultimo pase
uint8_t matriz[ROWS][COLS] = { {0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0}}; //0=Sin pulsar, 1=Marcada para Soltar, 2=Recien Pulsado, 3=Mantenido
uint8_t botones[NUMSW] = {0,0};

unsigned char espera = 0;
unsigned char fnpulsada = 0;
unsigned char fnpulsando = 0;

uint8_t modoPC=0; //Modo teclado 0=ZX / 1=PC (con mapeo de tabla caps shift y symbol shift (sin implementar aun)

//Teclas Modificadoras (para teclado spanish)
//unsigned char CAPS_SHIFT = KEY_LWIN; //Caps Shift (necesita E0)
//unsigned char SYMBOL_SHIFT = KEY_RWIN; //Symbol Shift (necesita E0)
//unsigned char mod_ctrl = 0; //No lleva mod E0 para el shift y el control de ps/2 (Usa los Izquierdos)

//Teclas Modificadoras (para teclado spectrum)
unsigned char CAPS_SHIFT = KEY_LSHIFT;; //Caps Shift (necesita E0)
unsigned char SYMBOL_SHIFT = KEY_LCTRL; //Symbol Shift (necesita E0)
unsigned char mod_ctrl = 1; //lleva mod E0 para el shift y el control de ps2(Usa los Derechos)

//Caps Shift (CAPS_SHIFT)
#define CAPS_SHIFT_ROW 5  
#define CAPS_SHIFT_COL 4  

//Symbol Shift (SYMBOL_SHIFT)
#define SYMBOL_SHIFT_ROW 7   
#define SYMBOL_SHIFT_COL 3   

//ENTER
#define ENTER_ROW 6
#define ENTER_COL 4 

//SPACE (Escape)
#define SPACE_ROW 7 
#define SPACE_COL 4 

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

//Row Q-T
#define Q_T_ROW 1
//Cols Q-T
#define Q_COL 4 //F11
#define W_COL 3 //Cursor Arriba
#define E_COL 2 //F12
#define R_COL 1 //RESET = Ctrl+Alt+Del
#define T_COL 0 //Master Reset = Ctrl+Alt+Backsp

//Row Y-P
#define Y_P_ROW 4
//Cols Y-P
#define Y_COL 0 //RGB-VGA Swich
#define U_COL 1 //Shift+F12 para el BBC  
#define I_COL 2 //NMI = Ctrl+Alt+F5
#define O_COL 3 //
#define P_COL 4 //

//Row A-G
#define A_G_ROW 2
//Cols A-G
#define A_COL 4 //
#define S_COL 3 //
#define D_COL 2 //
#define F_COL 1 //
#define G_COL 0 //

//Row H-L
#define H_L_ROW 6
//Cols H-L
#define H_COL 0 //
#define J_COL 1 //
#define K_COL 2 //
#define L_COL 3 //

//Row Z-V
#define Z_V_ROW 5 //
//Cols Z-V
#define Z_COL 3 //
#define X_COL 2 //
#define C_COL 1 //
#define V_COL 0 //

//Row B-M
#define B_M_ROW 7
//Cols B-M
#define B_COL 0 //
#define N_COL 1 //
#define M_COL 2 //


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
//Uso normal: CK1 = 20, CK2 = 40 // Para codigo sin optimizar (x12) CK1 = 240, CK2 = 480. 
#define CK1 4
#define CK2 8

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
		 
         _delay_us(50); //fin
         //_delay_us(CK2*3); //fin
        
LED_OFF;
}

//preparamos matriz teclado.
void matrixInit()
{
   uint8_t c, r;
    
  LED_CONFIG;
  LED_OFF;

  for (c=0;c<NUMSW;c++)
  { 
    pinSet(pinsSW[c], bcdSW[c],_IN);
    pinPut(pinsSW[c], bcdSW[c], HI);
  }

  for (c=0;c<COLS;c++)
  { 
    pinSet(pinsC[c],bcdC[c],_IN);
    pinPut(pinsC[c], bcdC[c], HI);
  }

  for (r=0;r<ROWS;r++)
    pinSet(pinsR[r],bcdR[r],_IN);
}

void pulsafn(unsigned char row, unsigned char col, unsigned char mod_E0, unsigned char key, unsigned char key_E0, unsigned char shift, unsigned char ctrl, unsigned char alt)
{ //Quita la pulsacoin de Caps Shift o Symbol Shift si mod_E0=0 necesita E0 xq son las teclas de windows, si mod_E0=1 no lo necesita porque son control y shift izdo.
  if(matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]==1 || matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]==3)         {if(!mod_E0) sendPS2(0xE0); sendPS2(0xF0); sendPS2(CAPS_SHIFT);   matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]=0;     espera++;}
  if(matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]==1 || matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]==3) {if(!mod_E0) sendPS2(0xE0); sendPS2(0xF0); sendPS2(SYMBOL_SHIFT); matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]=0; espera++;}

  if(espera) {_delay_us(5); espera=0;}
  if(shift)  {sendPS2(KEY_RSHIFT); espera++;} //El Shift derecho no necesita E0
  if(ctrl)   {sendPS2(0xE0); sendPS2(KEY_RCTRL);  espera++;} //Se manda E0 para el control derecho (que vale para ambos casos)
  if(alt)    {sendPS2(KEY_LALT);   espera++;} //Usamos el Alt izdo siempre
  if(espera) {_delay_us(5); espera=0;}
  if(key_E0) {sendPS2(0xE0);} //La tecla requiere modo E0 del PS2
  sendPS2(key);
  _delay_us(5); 
  if(key_E0) {sendPS2(0xE0);}
  sendPS2(0xF0); 
  sendPS2(key);  
  matriz[row][col]=0;
  if(shift) {sendPS2(0xF0); sendPS2(KEY_RSHIFT);}
  if(ctrl)  {sendPS2(0xE0); sendPS2(0xF0); sendPS2(KEY_RCTRL);}
  if(alt)   {sendPS2(0xF0); sendPS2(KEY_LALT);}
  _delay_us(5);
  fnpulsada=1;  
  fnpulsando=1;
}

void matrixScan()
{
  uint8_t r, c, b;

  fnpulsada=0; //Se pone a 0 la pulsacion de una tecla de funcion

  for (b=0;b<NUMSW;b++)
  {
   if(pinStat(pinsSW[b], bcdSW[b]))  //Controla los Botones Externos del Arduino
   {
    _delay_us(10); //debounce
     if(pinStat(pinsSW[b], bcdSW[b])) 
	  {
  		if(b==0) //Soft Reset en prueba 
		 {
	      sendPS2(KEY_A);_delay_us(5); 
		  sendPS2(0xF0); sendPS2(KEY_A); _delay_us(5); 
          fnpulsada=1;
   	     }
   		if(b==1) //NMI en Prueba
		 {
	      sendPS2(KEY_B);_delay_us(5); 
		  sendPS2(0xF0); sendPS2(KEY_B); _delay_us(5); 
          fnpulsada=1;
		  }
	  } 
   } 
  }

  if (!fnpulsada) {for (r=0;r<ROWS;r++)
  {
    //activar row/fila
    pinSet(pinsR[r],bcdR[r],_OUT);
    pinPut(pinsR[r], bcdR[r], LO);
    _delay_us(5);
    for (c=0;c<COLS;c++)
    { 
        if(pinStat(pinsC[c], bcdC[c])) 
        {
	     _delay_us(10); //debounce
	     if(pinStat(pinsC[c], bcdC[c])) 
	     {	
	      if(matriz[r][c]==2)       { matriz[r][c]=3;} //Pasa de pulsado a mantenido  "3"
	      if(matriz[r][c]==0)       { matriz[r][c]=2;} //Pasa de sin pulsar a pulsado "2"
	     } else if(matriz[r][c]!=0) { matriz[r][c]=1;} //Marcado para soltar la tecla "1" (si entra por debounce)
	    } else if(matriz[r][c]!=0)  { matriz[r][c]=1;} //Marcado para soltar la tecla "1"
   }//Fin de Escaneo de las Columnas para el Row/Fila indicado
    //desact. row/fila
    pinSet(pinsR[r],bcdR[r],_IN);
  }} //fin escaneo de Rows/Filas
  
  //Comprobacion de Teclas especiales al tener pulsado Caps Shift y Symbol Shift
  if (!fnpulsada) {if(matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]>1 && matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]>1)
  {
//funcion(row,col,key,key_e0,shift,control,alt) Prueba con pocas teclas
// Intento de Mapear muchas teclas especiales
// /*
  if(!fnpulsando) //Para evitar la pulsacion multiple de una tecla especial, solo 1 por cada toque.
   {
   //Tecla Escape
   if(matriz[SPACE_ROW][SPACE_COL]>1) pulsafn(SPACE_ROW,SPACE_COL,mod_ctrl,KEY_ESCAPE,0,0,0,0); //Escape
  //Teclas F1..F10 (Fila Primera del Spectrum, desde Arriba)
   if(matriz[F1_F5_ROW][F1_COL]>1) pulsafn(F1_F5_ROW,F1_COL,mod_ctrl,KEY_F1,0,0,0,0);  //F1
   if(matriz[F1_F5_ROW][F2_COL]>1) pulsafn(F1_F5_ROW,F2_COL,mod_ctrl,KEY_F2,0,0,0,0);  //F2
   if(matriz[F1_F5_ROW][F3_COL]>1) pulsafn(F1_F5_ROW,F3_COL,mod_ctrl,KEY_F3,0,0,0,0);  //F3
   if(matriz[F1_F5_ROW][F4_COL]>1) pulsafn(F1_F5_ROW,F4_COL,mod_ctrl,KEY_F4,0,0,0,0);  //F4
   if(matriz[F1_F5_ROW][F5_COL]>1) pulsafn(F1_F5_ROW,F5_COL,mod_ctrl,KEY_F5,0,0,0,0);  //F5
   if(matriz[F6_F0_ROW][F6_COL]>1) pulsafn(F6_F0_ROW,F6_COL,mod_ctrl,KEY_F6,0,0,0,0);  //F6 
   if(matriz[F6_F0_ROW][F7_COL]>1) pulsafn(F6_F0_ROW,F7_COL,mod_ctrl,KEY_F7,0,0,0,0);  //F7
   if(matriz[F6_F0_ROW][F8_COL]>1) pulsafn(F6_F0_ROW,F8_COL,mod_ctrl,KEY_F8,0,0,0,0);  //F8
   if(matriz[F6_F0_ROW][F9_COL]>1) pulsafn(F6_F0_ROW,F9_COL,mod_ctrl,KEY_F9,0,0,0,0);  //F9
   if(matriz[F6_F0_ROW][F0_COL]>1) pulsafn(F6_F0_ROW,F0_COL,mod_ctrl,KEY_F10,0,0,0,0); //F10
   //(Fila Segunda del Spectrum, desde Arriba)
   if(matriz[Q_T_ROW][Q_COL]>1) pulsafn(Q_T_ROW,Q_COL,mod_ctrl,KEY_F11,0,0,0,0);     //F11 
   if(matriz[Q_T_ROW][W_COL]>1) pulsafn(Q_T_ROW,W_COL,mod_ctrl,KEY_UP,1,0,0,0);      //Cursor Arriba
   if(matriz[Q_T_ROW][E_COL]>1) pulsafn(Q_T_ROW,E_COL,mod_ctrl,KEY_F12,0,0,0,0);     //F12
   if(matriz[Q_T_ROW][R_COL]>1) pulsafn(Q_T_ROW,R_COL,mod_ctrl,KEY_DELETE,0,0,1,1);  //ZXUNO Soft Reset
   if(matriz[Q_T_ROW][T_COL]>1) pulsafn(Q_T_ROW,T_COL,mod_ctrl,KEY_BACKSP,0,0,1,1);  //ZXUNO Hard Reset
   if(matriz[Y_P_ROW][Y_COL]>1) pulsafn(Y_P_ROW,Y_COL,mod_ctrl,KEY_F12,0,1,0,0);     //BBC Micro Software Menu (Como escribir *MENU)
   if(matriz[Y_P_ROW][U_COL]>1) pulsafn(Y_P_ROW,U_COL,mod_ctrl,KEY_SCRLCK,0,0,0,0);  //ZXUNO RGB/VGA Swich
   if(matriz[Y_P_ROW][I_COL]>1) pulsafn(Y_P_ROW,I_COL,mod_ctrl,KEY_F5,0,0,1,1);      //ZXUNO NMI
   if(matriz[Y_P_ROW][O_COL]>1) pulsafn(Y_P_ROW,O_COL,mod_ctrl,KEY_PTOCOMA,0,0,0,0); // ;
   if(matriz[Y_P_ROW][P_COL]>1) pulsafn(Y_P_ROW,P_COL,mod_ctrl,KEY_P,0,0,0,0);
   //(Fila Tercera del Spectrum, desde Arriba)
   if(matriz[A_G_ROW][A_COL]>1) pulsafn(A_G_ROW,A_COL,mod_ctrl,KEY_LEFT,1,0,0,0);    //Cursor Izda
   if(matriz[A_G_ROW][S_COL]>1) pulsafn(A_G_ROW,S_COL,mod_ctrl,KEY_DOWN,1,0,0,0);    //Cursor Abajo
   if(matriz[A_G_ROW][D_COL]>1) pulsafn(A_G_ROW,D_COL,mod_ctrl,KEY_RIGHT,1,0,0,0);   //Cursor Derecha
   if(matriz[A_G_ROW][F_COL]>1) pulsafn(A_G_ROW,F_COL,mod_ctrl,KEY_F,0,0,0,0);
   if(matriz[A_G_ROW][G_COL]>1) pulsafn(A_G_ROW,G_COL,mod_ctrl,KEY_G,0,0,0,0);
   if(matriz[H_L_ROW][H_COL]>1) pulsafn(H_L_ROW,H_COL,mod_ctrl,KEY_H,0,0,0,0);
   if(matriz[H_L_ROW][J_COL]>1) pulsafn(H_L_ROW,J_COL,mod_ctrl,KEY_J,0,0,0,0);
   if(matriz[H_L_ROW][K_COL]>1) pulsafn(H_L_ROW,K_COL,mod_ctrl,KEY_K,0,0,0,0);
   if(matriz[H_L_ROW][L_COL]>1) pulsafn(H_L_ROW,L_COL,mod_ctrl,KEY_L,0,0,0,0);
   //(Fila Cuarta del Spectrum, desde Arriba)
   if(matriz[Z_V_ROW][Z_COL]>1) pulsafn(Z_V_ROW,Z_COL,mod_ctrl,KEY_PTOCOMA,0,1,0,0); // :
   if(matriz[Z_V_ROW][X_COL]>1) pulsafn(Z_V_ROW,X_COL,mod_ctrl,KEY_X,0,0,0,0);
   if(matriz[Z_V_ROW][C_COL]>1) pulsafn(Z_V_ROW,C_COL,mod_ctrl,KEY_C,0,0,0,0);
   if(matriz[Z_V_ROW][V_COL]>1) pulsafn(Z_V_ROW,V_COL,mod_ctrl,KEY_V,0,0,0,0);
   if(matriz[B_M_ROW][B_COL]>1) pulsafn(B_M_ROW,B_COL,mod_ctrl,KEY_CCORCHE,0,0,0,0); // *
   if(matriz[B_M_ROW][N_COL]>1) pulsafn(B_M_ROW,N_COL,mod_ctrl,KEY_COMA,0,0,0,0);    // , 
   if(matriz[B_M_ROW][M_COL]>1) pulsafn(B_M_ROW,M_COL,mod_ctrl,KEY_PUNTO,0,0,0,0);   // .
   }
//Fin del Intento de Mapear Muchas Teclas especiales
//*/
  }else fnpulsando=0;}//Fin de Comprobacion de Teclas Especiales
  
  if(!fnpulsada) //Si no se ha pulsado ningun tecla de funcion
  {  
   //Enviar la pulsacion de Caps Shift y/o Symbol Shift si no se trata de una funcion especial (Necesita de E0 cuando son las teclas LWIN y RWIN y no lo necesita cuando son LCTRL y LSHIFT)
   if(matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]==2)     {if(!mod_ctrl) sendPS2(0xE0); sendPS2(CAPS_SHIFT);    matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]=3;     espera++;}
   if(matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]==2) {if(!mod_ctrl) sendPS2(0xE0); sendPS2(SYMBOL_SHIFT);  matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]=3; espera++;}
   if(matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]==1)     {if(!mod_ctrl) sendPS2(0xE0); sendPS2(0xF0); sendPS2(CAPS_SHIFT);    matriz[CAPS_SHIFT_ROW][CAPS_SHIFT_COL]=0;     espera++;}
   if(matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]==1) {if(!mod_ctrl) sendPS2(0xE0); sendPS2(0xF0); sendPS2(SYMBOL_SHIFT);  matriz[SYMBOL_SHIFT_ROW][SYMBOL_SHIFT_COL]=0; espera++;}
   if(espera) {_delay_us(5); espera=0;}
 
   //Enviar el resto de Teclas Pulsadas
   for(r=0;r<ROWS;r++) for(c=0;c<COLS;c++)
   {
    if(matriz[r][c]==2) {sendPS2(mapZX[r][c]);}
    if(matriz[r][c]==1) {sendPS2(0xF0); sendPS2(mapZX[r][c]);matriz[r][c]=0;}
   }
  }
 //if(espera) {_delay_us(5); espera=0;}
 
}//FIN de Matrixscan


int main() 
{
	matrixInit();
	ps2Init();
	while(1) {
	  matrixScan();
	}
}