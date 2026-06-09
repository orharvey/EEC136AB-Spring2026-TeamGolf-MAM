
// EEC 136B
// SWR Code
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <ti/grlib/grlib.h>
#include <stdio.h>
#include <stdint.h>
//#include <msp432.h>
#include "lcd16x2_msp43x.h"
#include <stdbool.h>
#include <math.h>

//#include "msp.h"
//#include <msp432.h>
//*****************************************************************************
#define num_Samples 512
#define min_Power 0.000001f     //0.000001f //0.001f // can change but do not make too LOW
float Vfwd, Vrev;
float Pfwd, Prev, SWR, Gamma, Returnloss;

const float VREF= 3.3f;
const float a= 1.0f;
const float b= 0.75f;

void initUART(void);
void uart_puts(char *s);

int i;
int w;

void initUART(void)
{
    // Configure P3.2 (RX) and P3.3 (TX) for EUSCI_A2
    P3->SEL0 |= BIT2 | BIT3;
    P3->SEL1 &= ~(BIT2 | BIT3);

    // Put UART in reset
    EUSCI_A2->CTLW0 |= EUSCI_A_CTLW0_SWRST;

    // Use SMCLK
    EUSCI_A2->CTLW0 |= EUSCI_A_CTLW0_SSEL__SMCLK;

    // 9600 baud for HC-05 (assuming ~3 MHz SMCLK)
    EUSCI_A2->BRW = 19;
    EUSCI_A2->MCTLW = (8 << EUSCI_A_MCTLW_BRF_OFS) | EUSCI_A_MCTLW_OS16;

    // Take UART out of reset
    EUSCI_A2->CTLW0 &= ~EUSCI_A_CTLW0_SWRST;
}
////////////////////////////////////////////////////////////////

void uart_puts(char *s)
{
    while (*s) {
        while (!(EUSCI_A2->IFG & EUSCI_A_IFG_TXIFG));
        EUSCI_A2->TXBUF = *s++;
    }
}

//////////////////////////////////////////////////////////////////////


int main(void)
  {
    uint16_t adc0Result;
    uint16_t adc1Result;
    char buffer[64];

    WDT_A_holdTimer();//pet the dog

    P4->DIR = 0xFF; //make p4 pins as output for data and controls FOR LCD
    P4->OUT = 0x00; //clear all pins FOR LCD


   // /* ADC init
     MAP_ADC14_enableModule();
     MAP_ADC14_initModule(ADC_CLOCKSOURCE_MCLK,
                          ADC_PREDIVIDER_1,
                          ADC_DIVIDER_2,  //MESS With divider was 4 originally
                          0);

    // /* P5.5 = A0, P5.4 = A1
     MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5,
                                                    GPIO_PIN5 | GPIO_PIN4,
                                                    GPIO_TERTIARY_MODULE_FUNCTION);



  /*   MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5,
                                                    GPIO_PIN5,
                                                    GPIO_TERTIARY_MODULE_FUNCTION);

     MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6,
                                                    GPIO_PIN1,
                                                    GPIO_TERTIARY_MODULE_FUNCTION);
*/

    // /* Sequence mode: MEM0 then MEM1
     MAP_ADC14_configureMultiSequenceMode(ADC_MEM0, ADC_MEM1, true);

     MAP_ADC14_configureConversionMemory(ADC_MEM0,
                                         ADC_VREFPOS_AVCC_VREFNEG_VSS,
                                         ADC_INPUT_A0,
                                         false);

     MAP_ADC14_configureConversionMemory(ADC_MEM1,
                                         ADC_VREFPOS_AVCC_VREFNEG_VSS,
                                         ADC_INPUT_A1,
                                         false);



   /*  MAP_ADC14_configureConversionMemory(ADC_MEM1,
                                         ADC_VREFPOS_AVCC_VREFNEG_VSS,
                                         ADC_INPUT_A14,
                                         false);
*/
     MAP_ADC14_enableSampleTimer(ADC_MANUAL_ITERATION);
     MAP_ADC14_enableConversion();

     ///* LCD init
    lcd16x2_Init();
    lcd16x2_Cmd(LCD_CMD_CLEAR_LCD);

     initUART();

     while(1)
     {
         static float filt0 = 0;
     static float filt1 = 0;
     static bool firstRun = true;

     uint32_t sum0 = 0;
     uint32_t sum1 = 0;

     for(i = 0; i < num_Samples; i++)
     {
         MAP_ADC14_toggleConversionTrigger();
         while (MAP_ADC14_isBusy());

         sum0 += MAP_ADC14_getResult(ADC_MEM0);
         sum1 += MAP_ADC14_getResult(ADC_MEM1);
     }

     float avg0 = (float)sum0 / num_Samples;
     float avg1 = (float)sum1 / num_Samples;

     if(firstRun)
     {
         filt0 = avg0;
         filt1 = avg1;
         firstRun = false;
     }
     else
     {
         filt0 = 0.80f * filt0 + 0.20f * avg0;
         filt1 = 0.80f * filt1 + 0.20f * avg1;
     }

     adc0Result = (uint16_t)filt0;
     adc1Result = (uint16_t)filt1;

         // convert 14 bit ADC result to voltage
         Vfwd= (adc0Result * VREF) / 16383.0f;
         Vrev= (adc1Result * VREF)/ 16383.0f;

        Vfwd= Vfwd;
        Vrev= Vrev;//
        printf("Vfwd: %.3f\n", Vfwd);
        printf("Vrev: %.3f\n", Vrev);

         // Compute Power
        // Pfwd= a *(Vfwd * Vfwd) + (Vfwd*b);
         //Prev= a *(Vrev * Vrev) + (Vrev*b);


        // Pfwd = pow(20*Vfwd,2)/400;
         Pfwd=(Vfwd * Vfwd);
         Prev= Vrev * Vrev; // impedances cancel with transformer ratio
       //  Prev = pow(20*Vrev,2)/400;


         if (Pfwd > min_Power) {

             if (Prev < min_Power) {
                 Prev = 0.0f;
                 Gamma = 0.0f;
                 SWR = 1.0f;
             }
             else if (Prev < Pfwd) {
                 Gamma = sqrtf(Prev / Pfwd);
                 SWR = (1.0f + Gamma) / (1.0f - Gamma);

                 if (SWR > 99.9f) {
                     SWR = 99.9f;
                 }
             }
             else {
                 Gamma = 0.999f;
                 SWR = 99.9f;
             }

         }
         else {
             Pfwd = 0.0f;
             Prev = 0.0f;
             Gamma = 0.0f;
             SWR = 1.0f;
         }

         Returnloss= -20.0f * log10f(Gamma);



       lcd16x2_Cmd(LCD_CMD_CLEAR_LCD);

         lcd16x2_SetPosition(0,0);
         lcd16x2_PrintString("Gamma:");
         lcd16x2_PrintFloat(Gamma);

         lcd16x2_SetPosition(1,0);
         lcd16x2_PrintString("SWR:");
         lcd16x2_PrintFloat(SWR);


         // UART output to HC-05  MATLAB
         // sprintf(buffer, "Gamma: %.2f | SWR: %.2f\r\n", Gamma, SWR);
          sprintf(buffer, "%.3f,%.3f,%.6f,%.6f,%.3f,%.3f,%.3f\r\n", SWR, Gamma, Pfwd, Prev, Vfwd, Vrev, Returnloss);


          uart_puts(buffer);

         __delay_cycles(3000000);



     }



}



















































































































