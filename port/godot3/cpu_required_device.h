// SPDX-License-Identifier: MIT
#ifndef CPU_REQUIRED_DEVICE_H
#define CPU_REQUIRED_DEVICE_H
#include <cstdio>
#include <cstdlib>
// Called only for explicitly requested device mode; no-device memory mode stays optional.
static inline void cpu_require_device(bool okay,const char *message) {
 if(!okay){fprintf(stderr,"CPU_REQUIRED_DEVICE_FAILED %s\n",message);fflush(stderr);std::exit(70);}
}
#endif
