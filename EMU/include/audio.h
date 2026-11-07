#ifndef _AUDIO_H_
#define _AUDIO_H_

int audio_init(void);
void audio_unpause(void);
void audio_pause(void);
void audio_set_gain(float gain);
int audio_cleanup(void);


#endif //_AUDIO_H_