/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"

#define and &&
#define or ||

#define variable Variables.Variable

typedef union Banderas{                                                                 
    struct Variables{                                                                   // Banderas para el control del sistema
        uint8 estado:1;                                                                 // Menu principal y PWMs
        uint8 teclaPresionada:1;                                                        // Teclado matricial
        uint8 contador:5;                                                               // Muestreo del ADC
    }Variable;
    uint16 resetear;
}banderas;
banderas Variables;

typedef struct Medidas{                                                                 // Estructura de medidas
    uint32 acumuladoADC1:17;                                                            // [0 : 83900]
    uint32 acumuladoADC2:17;                                                            // [0 : 83900]
    uint32 ADC1:12;                                                                     // [0 : 4095]
    uint32 ADC2:12;
    uint32 frecuencia:9;                                                                // [0 : 500]
    uint32 dureza:7;                                                                    // [0 : 100] 
    uint32 dureza_resultante:8;                                                         // [0 : 200]
}medidas;
medidas medida;

typedef struct Tiempo{                                                                  // Estructura de tiempo
    uint64 ms:10;                                                                       // Sólo se necesitaron mili segundos
}Tiempo;
Tiempo tiempo;

unsigned char tecla;

void menu(unsigned char menu);                                                          // Rutinas del Sistema
void sensar(void);
void pwm(void);

CY_ISR_PROTO(leerTecla);                                                                // Interrupciones del sistema
CY_ISR_PROTO(contador);  

int main(void)
{
    CyGlobalIntEnable; 
    isr_KBI_StartEx(leerTecla);                                                         // Inicializacion de interrupciones
    isr_ms_StartEx(contador);
    
    LCD_Start();                                                                        // Inicializacion de Componentes
    Contador_Start();
    ADC_Start();
    AMux_Start();
    
    LCD_ClearDisplay();                                                                 // Estado Inicial del sistema
    menu(0);

    for(;;)
    {
        if (variable.teclaPresionada == 1){                                             // Ingresa a este condicional siempre
            variable.teclaPresionada = 0;                                               // que se presione una tecla
            
            if (variable.estado == 1 and Teclado_teclaPresionada() == '#'){             // Sí se presionó la tecla '#', se almacena en la
                variable.estado = 0;                                                    // variable tecla.
                tecla = Teclado_teclaPresionada();                                      // Además se detienen los PWMs, y el sistema se
                PWM_Stop();                                                             // regresa al menu principal
                PWM_CA_Stop();
                menu(0);
            }                 
            
            // Solamente puede cambiarse la opcion de PWM desde el menu principal (estado = 0)
            // Por lo que si el estado está en 0 y se presiona alguna tecla entre el '1' y el '4'
            // se almacenará en la variable tecla
            
            else if (variable.estado == 0 and Teclado_teclaPresionada() == '1'){        // Sí se presiona la tecla '1', se acitiva el PWM configurado
                variable.estado = 1;                                                    // en "Dual Edge" y se establecen los respectivos modos de 
                tecla = Teclado_teclaPresionada();                                      // comparación para generar una alineación a la IZQUIERDA
                menu(1);                                                                // de la señal resutante
                PWM_Start();
                PWM_SetCompareMode1(PWM__B_PWM__GREATER_THAN_OR_EQUAL_TO);
                PWM_WriteCompare1(0);
                PWM_SetCompareMode2(PWM__B_PWM__GREATER_THAN_OR_EQUAL_TO);
                PWM_CA_Stop();
            }
            
            else if (variable.estado == 0 and Teclado_teclaPresionada() == '2'){        // La tecla '2' es análoga a la '1', con la diferencia de
                variable.estado = 1;                                                    // que los modos de comparación establecidos generan una
                tecla = Teclado_teclaPresionada();                                      // alineación a la DERECHA de la señal resultante
                menu(1);
                PWM_Start();
                PWM_SetCompareMode1(PWM__B_PWM__GREATER_THAN_OR_EQUAL_TO);
                PWM_WriteCompare1(0);
                PWM_SetCompareMode2(PWM__B_PWM__LESS_THAN_OR_EQUAL);
                PWM_CA_Stop();
            }
            
            else if (variable.estado == 0 and Teclado_teclaPresionada() == '3'){        // La tecla '3' desactiva el PWM de los estados anteriores,
                variable.estado = 1;                                                    // y activa el PWM_CA, el cual tiene una configuración para 
                tecla = Teclado_teclaPresionada();                                      // generar una señal alineada al CENTRO 
                PWM_Stop();
                PWM_CA_Start();
            }
            
            else if (variable.estado == 0 and Teclado_teclaPresionada() == '4'){        // La tecla '4' Desactiva el PWM alineado al CENTRO y activa
                variable.estado = 1;                                                    // el PWM configuado como "Dual Edge", para el cual se 
                tecla = Teclado_teclaPresionada();                                      // establece una configuración tal que el PWM 1 controle el eje
                menu(1);                                                                // de la IZQUIERDA, y el PWM 2 controle el eje de la DERECHA.
                Clock_2_SetDividerValue(24E6/((PWM_ReadPeriod()+1)*25)+1);                                            
                PWM_CA_Stop();                                                          // Además se establece una frecuencia fija de 25 Hz 
                PWM_Start();
                PWM_SetCompareMode1(PWM__B_PWM__LESS_THAN);
                PWM_SetCompareMode2(PWM__B_PWM__GREATER_THAN);
            }
        }
        
        if (variable.estado == 1){                                                      // Sí algun PWM se activó, el estado del sistema cambia a 1
            if (tiempo.ms%25 == 0) sensar();                                            // y se habilitan las rutinas de sensado y pwm.
            if (tiempo.ms%500 == 0) pwm();
        }
    }
}

