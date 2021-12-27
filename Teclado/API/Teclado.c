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

/* [] END OF FILE */
#include "project.h"

uint8 `$INSTANCE_NAME`_teclaPresionada(void){
    uint8 `$INSTANCE_NAME`_Teclas[4][4] = {                                     // Se genera la disposición
        {'1','2','3','A'},                                                      // física del teclado
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
    };
    
    uint8 Tecla;
    uint8 fila,columna;
    
    fila = (`$INSTANCE_NAME`_Status_Register_Read() & 0b11);                    // Se lee el estado del registro
                                                                                // Int col col fila fila y se aplica una máscara
    `$INSTANCE_NAME`_Columna_Write(0b1111);                                     //   0   0   0   1    1 (operación and bit a bit)
    `$INSTANCE_NAME`_Fila_Write(0b0000);                                        // = 0   0   0   F    F
    CyDelayUs(10);                                                              // Luego de registrar la fila en estado 0, cambio
                                                                                // filas -> 0 y columnas -> 1
    
    columna = ((`$INSTANCE_NAME`_Status_Register_Read() & 0b1100) >> 2);        // Para las columnas se aplica igualmente, pero
    `$INSTANCE_NAME`_Fila_Write(0b1111);                                        // se obtiene: 
    `$INSTANCE_NAME`_Columna_Write(0b0000);                                     // = 0   C   C   0   0, y para que quede un digito
    CyDelayUs(10);                                                             // del 0 al 3 (columna 1 a 4) desplazo los bits (->)
                                                                                // NOTA: Los delays evita rebotes
    Tecla = `$INSTANCE_NAME`_Teclas[fila][columna];
    return Tecla;                                                               // Finalmente se retorna el caracter correspondiente
}
        
void `$INSTANCE_NAME`_Start(void){
    `$INSTANCE_NAME`_Fila_Write(0b1111);
    `$INSTANCE_NAME`_Columna_Write(0b0000);
    }
        
        
        
        
        