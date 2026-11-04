#ifndef ALERT_H_
#define ALERT_H_

// abuse the msg dialog to play system sounds.
#include <sysutil/msg.h>

typedef enum {
    SOUND_OK,
    SOUND_ERROR,
    SOUND_COUNT
} AlertSound;

static const msgType sound_types[SOUND_COUNT] = {
    [SOUND_OK]    = MSG_DIALOG_NORMAL,
    [SOUND_ERROR] = MSG_DIALOG_ERROR,
};

void play_alert(AlertSound sound)
{
    if ((unsigned)sound >= SOUND_COUNT) {
        return;
    }
    msgDialogOpen(sound_types[sound], "", NULL, NULL, NULL);
    msgDialogAbort();
}

void play_alert_n(AlertSound sound, int n, int uinterval)
{
    if(uinterval < 5000) 
        uinterval = 5000;

    for(int i = 0; i < n; i++)
    {
        play_alert(sound);
        
        if (i + 1 < n)
            usleep(uinterval);
    }
}
#endif //ALERT_H_