

#ifndef DAEMOND_STATUS_H__
#define DAEMOND_STATUS_H__

extern void (*status_init)(void);
extern void (*status_update)(void);
extern void load_status_object(const char*);

#endif

