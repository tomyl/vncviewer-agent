
#ifndef __AGENT_H__
#define __AGENT_H__

#include <sys/select.h>
#include "DesktopWindow.h"

void agent_init(const char *path);
void agent_close();
void agent_add_fds(fd_set * rfds);
void agent_send(const char *fmt, ...);
void agent_process(fd_set * rfds, DesktopWindow *desktop);

#endif
