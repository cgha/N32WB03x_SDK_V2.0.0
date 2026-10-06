#ifndef AMS_COMMON_H_
#define AMS_COMMON_H_
#include "prf_types.h"

#define AMS_UUID_LEN 16

#define AMS_ERR_INVALID_STATE       0xA0
#define AMS_ERR_INVALID_COMMAND    0xA1
#define AMS_ERR_ABSENT_ATTRIBUTE   0xA2

/* Remote Command IDs */
enum {
    AMS_CMD_PLAY = 0,
    AMS_CMD_PAUSE = 1,
    AMS_CMD_TOGGLE_PLAY_PAUSE = 2,
    AMS_CMD_NEXT_TRACK = 3,
    AMS_CMD_PREVIOUS_TRACK = 4,
    AMS_CMD_VOLUME_UP = 5,
    AMS_CMD_VOLUME_DOWN = 6,
    AMS_CMD_ADVANCE_REPEAT = 7,
    AMS_CMD_ADVANCE_SHUFFLE = 8,
    AMS_CMD_SKIP_FORWARD = 9,
    AMS_CMD_SKIP_BACKWARD = 10,
    AMS_CMD_LIKE_TRACK = 11,
    AMS_CMD_DISLIKE_TRACK = 12,
    AMS_CMD_BOOKMARK_TRACK = 13,
};

/* Entity IDs */
enum {
    AMS_ENTITY_PLAYER = 0,
    AMS_ENTITY_QUEUE = 1,
    AMS_ENTITY_TRACK = 2,
};

/* Player attributes */
enum {
    AMS_PLAYER_NAME = 0,
    AMS_PLAYER_PLAYBACK_INFO = 1,
    AMS_PLAYER_VOLUME = 2,
};

/* Queue attributes */
enum {
    AMS_QUEUE_INDEX = 0,
    AMS_QUEUE_COUNT = 1,
    AMS_QUEUE_SHUFFLE_MODE = 2,
    AMS_QUEUE_REPEAT_MODE = 3,
};

/* Track attributes */
enum {
    AMS_TRACK_ARTIST = 0,
    AMS_TRACK_ALBUM = 1,
    AMS_TRACK_TITLE = 2,
    AMS_TRACK_DURATION = 3,
};

#define AMS_UPDATE_FLAG_TRUNCATED  (1 << 0)

#endif
