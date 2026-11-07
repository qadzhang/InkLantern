#ifndef STUB_BEEP_H
#define STUB_BEEP_H
static inline void beep(void) {}
static inline void beep_custom(int f, int t) { (void)f; (void)t; }
#endif
