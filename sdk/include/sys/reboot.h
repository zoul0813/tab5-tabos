#ifndef TABOS_SYS_REBOOT_H
#define TABOS_SYS_REBOOT_H

#define RB_AUTOBOOT  0x01234567
#define RB_POWER_OFF 0x4321fedc

int reboot(int command);

#endif
