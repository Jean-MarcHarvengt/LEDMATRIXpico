
#include "pico.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>

#include "platform_config.h"
#include "iopins.h"
#include "keyapi.h"

static uint16_t bLastState;

/********************************
 * Input and keyboard
********************************/ 
static uint16_t readAnalogJoystick(void)
{
  uint16_t joysval = 0;
  // First joystick
#if INVY
#ifdef PIN_JOY2_1
  if ( !gpio_get(PIN_JOY2_1) ) joysval |= MASK_JOY2_DOWN;
#endif
#ifdef PIN_JOY2_2
  if ( !gpio_get(PIN_JOY2_2) ) joysval |= MASK_JOY2_UP;
#endif
#else
#ifdef PIN_JOY2_1
  if ( !gpio_get(PIN_JOY2_1) ) joysval |= MASK_JOY2_UP;
#endif
#ifdef PIN_JOY2_2
  if ( !gpio_get(PIN_JOY2_2) ) joysval |= MASK_JOY2_DOWN;
#endif
#endif


#ifdef PIN_JOY2_BTN
  joysval |= (gpio_get(PIN_JOY2_BTN) ? 0 : MASK_JOY2_BTN);
#endif

  return (joysval);     
}


int emu_GetPad(void) 
{
  return(bLastState);
}

int emu_ReadKeys(void) 
{
  uint16_t retval = readAnalogJoystick();

  return (retval);
}

unsigned short emu_DebounceLocalKeys(void)
{
  uint16_t bCurState = emu_ReadKeys();
  uint16_t bClick = bCurState & ~bLastState;
  bLastState = bCurState;

  return (bClick);
}


/********************************
 * Initialization
********************************/ 
void emu_InitJoysticks(void) { 
  // First Joystick   
#ifdef PIN_JOY2_1
  gpio_set_pulls(PIN_JOY2_1,true,false);
  gpio_set_dir(PIN_JOY2_1,GPIO_IN);
  gpio_set_input_enabled(PIN_JOY2_1, true); // Force ADC as digital input        
#endif  
#ifdef PIN_JOY2_2
  gpio_set_pulls(PIN_JOY2_2,true,false);
  gpio_set_dir(PIN_JOY2_2,GPIO_IN);  
  gpio_set_input_enabled(PIN_JOY2_2, true);  // Force ADC as digital input       
#endif  
#ifdef PIN_JOY2_BTN
  gpio_set_pulls(PIN_JOY2_BTN,true,false);
  gpio_set_dir(PIN_JOY2_BTN,GPIO_IN);  
#endif  
}

