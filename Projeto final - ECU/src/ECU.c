/*
 * Nome do Projeto: Controle de Modo Normal e Turbo de um acelerador eletrônico com leitura de ADC e Display LCD
 * Autor:           Ítalo Silva
 * Data:            04/06/2024
 * Descrição:       Este projeto implementa um acelerador que alterna entre dois modos de operação: Normal e Turbo.
 *                  No modo Normal o acelerador alcança 100%.
 *                  No modo Turbo o acelerador pode alcançar 115% por um tempo de 15 segundos quando volta ao modo 
 *                  Normal para evitar stress térmico. O led indicador de modo Turbo piscará por 5 segundos indicando que 
 *                  que o sistema voltará a operar em modo Normal.
 *                  O modo Turbo foi projetado para ser auxiliar de ultrapassagens em carros.
 */

//Bits de configuração
#pragma config FOSC = HS                            // Oscillator Selection bits (HS oscillator);
#pragma config WDTE = OFF                           // Watchdog Timer Enable bit (WDT disabled) - trocar para on quando terminar de testar;
#pragma config PWRTE = OFF                          // Power-up Timer Enable bit (PWRT disabled);
#pragma config BOREN = ON                           // Brown-out Reset Enable bit (BOR enabled);
#pragma config LVP = OFF                            // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming);
#pragma config CPD = OFF                            // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off);
#pragma config WRT = OFF                            // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control);
#pragma config CP = OFF                             // Flash Program Memory Code Protection bit (Code protection off);

#include <stdio.h>
#include <stdbool.h>
#include <xc.h>
#include "lcd.h"

#define _XTAL_FREQ 4000000                          // Frequência do oscilador;
#define NORMAL_PIN RC3                              // Pino de modo Normal;
#define TURBO_PIN RC4                               // Pino de modo Turbo;

// Variável global
volatile unsigned char mode = 0;                    // 0: Normal, 1: Turbo;
volatile unsigned long int turbo_timer = 0;         // Contador de interrupções do Timer 1;
volatile bool turbo_active = false;                 // Flag que dispara em 115% do modo Turbo;
volatile unsigned int timer_counter = 0;            // Contador de 5 segundos para finalizar modo Turbo;
volatile float acel_percent;                        // Valor do acelerador em %;

// Função para inicializar o ADC
void init_ADC() {
    ADCON0 = 0x41;                                  // Configura AN0 como entrada analógica e liga o ADC;
    ADCON1 = 0x80;                                  // Configura todas as portas como entradas analógicas;
}

// Função para ler o valor do ADC
unsigned int read_ADC() {
    ADCON0bits.GO_DONE = 1;                         // Inicia a conversão;
    while (ADCON0bits.GO_DONE);                     // Aguarda a conversão terminar;
    return ((unsigned int)(ADRESH << 8) + ADRESL);  // Retorna o resultado da conversão;
}

// Função para inicializar a interrupção externa
void init_interrupt() {
    INTCONbits.GIE = 1;                             // Habilita interrupções globais;
    INTCONbits.PEIE = 1;                            // Habilita interrupções periféricas;
    INTCONbits.INTE = 1;                            // Habilita interrupção externa;
    OPTION_REGbits.INTEDG = 0;                      // Interrupção na borda de descida;
    PIE1bits.TMR1IE = 1;                            // Habilita interrupção Timer1;
}

// Função para inicializar o Timer 1 (contador de 16 bits);
void init_timer(){
   T1CONbits.TMR1CS = 0;                            // Define timer 1 como temporizador (Fosc/4);
   T1CONbits.T1CKPS0 = 1;                           // Bit pra configurar pre-escaler, nesta caso 1:8;
   T1CONbits.T1CKPS1 = 1;                           // Bit pra configurar pre-escaler, nesta caso 1:8;
   TMR1L = 0xDC;                                    // Carrega valor inicial no contador (65536-62500);
   TMR1H = 0x0B;                                    // Quando estourar contou 62500, passou 0,5s;
   T1CONbits.TMR1ON = 1;                            //Liga o timer
}

// Rotina de Serviço de Interrupção
void __interrupt() ISR() {
    if (INTCONbits.INTF) {
        mode = !mode;                               // Alterna o modo entre normal e turbo;
        INTCONbits.INTF = 0;                        // Limpa a flag de interrupção;
    }
    
    if (PIR1bits.TMR1IF) {
        PIR1bits.TMR1IF = 0;                        // Reseta o flag da interrupção;
        TMR1L = 0xDC;                               // Configura o byte baixo do Timer 1;
        TMR1H = 0x0B;                               // Configura o byte alto do Timer 1;
                                                    // o contador inicia em 0x0BDC = 3036;
        if (turbo_active) {
            turbo_timer++;                          // Conta as interrupções do Timer 1 desde o disparo da flag "turbo_active";
            if (turbo_timer >= 20) {                // 10 segundos se passaram (20 * 0.5s);
                turbo_active = false;               // Limpa a flag;
                turbo_timer = 0;                    // Reinicia o contador de interrupções;
                timer_counter = 10;                 // 5 segundos (10 * 0.5s)
            }
        } else if (timer_counter > 0) {
            timer_counter--;                        // Inicia contagem regressiva de 5 segundos para desabilitar modo Turbo;
            TURBO_PIN = !TURBO_PIN;                 // Pisca o LED indicador de mudança para modo Normal;
            __delay_ms(500);                       // Aguarda 1 segundo e testa a condição "timer_counter"
            if (timer_counter == 0) {
                mode = 0;                           // Volta para modo normal
                TURBO_PIN = 0;                      // Desliga o LED do modo Turbo;
            }
        }
    }
}

int main(void) {
    TRISC = 0x00;                                   // Configura PORTC como outputs;
    NORMAL_PIN = 1;                                 // LED indicador de modo Normal ativo;
    
    
    LCD lcd = { &PORTD, 2, 3, 4, 5, 6, 7 };         // PORT, RS, EN, D4, D5, D6, D7 - Seleciona PORTD como saída para o LCD;
    LCD_Init(lcd);                                  //Inicializa o ADC;
    init_interrupt();                               // Inicializa a interrupção externa;
    init_ADC();                                     // Inicializa o ADC;
    init_timer();                                   // Inicializa o Timer 1;
    
    char buffer[16];                                // Buffer para armazenar o texto a ser exibido no LCD;

    while(1){
        unsigned int adc_value = read_ADC();        // Lê o valor do ADC;
        
        if (mode == 0) {                           
            NORMAL_PIN = 1;
            TURBO_PIN = 0;
            acel_percent = (adc_value / 889.56f) * 100;
            if (acel_percent > 100) acel_percent = 100;
        } else {                                   
            
            TURBO_PIN = 1;
            NORMAL_PIN = 0;
            acel_percent = (adc_value / 1023.0f) * 115;
            if (acel_percent > 115) acel_percent = 115;
            if (acel_percent == 115 && !turbo_active && timer_counter == 0) {
                turbo_active = true;
                turbo_timer = 0;
            }
        }
        
        LCD_Clear();
        
        LCD_Set_Cursor(0, 0);                       //define o cursor e exibe a leitura do potenciômetro;
        sprintf(buffer, "Acel: %.1f%%", acel_percent);
        LCD_putrs(buffer);
        
        LCD_Set_Cursor(1, 0);                       // Define o cursor e exibe o modo atual;
        if (mode == 0) {
            LCD_putrs("Modo: Normal");
        } else {
            LCD_putrs("Modo: Turbo");
        }
        
        __delay_ms(200);                            // Aguarda 200ms antes de ler novamente
    }
    return (EXIT_SUCCESS);
}