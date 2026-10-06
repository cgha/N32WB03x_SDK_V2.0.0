AMS Remote Command Test
=======================

The AMS test is enabled by default in src/app_profile/app_ams.c:

    #define AMS_TEST_AUTO_CMD_ENABLE       1
    #define AMS_TEST_CMD_INTERVAL          1000

After the first AMS Entity Update notification is received, the firmware starts a 1-second timer and sends these commands in order:

    4  PREVIOUS_TRACK
    3  NEXT_TRACK
    5  VOLUME_UP
    6  VOLUME_DOWN
    2  TOGGLE_PLAY_PAUSE

Then the sequence repeats.

Expected log example:

    [AMS TEST] start auto command test, conidx=0
    [AMS TEST] send cmd=4 PREVIOUS_TRACK, conidx=0
    [AMS] write cmp op=6 gatt_op=12 status=0x00 state=3
    [AMS TEST] send cmd=3 NEXT_TRACK, conidx=0
    ...

The timer is stopped automatically on BLE disconnect.

For manual key/button testing, call:

    app_ams_remote_command(conidx, AMS_CMD_PREVIOUS_TRACK);
    app_ams_remote_command(conidx, AMS_CMD_NEXT_TRACK);
    app_ams_remote_command(conidx, AMS_CMD_VOLUME_UP);
    app_ams_remote_command(conidx, AMS_CMD_VOLUME_DOWN);
    app_ams_remote_command(conidx, AMS_CMD_TOGGLE_PLAY_PAUSE);

To disable automatic testing, change AMS_TEST_AUTO_CMD_ENABLE to 0.
