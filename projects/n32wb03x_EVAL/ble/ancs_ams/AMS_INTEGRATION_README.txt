N32WB03x ANCS + AMS integrated example

Based on the official ANCS example project.

Added:
- Apple Media Service Client (AMSC)
- AMS service discovery after pairing
- Entity Update CCCD subscription
- Remote Command CCCD subscription
- Player/Queue/Track attribute registration
- AMS Entity Update / Attribute Read / Remote Command parsing
- AMS remote command API
- AMS Service Solicitation UUID in advertising data (alongside ANCS)

Source:
  src/app_profile/amsc.c
  src/app_profile/amsc_task.c
  src/app_profile/app_ams.c
Headers:
  inc/app_profile/ams_common.h
  inc/app_profile/amsc.h
  inc/app_profile/amsc_task.h
  inc/app_profile/app_ams.h

Integration points:
  src/app_ble.c
  inc/app_user_config.h
  inc/app_profile/rwapp_config.h
  MDK-ARM/ancs.uvprojx