void pwm(void){
    
    if (tecla == '1'){                                                                  // PWM Alineado a la Izquierda
                                                                                        // RUTINA PARA LA FRECUENCIA                                                               
        if (medida.ADC1 < 8) {                                                          // Se calcula la frecuencia dependiendo del valor sensado por            
            medida.frecuencia = (uint32)10*8*50/4000;                                   // el ADC1, y se establecen condiciones para la frecuencia                        
            Clock_2_SetDividerValue(24E6/((PWM_ReadPeriod()+1)*medida.frecuencia/10)+1);// mínima y máxima. Con el valor de frecuencia obtenido,
        }                                                                               // se calcula el valor del divisor del Master Clock y
        else if (medida.ADC1 > 4000){                                                   // se establece.
            medida.frecuencia = (uint32)10*4000*50/4000; 
            Clock_2_SetDividerValue(24E6/((PWM_ReadPeriod()+1)*medida.frecuencia/10)+1);
        }
        else {
            medida.frecuencia = (uint32)10*medida.ADC1*50/4000; 
        }
                                                                                        // RUTINA PARA LA DUREZA
        medida.dureza = (uint32)medida.ADC2*100/4000;                                   // Se calcula el porcentaje de dureza con el valor sensado
        if (medida.ADC2 >= 4000){                                                       // del ADC2, se definen los rangos límite y se establece
            medida.dureza = 100;                                                        // el valor del CMP 2.
            PWM_WriteCompare2(0);
        }                                                                             
        else if (medida.ADC2 < 40) PWM_WriteCompare2(PWM_ReadPeriod());               
        else PWM_WriteCompare2((PWM_ReadPeriod())-(PWM_ReadPeriod() + 1)*medida.dureza/100);  
                                                                                        // Finalmente se imprimen los valores de interés en el LCD
        menu(1);
        
    }
    
    
    else if (tecla == '2'){                                                             // PWM Alineado a la DERECHA
        
                                                                                        // RUTINA PARA LA FRECUENCIA                                                               
        if (medida.ADC1 < 8) {                                                          // Se calcula la frecuencia dependiendo del valor sensado por            
            medida.frecuencia = (uint32)10*8*50/4000;                                   // el ADC1, y se establecen condiciones para la frecuencia                        
            Clock_2_SetDividerValue(24E6/((PWM_ReadPeriod()+1)*medida.frecuencia/10)+1);// mínima y máxima. Con el valor de frecuencia obtenido,
        }                                                                               // se calcula el valor del divisor del Master Clock y
        else if (medida.ADC1 > 4000){                                                   // se establece.
            medida.frecuencia = (uint32)10*4000*50/4000; 
            Clock_2_SetDividerValue(24E6/((PWM_ReadPeriod()+1)*medida.frecuencia/10)+1);
        }
        else {
            medida.frecuencia = (uint32)10*medida.ADC1*50/4000; 
            Clock_2_SetDividerValue(24E6/((PWM_ReadPeriod()+1)*medida.frecuencia/10)+1); 
        }
        
                                                                                        // RUTINA PARA LA DUREZA
        medida.dureza = (uint32)medida.ADC2*100/4000;                                   // Se calcula el porcentaje de dureza con el valor sensado
        if (medida.ADC2 > 4000) medida.dureza = 100;                                    // por el ADC2, se establecen límites máximos y mínimos,
        if (medida.dureza == 100) PWM_WriteCompare2(PWM_ReadPeriod());                  // y con el valor resultante se calcula el CMP VALUE del PWM
        else PWM_WriteCompare2((PWM_ReadPeriod()+1)*medida.dureza/100);
        
        menu(1);                                                                        // Finalmente se imprimen los valores de interes en el ADC
    }
    
    else if (tecla == '3'){                                                             // PWM ALINEADO AL CENTRO
                                                                                        // RUTINA PARA LA FRECUENCIA 
        if (medida.ADC1 < 8) {                                                          // Se emplea el mismo cálculo descrito previamente,
            medida.frecuencia = (uint32)10*8*50/4000;                                   // con la diferencia de que se tiene en cuenta el 
            Clock_2_SetDividerValue(24E6/(PWM_CA_ReadPeriod()*medida.frecuencia/10)+1); // período establecido para el PWM_CA
        }
        else if (medida.ADC1 > 4000){
            medida.frecuencia = (uint32)10*4000*50/4000; 
            Clock_2_SetDividerValue(24E6/(PWM_CA_ReadPeriod()*medida.frecuencia/10)+1);
        }
        else {
            medida.frecuencia = (uint32)10*medida.ADC1*50/4000; 
            Clock_2_SetDividerValue(24E6/(PWM_CA_ReadPeriod()*medida.frecuencia/10)+1); 
        }
        
                                                                                        // RUTINA PARA LA DUREZA
        medida.dureza = (uint32)medida.ADC2*100/4000;                                   // Rutina similar a la ya descrita
        if (medida.ADC2 > 4000) medida.dureza = 100;
        if (medida.dureza == 100) PWM_CA_WriteCompare(PWM_CA_ReadPeriod());
        else PWM_CA_WriteCompare((PWM_CA_ReadPeriod()+1)*medida.dureza/100);
        
        menu(1);
    }
    
    else if (tecla == '4'){                                                             // PWM COMO DUAL EDGE
        menu(2);                                                                        // Inicialmente se llama la funcion menu, que imprime
                                                                                        // una plantilla de base sobre la que se sobreescriben
                                                                                        // luego los valores.
        medida.dureza = (uint32)medida.ADC1*100/4000;
        if (medida.ADC1 > 4000){                                                        // RUTINA PARA LA DUREZA 1
            medida.dureza = 100;                                                        // Inicialmente se calcula el porcentaje de dureza 
            PWM_WriteCompare1(PWM_ReadPeriod());                                        // generado por el ADC1, y se limitan los rangos
        }                                                                               // para establecer el CMP VALUE del PWM1
        else PWM_WriteCompare1((PWM_ReadPeriod()+1)*medida.dureza/100);
        
        
        LCD_Position(0,7);                                                              // Se imprime el valor de dureza calculado
        if (medida.dureza < 10) LCD_PutChar('0');
        LCD_PrintNumber(medida.dureza);
                                                                                        // RUTINA PARA LA DUREZA 2
        medida.dureza = (uint32)medida.ADC2*100/4000;                                   // Y se procede a realizar el mismo cálculo de dureza con
        if (medida.ADC2 >= 4000){                                                       // el ADC2 empleando la misma variable para "reciclarla"
            medida.dureza = 100;
            PWM_WriteCompare2(0);
        }                                                                               // luego se limitan los rangos de medida y se establece
        else if (medida.ADC2 < 40) PWM_WriteCompare2(PWM_ReadPeriod());                 // el CMP VALUE del PWM2
        else PWM_WriteCompare2((PWM_ReadPeriod())-(PWM_ReadPeriod() + 1)*medida.dureza/100);  
                                                                                    
        LCD_Position(1,7);
        if (medida.dureza < 10) LCD_PutChar('0');                                       // Sí la dureza es menor a 10, se imprime un 0, para 
        LCD_PrintNumber(medida.dureza);                                                 // mantener un formato de presentacion de datos mayor 
                                                                                        // a 1 cifra        
        LCD_Position(2,15);
                                                                                        // DUREZA RESULTANTE  
        medida.dureza_resultante = ((PWM_ReadCompare1()+1)*100/PWM_ReadPeriod() + (PWM_ReadPeriod()+1 - PWM_ReadCompare2())*100/PWM_ReadPeriod());
        if (medida.dureza_resultante > 100){                                            // Finalmente, se calcula el porcentaje de dureza para
            LCD_PrintNumber(medida.dureza_resultante - 100);                            // la señal resultante del PWM1 & PWM2
        }
        else LCD_PrintNumber(0);
        LCD_PutChar('%');
    }
}

