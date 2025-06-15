#ifndef RL_CMD_H
#define RL_CMD_H

#include "public.h"

#ifdef __cplusplus
extern "C"
{
#endif

int rl_system_100ms_ex(const char *cmdStr, int timeout_count, int try_count);

#ifdef __cplusplus
}
#endif

#endif
