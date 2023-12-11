// Thanks to the anonymous donor of this code on:
// https://forums.raspberrypi.com/viewtopic.php?t=310320

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "notes.h"

typedef struct{
  unsigned short frequency;
  short  duration;
} note_struct;

note_struct HappyBirday[]={
    { NOTE_C4,4 },
    { NOTE_C4,8 },
    { NOTE_D4,-4},
    { NOTE_C4,-4},
    { NOTE_F4,-4},
    { NOTE_E4,-2},
    { NOTE_C4,4},
    {NOTE_C4,8},
    {NOTE_D4,-4},
    {NOTE_C4,-4},
    {NOTE_G4,-4},
    {NOTE_F4,-2},
    {NOTE_C4,4},
    {NOTE_C4,8},
    {NOTE_C5,-4},
    {NOTE_A4,-4},
    {NOTE_F4,-4},
    {NOTE_E4,-4},
    {NOTE_D4,-4},
    {NOTE_AS4,4},
    {NOTE_AS4,8},
    {NOTE_A4,-4},
    {NOTE_F4,-4},
    {NOTE_G4,-4},
    {NOTE_F4,-2},
    {REST,0}};

note_struct HarryPotter[]={
 // Hedwig's theme fromn the Harry Potter Movies
  // Socre from https://musescore.com/user/3811306/scores/4906610

  { REST, 2}, { NOTE_D4, 4},{ NOTE_G4, -4}, { NOTE_AS4, 8}, { NOTE_A4, 4},
  { NOTE_G4, 2}, { NOTE_D5, 4},{ NOTE_C5, -2}, { NOTE_A4, -2},
  { NOTE_G4, -4}, { NOTE_AS4, 8}, { NOTE_A4, 4},
  { NOTE_F4, 2}, { NOTE_GS4, 4},
  { NOTE_D4, -1}, { NOTE_D4, 4},

  { NOTE_G4, -4}, { NOTE_AS4, 8}, { NOTE_A4, 4},
  { NOTE_G4, 2}, { NOTE_D5, 4},
  { NOTE_F5, 2}, { NOTE_E5, 4},
  { NOTE_DS5, 2}, { NOTE_B4, 4},
  { NOTE_DS5, -4}, { NOTE_D5, 8}, { NOTE_CS5, 4},
  { NOTE_CS4, 2}, { NOTE_B4, 4},
  { NOTE_G4, -1},
  { NOTE_AS4, 4},

  { NOTE_D5, 2}, { NOTE_AS4, 4},
  { NOTE_D5, 2}, { NOTE_AS4, 4},
  { NOTE_DS5, 2}, { NOTE_D5, 4},
  { NOTE_CS5, 2}, { NOTE_A4, 4},
  { NOTE_AS4, -4}, { NOTE_D5, 8}, { NOTE_CS5, 4},
  { NOTE_CS4, 2}, { NOTE_D4, 4},
  { NOTE_D5, -1},
  {REST,4}, { NOTE_AS4,4},

  { NOTE_D5, 2}, { NOTE_AS4, 4},
  { NOTE_D5, 2}, { NOTE_AS4, 4},
  { NOTE_F5, 2}, { NOTE_E5, 4},
  { NOTE_DS5, 2}, { NOTE_B4, 4},
  { NOTE_DS5, -4}, { NOTE_D5, 8}, { NOTE_CS5, 4},
  { NOTE_CS4, 2}, { NOTE_AS4, 4},
  { NOTE_G4, -1}};

// timer declaration  First timer for note, second for spacing
typedef struct{
  uint slice_num;
  note_struct *pt;
  uint delayOFF;
  uint wholenote;
  uint tempo;
  volatile uint Done;
} note_timer_struct;

int64_t timer_note_callback(alarm_id_t id, void *user_data);

static inline void pwm_calcDivTop(pwm_config *c,int frequency,int sysClock)
{
  uint  count = sysClock * 16 / frequency;
  uint  div =  count / 60000;  // to be lower than 65535*15/16 (rounding error)
  if(div < 16)
      div=16;
  c->div = div;
  c->top = count / div;
}

uint playTone(note_timer_struct *ntTimer)
{
  int duration;
  pwm_config cfg = pwm_get_default_config();

  note_struct * note = ntTimer->pt;
  duration = note->duration;
  if(duration == 0) return 0;
  if(duration>0)
      duration = ntTimer->wholenote / duration;
  else
      duration = ( 3 * ntTimer->wholenote / (-duration))/2;

  if(note->frequency == 0)
      pwm_set_chan_level(ntTimer->slice_num,PWM_CHAN_A,0);
  else
  {
   pwm_calcDivTop(&cfg,note->frequency,125000000);
   pwm_init(ntTimer->slice_num,&cfg,true);
   pwm_set_chan_level(ntTimer->slice_num,PWM_CHAN_A,cfg.top / 2);
  }
  ntTimer->delayOFF = duration;
  return duration;
}

uint play_tone_1(uint slice_num, unsigned short frequency)
{
  pwm_config cfg = pwm_get_default_config();

  if(frequency == 0)
      pwm_set_chan_level(slice_num,PWM_CHAN_A,0);
  else
  {
   pwm_calcDivTop(&cfg,frequency,125000000);
   pwm_init(slice_num,&cfg,true);
   pwm_set_chan_level(slice_num,PWM_CHAN_A,cfg.top / 2);
  }
  return 1; 

}


int64_t timer_note_callback(alarm_id_t id, void *user_data)
{
  note_timer_struct *ntTimer = (note_timer_struct *) user_data;
  note_struct * note = ntTimer->pt;
  if(note->duration == 0)
     {
      ntTimer->Done=true;
      return 0;  // done!
     }
  // are we in pause time between  note
  if(ntTimer->delayOFF==0)
    {
       uint delayON = playTone(ntTimer);
       if(delayON == 0)
        {
           ntTimer->Done=true;
           return 0;
        }
       ntTimer->delayOFF = delayON;
       return 900*delayON;
    }
    else
    {
       pwm_set_chan_level(ntTimer->slice_num,PWM_CHAN_A,0);
       uint delayOFF = ntTimer->delayOFF;
       ntTimer->delayOFF=0;
       //  next note
       (ntTimer->pt)++;
       return 100*delayOFF;
    }
}




int play_melody(note_timer_struct *ntTimer, note_struct * melody, int tempo)
{

      ntTimer->Done = false;
      ntTimer->pt = melody;
      // set tempo
      ntTimer->tempo= tempo;
      ntTimer->wholenote = (60000 * 4) / tempo;
      ntTimer->delayOFF = 0;
      // start timer
      return add_alarm_in_us(1000,timer_note_callback,ntTimer,false);
}