void sensar(void){
    variable.contador++;                                                                // La variable contador se encarga de sensar 20 veces
                                                                        
    AMux_Select(0);                                                                     // Rutina de sensado para el ADC1
    ADC_StartConvert();                                                         
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
    medida.acumuladoADC1 += ADC_GetResult16();                          
    
    AMux_Select(1);                                                                     // Rutina de sensado para el ADC2
    ADC_StartConvert();
    ADC_IsEndConversion(ADC_WAIT_FOR_RESULT);
    medida.acumuladoADC2 += ADC_GetResult16();                                               
    
    if (variable.contador == 20){                                                       // Cuando se ha sensado 20 veces
        variable.contador = 0;                                                          // se hace un promedio de los 
        medida.acumuladoADC1/=20;                                                       // registros y se reinician las variables acumuladas
        medida.acumuladoADC2/=20;
        
        medida.ADC1 = medida.acumuladoADC1;
        medida.ADC2 = medida.acumuladoADC2;
        
        medida.acumuladoADC1 = 0;
        medida.acumuladoADC2 = 0;
    }
}

void menu(unsigned char menu){
    switch(menu){                                                                       // MENU DEL SISTEMA
        case 0:                                                                         // Menu = 0: menu principal
            LCD_ClearDisplay();
            LCD_Position(0,0);LCD_PrintString("Left-aligned");
            LCD_Position(1,0);LCD_PrintString("Right-aligned");
            LCD_Position(2,0);LCD_PrintString("Center-aligned");
            LCD_Position(3,0);LCD_PrintString("Dual-edged");
            break;
        case 1:                                                                         // Menu = 1: Menu para las 3 primeras 
            LCD_ClearDisplay();                                                         // opciones
            LCD_Position(1,0);
            LCD_PrintString("ADC1:");
            LCD_PrintNumber(medida.ADC1);
            
            LCD_Position(2,0);
            LCD_PrintString("ADC2:");
            LCD_PrintNumber(medida.ADC2);
            
            LCD_Position(1,11);
            LCD_PrintString("F:");
            LCD_PrintNumber(medida.frecuencia/10);
            LCD_PutChar('.');
            LCD_PrintNumber(medida.frecuencia%10);
            
            LCD_Position(2,11);
            LCD_PrintString("D:");
            if (medida.dureza < 10) LCD_PutChar('0');
            LCD_PrintNumber(medida.dureza);
            LCD_PutChar('%');
                                                                                        // Lee el divisor actual del reloj y lo imprime
            LCD_Position(0,0);LCD_PrintString("CLK DIV:");LCD_PrintNumber(Clock_2_GetDividerRegister()); 
            
            if (tecla == '1' or tecla == '2'){                                          // Imprime el Periodo del contador y el CMP VALUE actual
                LCD_Position(3,0);LCD_PrintString("Tct:");LCD_PrintNumber(PWM_ReadPeriod());            
                LCD_Position(3,11);LCD_PrintString("CMP:");LCD_PrintNumber(PWM_ReadCompare2());          
            }
            
            else if (tecla == '3'){                                                     // Imprime el Periodo del contador y el CMP VALUE actual
                LCD_Position(3,0);LCD_PrintString("Tct:");LCD_PrintNumber(PWM_CA_ReadPeriod());           
                LCD_Position(3,11);LCD_PrintString("CMP:");LCD_PrintNumber(PWM_CA_ReadCompare());         
            }
            break;
        case 2:                                                                         // Menu = 2: menu para la opcion 4
            LCD_ClearDisplay();
            LCD_Position(0,0);
            LCD_PrintString("D.PWM1:");
            LCD_Position(0,9);
            LCD_PutChar('%');
            
            LCD_Position(0,11);
            LCD_PrintString("ADC1:");
            LCD_PrintNumber(medida.ADC1);
            
            LCD_Position(1,0);
            LCD_PrintString("D.PWM2:");
            LCD_Position(1,9);
            LCD_PutChar('%');
            
            LCD_Position(1,11);
            LCD_PrintString("ADC2:");
            LCD_PrintNumber(medida.ADC2);
            
            LCD_Position(2,2);
            LCD_PrintString("D.PWM output:");
                                                                                        // Imprime el Periodo del contador y el CMP VALUE actual
            LCD_Position(3,0);LCD_PrintString("CP1:");LCD_PrintNumber(PWM_ReadCompare1());      
            LCD_Position(3,11);LCD_PrintString("CP2:");LCD_PrintNumber(PWM_ReadCompare2());        
            break;
    }
    
}

CY_ISR(contador){                                                                       // El tiempo permite controlar frecuencias de      
    tiempo.ms++;                                                                        // sensado , control de PWMs e impresión.
    if (tiempo.ms == 1000) tiempo.ms = 0;
}

CY_ISR(leerTecla){                                                                      // Interrupcion del teclado matricial
    variable.teclaPresionada = 1;
}
/* [] END OF FILE */
