#ifndef KEYAPI_H
#define KEYAPI_H

#ifdef __cplusplus
extern "C" {
#endif


#define MASK_JOY2_UP    0x0001
#define MASK_JOY2_DOWN  0x0002
#define MASK_JOY2_BTN   0x0004

extern void emu_InitJoysticks(void);
extern unsigned short emu_DebounceLocalKeys(void);
extern int emu_ReadKeys(void);
extern int emu_GetPad(void);

#ifdef __cplusplus
}
#endif

#endif
