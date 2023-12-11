#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
// pico-examples/i2c/ssd1306_i2c
#include "ssd1306_i2c.c"
#include "sd_card.h"
#include "ff.h"
#include "hardware/adc.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/spi.h"

//
// Macro 
// 

#define byte uint8_t

//
// TTS Note: the source of the TTS Interface to the Epson S1V30120 is
// currently omitted.  Hence, TTS is disabled for the public version
// of PicoRAM 2090.  Note that the firmware image (firmware/sram.uf2)
// contains TTS support though!
// 

//#define TTS 

#ifdef TTS
#include "tts.c"
#endif

#define TTS_BUFF_SIZE 80

static char current_tts[TTS_BUFF_SIZE];
static int cur_tts_index = 0;

#ifndef TTS
void tts_setup(void) {
}

void tts_stop(void) {
}

void speak(char message[]) {
}
#endif 

//
// Sound 
// Thanks to the anonymous donor of this code on:
// https://forums.raspberrypi.com/viewtopic.php?t=310320
// 

#include "tone.c"

static unsigned short NOTES_ARR[8][16] = {
  { REST, NOTE_C1, NOTE_CS1, NOTE_D1, NOTE_DS1, NOTE_E1, NOTE_F1, NOTE_FS1, NOTE_G1, NOTE_GS1, NOTE_A1, NOTE_AS1, NOTE_B1, NOTE_C2, REST, REST }, 
  { REST, NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2, NOTE_C3, REST, REST }, 
  { REST, NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3, NOTE_C4, REST, REST }, 
  { REST, NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4, NOTE_C5, REST, REST }, 
  { REST, NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5, NOTE_C6, REST, REST }, 
  { REST, NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6, NOTE_C7, REST, REST }, 
  { REST, NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7, NOTE_C8, REST, REST }, 
  { REST, NOTE_C8, NOTE_CS8, NOTE_D8, NOTE_DS8, NOTE_E1, NOTE_F1, NOTE_FS1, NOTE_G1, NOTE_GS1, NOTE_A1, NOTE_AS1, NOTE_B1, NOTE_C2, REST, REST } };

//
// Display 
// 

#define LINES 4
#define DISPLAY_DELAY 250 

#define BLINK_DELAY (100 * 1000) 
#define LONG_BUTTON_DELAY (400 * 1000) 

typedef char display_line[17];
display_line file;

const char* hexStringChar[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "C", "d", "E", "F" };

#define TEXT_BUFFER_SIZE 256
char text_buffer[TEXT_BUFFER_SIZE]; 

char mnemonic[TEXT_BUFFER_SIZE]; 
char line1[TEXT_BUFFER_SIZE]; 
char line2[TEXT_BUFFER_SIZE]; 
char line3[TEXT_BUFFER_SIZE]; 
char line4[TEXT_BUFFER_SIZE]; 

char *screen[LINES] = {line1, line2, line3, line4}; 

//
// Pico Pins 
//

#define PICO_DEFAULT_I2C_SDA_PIN 20
#define PICO_DEFAULT_I2C_SCL_PIN 21

#define ADR_INPUTS_START 2
#define DATA_GPIO_START (ADR_INPUTS_START + 10)
#define DATA_GPIO_END (DATA_GPIO_START + 4) 
#define DISP_INPUT 0
#define WE_INPUT 1
#define SOUND_OUTPUT 28
#define ADC_KEYS_INPUT 27

const uint LED_PIN = PICO_DEFAULT_LED_PIN;

uint32_t addr_mask = 0; 
uint32_t data_mask = 0; 
uint32_t gpio = 0; 

//
// Audio State 
// 

bool SOUND_OR_TTS = false; 
int slice_num = 0; 

//
// RTC 
//

static uint8_t rtc_seconds;
static uint8_t rtc_minutes;
static uint8_t rtc_hours;

static uint8_t rtc_hours_dec;
static uint8_t rtc_mins_dec; 
static uint8_t rtc_secs_dec;

#define I2C_ADDR 0x68
#define DS3231_I2C_TIMEOUT 5000
#define I2C_BUFFER_SIZE 0x12
#define DS3231_TIME_REGISTER_COUNT 0x12
#define DS3231_REGISTER_DAY 0x03

//
// SRAM Memory Banks 
//

static uint8_t cur_bank = 0; 

#define MAX_BANKS 0x10

uint8_t ram[MAX_BANKS][(uint32_t) 1 << 10] = {};

//
// RTC F06 Intercept 
// 

bool rtc_load_disarm = false;
bool rtc_load_active = false;
bool rtc_loaded = false;

#define RTC_LOAD_BANK (MAX_BANKS +1) 

//
// Register Operands for Extended Op-Codes 
//

static uint8_t cur_reg_load_bank = 0;
static uint8_t req_reg_load_bank = 0; 

bool reg_load_disarm = false;
bool reg_load_active = false;

uint32_t regload_return_adr = 0; 

// HIGHEST BANK IS RTC LOAD!, BELOW IS GOTO JUMP 01 BANK!
uint8_t reg_load_ram[MAX_BANKS + 2][(uint32_t) 1 << 10] = {};

// 0x00 - 0x2F 
// target addresses that jump to C30 
// 09 = 0, 08 = 1, 0D = 2, 0C = 3, 13 = 4, 12 = 5, 17 = 6, 16 = 7
// 1F = 8, 1E = 9, 23 = A, 22 = B, 29 = C, 28 = D, 2D = E, 2C = F

#define REG_LOAD "F01 F01 970 D1A 930 D10 910 D0C 900 E0B C30 C30 920 E0F C30 C30 950 D16 940 E15 C30 C30 960 E19 C30 C30 9B0 D26 990 D22 980 E21 C30 C30 9A0 E25 C30 C30 9D0 D2C 9C0 E2B C30 C30 9E0 E2F C30 C30 " // from 30...40 there will be NOPs, and the Cxx return computed

static byte reg_load_target_addresses[] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 0..9 
  1, 0, // A, B
  255, 255, // C, D
  3, 2, // E, F
  255, 255, 255, 255, // 10, 11, 12, 13
  5, 4, // 14, 15
  255, 255, // 16, 17
  7, 6, // 18, 19
  255, 255, 255, 255, 255, 255, // 1A, 1B, 1C, 1D, 1E, 1F 
  9, 8, // 20, 21 
  255, 255, // 22, 23
  0xB, 0xA, // 24, 25
  255, 255, 255, 255, // 26, 27, 28, 29 
  0xD, 0xC, // 2A, 2B
  255, 255, // 2C, 2D
  0xF, 0xE // 2E, 2F 
}; 
  
//
// "ROM" Banks / Pre-Loaded Programs 
//

#define MAX_PGMS 0x0E

static char *PGM[MAX_PGMS] = {

  // 0 = LOAD RTC CLOCK 
  "F6A F06 FF0 F08 C00 ", 

  // 1 = 17+4 
  "F08 FE0 14A 1DB 1DC 1AD 17E 11F F6A FFF F02 B6C C12 F40 B6C FFF F02 E2C 80D E15 C1E 9A0 E18 C1E 902 E1B C1E 1A2 1A3 C5A 0D0 402 D24 992 D24 C26 513 562 923 E29 C2C 912 E5A D60 917 D33 E30 C36 966 D33 C36 90F E53 C0D 84E E39 C42 9A4 E3C C42 903 E3F C42 1A6 1A7 C60 0E4 446 D48 996 D48 C4A 566 517 927 E4F 90F E53 C0D 916 E60 D5A C4C 837 E57 D60 C5A 826 E60 D60 1DC 10D 10E 16F F4C C64 1DD 1AE 1BF F3D 1FB FEB FFB 104 FE4 F62 FF0 C00 F05 4FE 9AD D71 C73 57D C6E 9AE D76 F07 57E C73 ", 
    
  // 2 = NIM GAME
  "F08 FE0 F41 FF2 FF1 FF4 045 046 516 FF4 854 D19 904 E19 B3F F03 0D1 0E2 911 E15 C1A 902 D1A 1F0 FE0 F00 F02 064 10C 714 B3F 11A 10B C24 46A FBB 8AD E27 C29 8BE E2F 51C E2C C22 914 E2F C1C F03 0D1 0E2 F41 902 D09 911 E38 C09 1E2 1E3 1F5 FE5 105 FE5 C3A 01D 02E F04 64D FCE F07 ",

  // 3 = three digit counter with carry
  "F30 510 FB1 FB2 FE1 FE1 C00 ",

  // 4 = electronic die
  "F05 90D E00 96D D00 F1D FF0 C00 ", 

  // 5 = tree digit counter with carry
  "F30 510 FB1 FB2 FE1 FE1 C00 ", 

  // 6 = scrolling LED light
  "110 F60 FE0 FA0 FB0 C02 ", 

  // 7 = DIN digital input test
  "F10 FD0 FE0 C00 ", 

  // 8 = Lunar Lander
  "F02 F08 FE0 142 1F3 114 125 136 187 178 1A1 02D 03E 04F F03 F5D FFB F02 1B1 10F 05D 06E F03 F5D FFB F02 1C1 07D 08E F03 F5D FFB 10D 10E F2D FFB 99B D29 0DE 0BD C22 F02 10F F04 6D7 FC8 D69 6E8 D69 75D FCE D5A 4D2 FB3 FB4 4E3 FB4 652 FC3 FC4 D7B 663 FC4 D7B 6D5 FC6 D80 6E6 D80 904 D0A 903 D0A 952 D0A 906 D0A 955 D0A 1E0 1E1 1E2 1E3 1E4 1E5 1F6 FE6 F60 FF0 C00 6DF 6F2 FC3 FC4 D7B 652 FC3 FC4 D7B 663 FC4 D7B 4F5 FB6 C45 1E0 1A1 1E2 1A3 1FF 1FE FEF 71E D73 C70 F40 FEF 10D FED 71E D58 F02 C73 1A0 1A1 1A2 1A3 C6D 1F0 1A1 1F2 1A3 C6D ", 

  // 9 = Primes
  "F08 FEF F50 FFE FE5 9BE D09 E0C C00 9CE D02 C16 FFF 9AF EDA D18 034 023 012 001 0F0 C0C F02 B77 F02 B90 F0D F08 168 F0D F0F B77 F0F 904 D2A 903 D33 902 D36 901 D3C C3F 92A D2D C41 909 D30 C41 958 D3F C41 90A D3F C41 929 D39 C41 918 D3F C41 909 D3F C41 1FF C01 F70 F71 F72 F73 F74 680 D49 C4C 760 711 D4F 691 D50 C53 691 761 712 D56 6A2 D57 C5D 6A2 762 713 D58 C5D 763 714 900 D65 010 021 032 043 104 C5D 904 D46 903 D46 82A D1D E6D C46 819 D1D E71 C46 808 D1D E75 C46 F0D C17 510 990 D7B C90 560 511 991 D80 C90 561 512 992 D85 C90 562 513 993 D8A C90 563 514 994 D8F C90 564 910 EA9 920 E9D 930 E9D 970 EA9 990 EA9 950 E9D C77 904 DA6 903 DA6 902 DA6 901 DA6 CCF 930 EA9 C77 F70 F71 F72 F73 F74 410 101 10F DD2 990 DD2 420 102 11F DD2 990 DD2 430 103 12F DD2 990 DD2 440 104 13F DD2 990 DD2 901 DAE 930 ED0 960 ED0 990 ED0 F0D F07 F0D C77 560 511 92F DC4 EBE 90F DB8 CB2 F08 C0C ",

  // A = TIC TAC TOE
  "F08 F0D F08 FE0 198 F28 FF9 51A 94A E21 09D 51D B2B 0C8 FF9 08D 54D B2B 89C E07 0C8 1CA 1CB F48 10F FEF 516 D1F 1FF FEF C18 FF0 C00 09D 55D 02B 0C8 1EA 1EB 1FF FEF F48 C1F 0DC 71D 10E F03 0D0 0E1 F0D 180 F0C F0D 101 180 F0B 60C F07 ", 

  // B = CAR RACING 
  "F08 112 124 F40 F05 8D9 E0D 9FD E0D 0D9 515 FB6 C10 71E 6ED C05 F06 0A7 0B8 967 D10 527 FE9 FFC 92C E2C 99C E25 95C E26 91C E27 9BC E29 97C E2A C2B F94 F94 F94 C2C FA4 FA4 FA4 100 101 102 103 914 E39 924 E3B 944 E3D 984 E3F C49 113 C40 112 C40 111 C40 110 F06 87A D49 88B E46 C49 098 248 E04 10D 10E 10F F02 51D D50 C4C F40 51E D54 C50 55F D57 C4C 10F 05D 06E F03 F2D FFF C00 ", 

  // C = BLOCKADE
  "F08 FE0 1A0 151 112 113 F2E FFF FFE B76 91F E0F 92F E11 C00 0E0 C12 0E1 B5B 664 675 845 E18 C1B B5B B60 C06 B5B 516 864 D2B B5B B72 905 E28 9F5 E28 9E5 E28 C2D B5B B6E C2D B5B B6E 904 E34 9F4 E34 9E4 E34 C36 B5B B72 91F D3B 042 02E C3D 053 03E 992 E40 C42 943 E43 C06 1FA FEA 51B D48 C43 FE9 1FB 1FC F3A FF0 C00 1FA FEA 10A FEA 51B D55 C4E 1CA 1CB 1CC F3A FF0 C00 004 015 026 037 F07 11F 516 864 E67 062 02E F07 726 062 02E 924 E6D F07 CAE 675 654 11F F07 664 645 12F F07 92F E80 9AE D00 8E2 D00 E00 80E E00 F07 95E D00 8E3 D00 E00 81E E00 F07 ", 

  // D = REGLOAD Test Program 
  "F10 FF0 970 D1A 930 D10 910 D0C 900 E0B F00 F00 920 E0F F00 F00 950 D16 940 E15 F00 F00 960 E19 F00 F00 9B0 D26 990 D22 980 E21 F00 F00 9A0 E25 F00 F00 9D0 D2C 9C0 E2B F00 F00 9E0 E2F F00 F00 ", 

};

//
// PC / Address Bus & Current Instruction Monitoring 
//

uint32_t m_adr = 0; 
uint32_t m_op1 = 0; 
uint32_t m_op2 = 0; 
uint32_t m_op3 = 0; 

uint32_t l_adr = 0; 
uint32_t l_op1 = 0; 
uint32_t l_op2 = 0; 
uint32_t l_op3 = 0; 

//
// Microtronic Op-Codes 
//

#define OP_MOV  0x0
#define OP_MOVI 0x1
#define OP_AND  0x2
#define OP_ANDI 0x3
#define OP_ADD  0x4
#define OP_ADDI 0x5
#define OP_SUB  0x6
#define OP_SUBI 0x7
#define OP_CMP  0x8 
#define OP_CMPI 0x9 // ZERO if EQUAL, CARRY if GREATER
#define OP_OR   0xA

#define OP_CALL 0xB
#define OP_GOTO 0xC
#define OP_BRC  0xD
#define OP_BRZ  0xE

#define OP_MAS  0xF7
#define OP_INV  0xF8
#define OP_SHR  0xF9
#define OP_SHL  0xFA
#define OP_ADC  0xFB
#define OP_SUBC 0xFC

#define OP_DIN  0xFD
#define OP_DOT  0xFE
#define OP_KIN  0xFF

#define OP_HALT   0xF00
#define OP_NOP    0xF01
#define OP_DISOUT 0xF02
#define OP_HXDZ   0xF03
#define OP_DZHX   0xF04
#define OP_RND    0xF05
#define OP_TIME   0xF06
#define OP_RET    0xF07
#define OP_CLEAR  0xF08
#define OP_STC    0xF09
#define OP_RSC    0xF0A
#define OP_MULT   0xF0B
#define OP_DIV    0xF0C
#define OP_EXRL   0xF0D
#define OP_EXRM   0xF0E
#define OP_EXRA   0xF0F

#define OP_DISP 0xF

//
// Utilities 
//

unsigned char decode_hex(char c) {

  if (c >= 65 && c <= 70)
    return c - 65 + 10;
  else if (c >= 48 && c <= 67)
    return c - 48;
  else
    return -1;
}

unsigned char reverse_bits(unsigned char b) {
  return (b & 0b00000001) << 3 |  (b & 0b00000010) << 1 | (b & 0b00000100) >> 1 |  (b & 0b00001000) >> 3 | 
    (b & 0b00010000) << 3 |  (b & 0b00100000) << 1 | (b & 0b01000000) >> 1 |  (b & 0b10000000) >> 3 ; 
}

//
// SRAM Bank Management 
// 

void clear_bank(uint8_t bank) {
  for (uint32_t adr = 0; adr < (1 << 10); adr++) {
    ram[bank][adr] = 0; 
  }
} 

void enter_program(uint8_t bank) {

  uint32_t adr = 0; 
  char* i = PGM[bank]; 

  memset(buf, 0, SSD1306_BUF_LEN);
  render(buf, &frame_area);
      
  while (*i) {
        
    char cm1 = *i++; 
    char cm2 = *i++; 
    char cm3 = *i++; 

    char m1 = decode_hex(cm1); 
    char m2 = decode_hex(cm2); 
    char m3 = decode_hex(cm3); 

    /* 
       WriteChar(buf, 0, 0, bank + 65);     
       WriteChar(buf, 16, 0, cm1); 
       WriteChar(buf, 24, 0, cm2); 
       WriteChar(buf, 32, 0, cm3); 
       render(buf, &frame_area); 
    */

    ram[bank][adr] = m1; 
    ram[bank][adr | 1 << 9] = m2; 
    ram[bank][adr | 1 << 8] = m3; 
    
    adr++;
    i++; // skip over space in PGM string

  }
  
}

void prepare_regload_banks() {

  uint32_t adr = 0;

  // C01 GOTO JMP BANK = 0x10 
  // cause 00 cannot be detected! 
  for (adr = 0; adr < 0x100; adr++) {
    reg_load_ram[0x10][adr] = 0xc; 
    reg_load_ram[0x10][adr | 1 << 9] = 0; 
    reg_load_ram[0x10][adr | 1 << 8] = 1; 
  }

  // F01 in RTC LOAD BANK = 0x11 
  // so Microtronic can execute NOPS with Pico prepares clock load program! 

  for (adr = 0; adr < 0x100; adr++) {
    reg_load_ram[RTC_LOAD_BANK][adr] = 0xf; 
    reg_load_ram[RTC_LOAD_BANK][adr | 1 << 9] = 0; 
    reg_load_ram[RTC_LOAD_BANK][adr | 1 << 8] = 1; 
  }

  //
  // 
  // 
  
  for (uint8_t reg_bank = 0; reg_bank < 0x10; reg_bank++) { 
        
    // NOPS F01 first
    for (adr = 0; adr < 0x100; adr++) {
      reg_load_ram[reg_bank][adr] = 0xf; 
      reg_load_ram[reg_bank][adr | 1 << 9] = 0x0; 
      reg_load_ram[reg_bank][adr | 1 << 8] = 0x1; 
    }

    // then the REGLOAD program

    char* i = REG_LOAD;     
    adr = 0;
       
    while (*i) {
        
      char cm1 = *i++; 
      char cm2 = *i++; 
      char cm3 = *i++; 

      char m1 = decode_hex(cm1); 
      char m2 = decode_hex(cm2); 
      char m3 = decode_hex(cm3);

      if (m1 == 9 && m3 == 0) // rewrite 9x0 comparisons -> 9x<reg> 
	m3 = reg_bank;
      
      reg_load_ram[reg_bank][adr] = m1; 
      reg_load_ram[reg_bank][adr | 1 << 9] = m2; 
      reg_load_ram[reg_bank][adr | 1 << 8] = m3; 
    
      adr++;
      i++; // skip over space in PGM string

    }
  }
  
}

//
// Display Management 
// Adapted from pico-examples/i2c/ssd1306_i2c
// 

void clear_screen() {
  memset(buf, 0, SSD1306_BUF_LEN);
  render(buf, &frame_area);
  memset(line1, 0, TEXT_BUFFER_SIZE);
  memset(line2, 0, TEXT_BUFFER_SIZE);
  memset(line3, 0, TEXT_BUFFER_SIZE);
  memset(line4, 0, TEXT_BUFFER_SIZE);
}

void clear_screen0() {
  memset(buf, 0, SSD1306_BUF_LEN);
  memset(line1, 0, TEXT_BUFFER_SIZE);
  memset(line2, 0, TEXT_BUFFER_SIZE);
  memset(line3, 0, TEXT_BUFFER_SIZE);
  memset(line4, 0, TEXT_BUFFER_SIZE);
}

void clear_line0(int line) {
  switch (line) {
  case 0 : memset(line1, 0, TEXT_BUFFER_SIZE); break; 
  case 1 : memset(line2, 0, TEXT_BUFFER_SIZE); break;
  case 2 : memset(line3, 0, TEXT_BUFFER_SIZE); break; 
  case 4 : memset(line4, 0, TEXT_BUFFER_SIZE); break;
  }
  strcpy(screen[line],  "                "); 
  WriteString(buf, 0, line*8, screen[line]);
}

void clear_line(int line) {
  clear_line0(line);
  render(buf, &frame_area); 
}

void print_string0(int x, int y, char* text, ...) {

  va_list args;
  va_start(args, text); 
  vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args); 
  strcpy(screen[y], text_buffer); 
  WriteString(buf, x*8, y*8, text_buffer);
  va_end(args);
}

void print_string(int x, int y, char* text, ...) {
  va_list args;
  va_start(args, text); 
  vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args); 
  strcpy(screen[y], text_buffer); 
  WriteString(buf, x*8, y*8, text_buffer);
  render(buf, &frame_area); 
}

void print_line(int x, char* text, ...) {    
  va_list args;
  va_start(args, text); 
  vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args); 
  char* screen0 = screen[0]; 
  for (int i = 0; i < 3; i++) {
    screen[i] = screen[i+1];
    WriteString(buf, x*8, i*8, screen[i]); 
  }
  screen[3] = screen0; 
  strcpy(screen[3], text_buffer);     
  WriteString(buf, x*8, 3*8, text_buffer);
  render(buf, &frame_area); 
  va_end(args);
}

void print_line0(int x, char* text, ...) {    
  va_list args;
  va_start(args, text); 
  vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args); 
  char* screen0 = screen[0]; 
  for (int i = 0; i < 3; i++) {
    screen[i] = screen[i+1];
    WriteString(buf, x*8, i*8, screen[i]); 
  }
  screen[3] = screen0; 
  strcpy(screen[3], text_buffer);     
  WriteString(buf, x*8, 3*8, text_buffer);
  va_end(args);
}

void print_char0(int x, int y, char c) {
  text_buffer[0] = c;
  text_buffer[1] = 0; 
  WriteString(buf, x*8, y*8, text_buffer);
}

void print_char(int x, int y, char c) {
  text_buffer[0] = c; 
  text_buffer[1] = 0; 
  WriteString(buf, x*8, y*8, text_buffer);
  render(buf, &frame_area); 
}

void disp_plot0(int x, int y) {
  SetPixel(buf, x, y, true);
}

void disp_plot(int x, int y) {
  SetPixel(buf, x, y, true);
  render(buf, &frame_area); 
}

void disp_line0(int x1, int y1, int x2, int y2 ) {
  DrawLine(buf, x1, y1, x2, y2, true);
}

void disp_line(int x1, int y1, int x2, int y2 ) {
  DrawLine(buf, x1, y1, x2, y2, true);
  render(buf, &frame_area); 
}

void render_display() {    
  render(buf, &frame_area); 
}

//
// Sound 
//

void set_pwm_pin(uint pin, uint freq, uint duty_c) { // duty_c between 0..10000
  gpio_set_function(pin, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(pin);
  pwm_config config = pwm_get_default_config();
  float div = (float)clock_get_hz(clk_sys) / (freq * 10000);
  pwm_config_set_clkdiv(&config, div);
  pwm_config_set_wrap(&config, 10000); 
  pwm_init(slice_num, &config, true); // start the pwm running according to the config
  pwm_set_gpio_level(pin, duty_c); //connect the pin to the pwm engine and set the on/off level. 
};

//
// Mnemonics Display 
//

uint8_t lcd_cursor = 0; 

void clear_mnemonics_buffer() {

  for (int i = 0; i < 14; i++)
    mnemonic[i] = ' ';

  mnemonic[14] = 0;

  lcd_cursor = 0;

}

void input_mnem(const char* string) {
  
  char c; 
  while ( c = *string++) 
    mnemonic[lcd_cursor++] = c;

}

void advance_cursor(bool b) {
  if (b) lcd_cursor ++;
}

void get_mnem(bool spaces) {

  clear_mnemonics_buffer();

  uint16_t hi = m_op2; 
  uint16_t lo = m_op3; 
  uint16_t op2 = m_op1 * 16 + m_op2;
  uint16_t op3 = m_op1 * 256 + m_op2 * 16 + m_op3;

  switch ( m_op1 ) {
  case OP_MOV  : input_mnem("MOV   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_MOVI : input_mnem("MOVI  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_AND  : input_mnem("AND   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_ANDI : input_mnem("ANDI  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_ADD  : input_mnem("ADD   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_ADDI : input_mnem("ADDI  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_SUB  : input_mnem("SUB   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_SUBI : input_mnem("SUBI  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_CMP  : input_mnem("CMP   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_CMPI : input_mnem("CMPI  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_OR   : input_mnem("OR    ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
  case OP_CALL : input_mnem("CALL  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); input_mnem( hexStringChar[lo] ); break;
  case OP_GOTO : input_mnem("GOTO  ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); input_mnem( hexStringChar[lo] ); break;
  case OP_BRC  : input_mnem("BRC   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); input_mnem( hexStringChar[lo] ); break;
  case OP_BRZ  : input_mnem("BRZ   ") ; advance_cursor(spaces); input_mnem( hexStringChar[hi] ); input_mnem( hexStringChar[lo] ); break;
  default : {
    switch (op2) {
    case OP_MAS  : input_mnem( "MAS   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_INV  : input_mnem( "INV   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_SHR  : input_mnem( "SHR   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_SHL  : input_mnem( "SHL   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_ADC  : input_mnem( "ADC   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_SUBC : input_mnem( "SUBC  ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_DIN  : input_mnem( "DIN   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_DOT  : input_mnem( "DOT   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    case OP_KIN  : input_mnem( "KIN   ");  advance_cursor(spaces); input_mnem( hexStringChar[lo] ); break;
    default : {
      switch (op3) {
      case OP_HALT   : input_mnem( "HALT  ");  break;
      case OP_NOP    : input_mnem( "NOP   ");  break;
      case OP_DISOUT : input_mnem( "DISOUT");  break;
      case OP_HXDZ   : input_mnem( "HXDZ  ");  break;
      case OP_DZHX   : input_mnem( "DZHX  ");  break;
      case OP_RND    : input_mnem( "RND   ");  break;
      case OP_TIME   : input_mnem( "TIME  ");  break;
      case OP_RET    : input_mnem( "RET   ");  break;
      case OP_CLEAR  : input_mnem( "CLEAR ");  break;
      case OP_STC    : input_mnem( "STC   ");  break; 
      case OP_RSC    : input_mnem( "RSC   ");  break;
      case OP_MULT   : input_mnem( "MULT  ");  break;
      case OP_DIV    : input_mnem( "DIV   ");  break;
      case OP_EXRL   : input_mnem( "EXRL  ");  break;
      case OP_EXRM   : input_mnem( "EXRM  ");  break;
      case OP_EXRA   : input_mnem( "EXRA  ");  break;
      default        : input_mnem( "DISP  ");  advance_cursor(spaces); input_mnem( hexStringChar[hi] ); advance_cursor(spaces); input_mnem( hexStringChar[lo]); break;
      }
    }
    }
  }
  }

}

//
//
//

typedef enum { NONE, UP, DOWN, BACK, OK, CANCEL } button_state; 

button_state read_button_state(void) { 

  adc_select_input(1); 
  uint16_t adc = adc_read();

  if (adc <= 0x100) {
    return UP; 
  } else if (adc <= 0x300) {
    return DOWN; 
  } else if (adc <= 0x700) { 
    return BACK; 
  } else if (adc <= 0x900) {
    return OK; 
  } else if (adc <= 0xC00) {
    return CANCEL; 
  } else {
    return NONE; 
  }
}

//
// UI Button Management 
// 

bool wait_for_button_release(void) {
  uint64_t last = time_us_64(); 
  while (read_button_state() != NONE) {}
  return (time_us_64() - last > LONG_BUTTON_DELAY); 
}

void wait_for_button(void) {
  while (read_button_state() == NONE) {}
  wait_for_button_release(); 
  return;
}

bool wait_for_yes_no_button(void) {
  button_state button; 
  while (1) {
    button = read_button_state();
    if (button == OK) {
      wait_for_button_release(); 
      return true;
    } else if (button == CANCEL) {
      wait_for_button_release(); 
      return false;
    }
  }
}

//
// Test Tone 
//

void play_test_sound(void) {
  play_tone_1(slice_num, NOTES_ARR[1][1]); 
  sleep_ms(100);
  play_tone_1(slice_num, NOTES_ARR[1][8]); 
  sleep_ms(100);
  play_tone_1(slice_num, NOTES_ARR[1][5]); 
  sleep_ms(200);
  play_tone_1(slice_num, 0);
}


//
// Display Modes 
//

typedef enum { OFF, OP, OP_AND_MNEM } disp_mode; 

//
// Extended Op-Codes 
//

/* 

   0xx ENTER LITERAL DATA x

   3Fx ENTER DATA FROM REG x 

   500 HEX DATA ENTRY MODE
   501 DEC DATA ENTRY MODE
   502 DISP CLEAR SCREEN 
   503 DISP TOGGLE UPDATE  
   504 DISP REFRESH
   505 DISP CLEAR LINE <X> 
   506 DISP SHOW CHAR <LOW><HIGH>
   507 DISP CURSOR SET CURSOR LINE <X>
   508 DISP SET CURSOR <X> <Y>
   509 DISP PLOT <X> <Y> 
   50A DISP LINE <X1> <Y1> <X2> <Y2>
   50B DISP LINE FROM <X> <Y> 
   50C DISP LINE TO   <X> <Y> 
   50D SOUND PLAY NOTE <X><Y> (SOUND OFF FIRST) 
   50E SPEAK DISP ECHO
   50F SPEAK BYTE <LOW><HIGH>

   70x SWITCH MEMORY BANK x

*/

typedef enum { ARG1_LOW, ARG1_HIGH, ARG2_LOW, ARG2_HIGH, ARG3_LOW, ARG3_HIGH, ARG4_LOW, ARG4_HIGH } arg_mode; 

typedef enum { DISP_AUTO_REFRESH, DISP_MANUAL_REFRESH } disp_refresh_mode; 

typedef enum { ARGS_HEX, ARGS_DEC } arg_mode_hex_or_dec; 

typedef enum { NOP, DISP_CLEAR_LINE, DISP_SHOW_CHAR, DISP_SET_CURSOR_AT_LINE, DISP_SET_CURSOR_XY, DISP_PLOT,
  DISP_LINE, DISP_LINE_TO, DISP_LINE_FROM, SOUND_PLAY_NOTE, SPEAK_BYTE } ext_op_code_mode; 


//
// SD Card Test 
//

void sd_test(void) {

  FRESULT fr;
  FATFS fs;
  FIL fil;
  int ret;
  char buf[100];
  char filename[] = "test02.txt";

  /* clear_screen(); 
     print_string(0,0,"Test %d %d %d", 1, 2, 3);  */ 
    
  // Initialize SD card
  if (!sd_init_driver()) {
    print_string(0,0,"ERROR 1");
    while (true);
  }

  // Mount drive
  fr = f_mount(&fs, "0:", 1);
  if (fr != FR_OK) {
    print_string(0,0,"ERROR 2");
    while (true);
  }

  // Open file for writing ()
  fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
  if (fr != FR_OK) {
    print_string(0,0,"ERROR 3");
    while (true);
  }

  // Write something to file
  ret = f_printf(&fil, "SD Card\r\n");
  if (ret < 0) {
    print_string(0,0,"ERROR 4");
    f_close(&fil);
    while (true);
  }
  ret = f_printf(&fil, "works without\r\n");
  if (ret < 0) {
    print_string(0,0,"ERROR 5");
    f_close(&fil);
    while (true);
  }

  ret = f_printf(&fil, "any problems!\r\n");
  if (ret < 0) {
    print_string(0,0,"ERROR 5");
    f_close(&fil);
    while (true);
  }

  ret = f_printf(&fil, "Cool!\r\n");
  if (ret < 0) {
    print_string(0,0,"ERROR 5");
    f_close(&fil);
    while (true);
  }

  // Close file
  fr = f_close(&fil);
  if (fr != FR_OK) {
    print_string(0,0,"ERROR 6");
    while (true);
  }

  // Open file for reading
  fr = f_open(&fil, filename, FA_READ);
  if (fr != FR_OK) {
    print_string(0,0,"ERROR 7");
    while (true);
  }

  // Print every line in file over serial
  clear_screen(); 
  
  while (f_gets(buf, sizeof(buf), &fil)) {
    // print_string(0, i++ , buf);
  }

  // Close file
  fr = f_close(&fil);
  if (fr != FR_OK) {
    print_string(0,0,"ERROR 8");
    while (true);
  }

  // Unmount drive
  f_unmount("0:");

}

//
// SD Card Interface 
//

static FRESULT fr;
static FATFS fs;
static cwdbuf[FF_LFN_BUF] = {0};

void show_error_and_halt(char* err) {
  clear_screen(); 
  print_string(0,0,err);
  while (true); 
}

void show_error(char* err) {
  clear_screen(); 
  print_string(0,0,err);
  sleep_ms(1000);
  return; 
}

void show_error_wait_for_button(char* err) {
  show_error(err);
  wait_for_button_release(); 
  return; 
}

char* init_and_mount_sd_card(void) {

  memset(&cwdbuf, 0, FF_LFN_BUF);

  if (!sd_init_driver())
    show_error_and_halt("SDCARD ERR 1");

  // Mount drive
  fr = f_mount(&fs, "0:", 1);
  if (fr != FR_OK) 
    show_error_and_halt("SDCARD ERR 2");

  fr = f_getcwd(cwdbuf, sizeof cwdbuf);
  if (FR_OK != fr)
    show_error_and_halt("SDCARD ERR 3");
    
  return &cwdbuf;

}

void ls() {

  char const *p_dir;
   
  p_dir = init_and_mount_sd_card(); 
    
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
    
  fr = f_findfirst(&dj, &fno, p_dir, "*.MIC");
  if (FR_OK != fr) {
    show_error("SDCARD ERR 4"); 
    return;
  }

  while (fr == FR_OK && fno.fname[0]) { 
    if (fno.fattrib & AM_DIR) {
      // directory 
    } else {
      print_line(0,"%16s", fno.fname, fno.fsize);
      sleep_ms(100);
    }

    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
    
  f_closedir(&dj);
    
}


int count_files() {

  int count = 0;

  char const *p_dir;
   
  p_dir = init_and_mount_sd_card(); 
    
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
    
  fr = f_findfirst(&dj, &fno, p_dir, "*.MIC");
  if (FR_OK != fr) {
    show_error("SDCARD ERR 4"); 
    return 0;
  }

  while (fr == FR_OK && fno.fname[0]) { 
    if (fno.fattrib & AM_DIR) {
      // directory 
    } else {
      count++; 
    }

    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
    
  f_closedir(&dj);

  return count; 

}

void clear_file_buffer() {

  for (int i = 0; i < 16; i++)
    file[i] = ' ';

  file[16] = 0;

}

int select_file_no(int no) {
  int count = 0;

  clear_file_buffer();

  char const *p_dir;
   
  p_dir = init_and_mount_sd_card(); 
    
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
    
  fr = f_findfirst(&dj, &fno, p_dir, "*.MIC");
  if (FR_OK != fr) {
    show_error("SDCARD ERR 4"); 
    return 0;
  }

  while (fr == FR_OK && fno.fname[0]) { 
    if (fno.fattrib & AM_DIR) {
      // directory 
    } else {
      count++;
      if (count == no) {
	// copy name into file buffer for display 
	strcpy(file, fno.fname);
	return count; 
      }
    }

    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
    
  f_closedir(&dj);

  return 0;

}

int select_file() {

  clear_screen(); 
  int no = 1;
  int count = count_files();
  select_file_no(no);

  uint64_t last = time_us_64(); 
   
  bool blink = false;

  wait_for_button_release();

  while ( read_button_state() !=  OK ) {

    print_string(0,0,"Load %2d of %2d", no, count);
    print_string(0,3,"CANCEL OR OK");

    if ( time_us_64() - last > BLINK_DELAY) {

      last = time_us_64();
      blink = !blink;

      if (blink)
	print_string(0,1,"                ");
      else
	print_string(0,1,file);
    }

    switch ( read_button_state() ) {
    case UP :
      wait_for_button_release();
      if (no < count)
	no = select_file_no(no + 1);
      else
	no = select_file_no(1);
      break;
    case DOWN :
      wait_for_button_release();
      if (no > 1)
	no = select_file_no(no - 1);
      else
	no = select_file_no(count);
      break;
    case CANCEL :
      wait_for_button_release();
      return -1;
    default : break;
    }
  }

  wait_for_button_release();

  return 0;

}

int create_name() {

  clear_screen(); 

  wait_for_button_release();

  clear_file_buffer();
  
  file[0] = 'P';
  file[1] = 0;

  int cursor = 0;

  uint64_t last = time_us_64(); 

  bool blink = false;

  read_button_state();

  while ( read_button_state() != OK ) {

    print_string(0,0,"Save MIC - Name:");
    print_string(0,3,"CANCEL OR OK");

    if ( time_us_64() - last > BLINK_DELAY ) {

      last = time_us_64();      
      print_string0(0,1,file);      
      blink = !blink;

      if (blink)
	print_char(cursor,1,'_');
      else
	print_char(cursor,1,file[cursor]);
    }

    switch ( read_button_state() ) {
    case UP : if (file[cursor] < 127)  { file[cursor]++; wait_for_button_release(); } break;
    case DOWN : if (file[cursor] > 32) { file[cursor]--; wait_for_button_release(); } break;
    case BACK : 
      if ( ! wait_for_button_release() ) { // short press?
	if (cursor < 7) {
	  cursor++;
	  if (! file[cursor] ) 
	    file[cursor] = file[cursor-1];
	}
      } else {
	if (cursor > 0) {
	  cursor--;
	}
      }
      break; 
    case CANCEL : wait_for_button_release(); return -1;
    default : break;
    }

  }

  cursor++;
  file[cursor++] = '.';
  file[cursor++] = 'M';
  file[cursor++] = 'I';
  file[cursor++] = 'C';
  file[cursor++] = 0;

  clear_screen(); 

  wait_for_button_release();
  
  return 0;

}


//
// PGM 1 - Load from SD Card
//

void pgm1() {

  int aborted = select_file();
  clear_screen(); 

  if ( aborted == -1 ) {
    print_string(0,3,"CANCELED!       ");
    sleep_ms(DISPLAY_DELAY);
    sleep_ms(DISPLAY_DELAY);
    return;
  }

  print_string(0,0,"Loading");
  print_string(0,1,file);

  FRESULT fr;
  FIL fil;
  char buf[40];

  fr = f_open(&fil, file, FA_READ);
  if (fr != FR_OK) {
    show_error("SDCARD ERR 5");
    return; 
  }

  bool readingComment = false;
  bool readingOrigin = false;

  //
  //
  //

  int pc = 0;
  int count = 0;
  int i = 0;

  byte op = 0;
  byte arg1 = 0;
  byte arg2 = 0;

  //
  //
  //

  while (1) {


    //memset(&buf, 0, sizeof(buf));
    if (! f_gets(buf, sizeof(buf), &fil)) 
      break;

    i = 0;
    
    print_string0(0,2, "%s                ", buf); 
  
    while (1) {

      int b = buf[i++]; 
      if (!b)
	break;

      if (b == '\n' || b == '\r') {
	readingComment = false;
	readingOrigin = false;
      } else if (b == '#') {
	readingComment = true;
      } else if (b == '@') {
	readingOrigin = true;
      }

      if (!readingComment && !readingOrigin &&
	  b != '\r' && b != '\n' && b != '\t' && b != ' ' && b != '@' ) { // skip whitespace

	switch ( b ) {
	case 'I' : b = '1'; break; // correct for some common OCR errors
	case 'l' : b = '1'; break;
	case 'P' : b = 'D'; break;
	case 'Q' : b = '0'; break;
	case 'O' : b = '0'; break;
	}

	int decoded = decode_hex(b);

	if ( decoded == -1) {
	  show_error_wait_for_button("FILE ERROR!");
	  clear_screen(); 
	  fr = f_close(&fil);
	  return; 
	}
	
	switch ( count ) {
	case 0 : op  = decoded; count = 1;  break;
	case 1 : arg1 = decoded; count = 2; break;
	case 2 :

	  arg2 = decoded;
	  count = 0;

	  print_string(0,3, "SD -> %1x:%02x %1x%1x%1x", cur_bank, pc, op, arg1, arg2);

	  ram[cur_bank][pc] = op; 
	  ram[cur_bank][pc | 1 << 9] = arg1; 
	  ram[cur_bank][pc | 1 << 8] = arg2;  
	  pc++;

	}
      }
    }
  }

  //
  //
  //
  
  fr = f_close(&fil);
  if (fr != FR_OK) {
    show_error("Cant't close file"); 
  }

  // Unmount drive
  f_unmount("0:");

  //
  //
  //
  
  clear_screen();
  print_string(0,0,"Loaded: RESET!");
  print_string(0,1,file);
  sleep_ms(DISPLAY_DELAY);

  //
  //
  //
  
  return;

}

//
// PGM 2 - Save to SD Card
//

void pgm2() {

  clear_screen(); 
  int aborted = create_name();

  if ( aborted == -1 ) {
    print_string(0,3,"CANCELED!       ");
    sleep_ms(DISPLAY_DELAY);
    sleep_ms(DISPLAY_DELAY);
    return;
  }

  print_string(0,0,"Saving");
  print_string(0,1,file);

  FRESULT fr;
  FIL fil;
  int ret;

  fr = f_open(&fil, file, FA_READ);
  if (FR_OK == fr) {
    print_string(0,2,"Overwrite File?");
    if (! wait_for_yes_no_button()) {
      print_string(0,3,"*** CANCELED ***");
      sleep_ms(DISPLAY_DELAY);
      sleep_ms(DISPLAY_DELAY);
      f_close(&fil);
      return;
    } else
      print_string(0,2,"               ");
    f_close(&fil);
  } 

  //
  //
  //

  // Open file for writing ()
  fr = f_open(&fil, file, FA_WRITE | FA_CREATE_ALWAYS);
  if (fr != FR_OK) {
    show_error("WRITE ERROR 1");
    f_close(&fil);
    return; 
  }

  //
  //
  //

  byte op = 0;
  byte arg1 = 0;
  byte arg2 = 0;

  for (int pc = 0; pc <= 0xff ; pc++) {

    op = ram[cur_bank][pc]; 
    arg1 = ram[cur_bank][pc | 1 << 9]; 
    arg2 = ram[cur_bank][pc | 1 << 8];

    // use %X, not %x! the display font doesn't distinguish, but decode_hex() does!
    ret = f_printf(&fil, "%1X%1X%1X\r\n", op, arg1, arg2);
    if (ret < 0) {
      show_error("WRITE ERROR 2");
      f_close(&fil);
      return; 
    }

    print_string(0,2, "%1x:%02x %1x%1x%1x -> SD", cur_bank, pc, op, arg1, arg2);
    
  }

  //
  //
  //
  
  fr = f_close(&fil);
  if (fr != FR_OK) {
    show_error_wait_for_button("CANT'T CLOSE FILE");
    clear_screen(); 
  } else {
    print_string(0,3, "Saved: %s", file);
    sleep_ms(DISPLAY_DELAY);
    sleep_ms(DISPLAY_DELAY);
  }

  //
  //
  //
  
  return;

}

//
// RTC Management
// 
// Adapted from https://github.com/bnielsen1965/pi-pico-c-rtc/tree/master
// 

// write buffer to DS3231 registers
int write_DS3231_registers (uint8_t * buffer, int bufferLength) {
  int status = i2c_write_timeout_us(i2c0, I2C_ADDR, buffer, bufferLength, true, DS3231_I2C_TIMEOUT);
  if (status == PICO_ERROR_GENERIC || status == PICO_ERROR_TIMEOUT) return 1;
  return 0; 
}

// read registers from DS3231 to buffer
int read_DS3231_registers (uint8_t * buffer, int bufferLength) {
  int status = i2c_read_timeout_us(i2c0, I2C_ADDR, buffer, bufferLength, true, DS3231_I2C_TIMEOUT);
  if (status == PICO_ERROR_GENERIC || status == PICO_ERROR_TIMEOUT) return 1;
  return 0; 
}

void decode_time_buffer (uint8_t * buffer) {
  uint8_t tmp;
  //  buffer bcd time to seconds
  tmp = buffer[0];
  rtc_secs_dec = tmp & 0x0f;
  tmp >>= 4;
  rtc_secs_dec += 10 * (tmp & 0x7);
  // buffer bcd to minutes
  tmp = buffer[1];
  rtc_mins_dec = tmp & 0x0f;
  tmp >>= 4;
  rtc_mins_dec += 10 * (tmp & 0x7);
  // buffer bcd to hours
  tmp = buffer[2];
  // extract hours from the first nibble
  rtc_hours_dec = tmp & 0x0f;
  tmp >>= 4;
  // test if 12 hour mode
  if (tmp & 0x04) {
    // extract the 10 hour bit
    rtc_hours_dec += 10 * (tmp & 0x01);
    // test if PM and hour is not 12
    if (tmp & 0x02 && rtc_hours_dec != 12) rtc_hours_dec += 12;
    // else test if AM and hour is 12
    else if (!(tmp & 0x02) && rtc_hours_dec == 12) rtc_hours_dec = 0;
  }
  else {
    // extract the 10 and 20 hour bits in 24 hour mode
    rtc_hours_dec += 10 * (tmp & 0x03);
  }
}

void encode_time_buffer (uint8_t * buffer) {
  
  rtc_hours = (((uint8_t)(rtc_hours_dec) / 10) << 4) | (((uint8_t)(rtc_hours_dec) % 10));
  rtc_minutes = (((uint8_t)(rtc_mins_dec) / 10) << 4) | (((uint8_t)(rtc_mins_dec) % 10));
  rtc_seconds = (((uint8_t)(rtc_secs_dec) / 10) << 4) | (((uint8_t)(rtc_secs_dec) % 10));
  
  buffer[0] = rtc_seconds;
  buffer[1] = rtc_minutes;
  buffer[2] = rtc_hours;
 
}

void ds3231_get(void) {

  uint8_t reg = 0x00; // start register
  uint8_t buffer[DS3231_TIME_REGISTER_COUNT];
  
  memset(buffer, 0, DS3231_TIME_REGISTER_COUNT);
  // write single byte to specify the starting register for follow up reads
  if ( write_DS3231_registers(&reg, 1))
    return; 
  // read all registers into buffer
  if (read_DS3231_registers(buffer, DS3231_TIME_REGISTER_COUNT))
    return; 
  decode_time_buffer(buffer);
}

void ds3231_set(void) {
  uint8_t buffer[8];
  memset(buffer, 0, 8);
  encode_time_buffer(buffer + 1);
  write_DS3231_registers(buffer, 8);
}


//
// RTC Interface 
//

void get_time(char* buffer) { 
  ds3231_get();
  sprintf(buffer, "It is %d hours, %d minutes, and %d seconds.", rtc_hours_dec, rtc_mins_dec, rtc_secs_dec);
}

void speak_time(void) {
  get_time(current_tts);
  speak(current_tts); 
}

void set_time(void) {

  get_time(text_buffer);
  clear_screen();

  print_string(0,3,"CANCEL OR OK");

  int pos = 0; 

  uint64_t last = time_us_64();    
  bool blink = false;

  wait_for_button_release();

  while ( read_button_state() !=  OK ) {

    if ( time_us_64() - last > BLINK_DELAY) {
      last = time_us_64();
      blink = !blink;

      if (blink) {
	switch (pos) {

	case 0 :
	  print_string(0,0, "TIME    :%02d%:%02d", rtc_mins_dec, rtc_secs_dec);
	  break; 
	case 1 :
	  print_string(0,0, "TIME  %02d%:  :%02d", rtc_hours_dec, rtc_secs_dec);
	  break; 
	case 2 :
	  print_string(0,0, "TIME  %02d%:%02d:  ", rtc_hours_dec, rtc_mins_dec);
	  break; 	  

	}
      } else {

	print_string(0,0, "TIME  %02d:%02d%:%02d", rtc_hours_dec, rtc_mins_dec, rtc_secs_dec); 

      }
    }
    
    switch ( read_button_state() ) {
    case UP :
      
      switch (pos) {
      case 0 :
	if (rtc_hours_dec == 23)
	  rtc_hours_dec = 0;
	else
	  rtc_hours_dec ++;
	break; 
      case 1 :
	if (rtc_mins_dec == 59)
	  rtc_mins_dec = 0;
	else
	  rtc_mins_dec ++;
	break; 
      case 2 :
	if (rtc_secs_dec == 59)
	  rtc_secs_dec = 0;
	else
	  rtc_secs_dec ++;
	break; 
      }
      wait_for_button_release(); 
      break;

    case DOWN :
      switch (pos) {
      case 0 :
	if (rtc_hours_dec == 0)
	  rtc_hours_dec = 23;
	else
	  rtc_hours_dec--; 
	break; 
      case 1 :
	if (rtc_mins_dec == 0)
	  rtc_mins_dec = 59;
	else
	  rtc_mins_dec--; 
	break; 
      case 2 :
	if (rtc_secs_dec == 0)
	  rtc_secs_dec = 59;
	else
	  rtc_secs_dec--; 
	break; 
      }
      wait_for_button_release(); 
      break;

    case BACK: 

      if ( ! wait_for_button_release() ) { // short press?
	if (pos == 2) 
	  pos = 0; 
	else
	  pos++;
      } else {
	if (pos == 0) 
	  pos = 2;
	else 
	  pos--;
      } 
      break;
      
    case CANCEL :

      wait_for_button_release(); 

      print_string(0,3,"CANCELED!       ");
      sleep_ms(DISPLAY_DELAY);
      sleep_ms(DISPLAY_DELAY);
      clear_screen();

      return;
      
    default : break;

    }
  }

  // ok, set time 

  ds3231_set();
  ds3231_get();

  if (! SOUND_OR_TTS) 
    speak_time();

  print_string(0,2,"TIME SET TO:   ");
  print_string(0, 3, "TIME  %02d:%02d%:%02d  ", rtc_hours_dec, rtc_mins_dec, rtc_secs_dec); 
  sleep_ms(DISPLAY_DELAY);	
  sleep_ms(DISPLAY_DELAY);	
  sleep_ms(DISPLAY_DELAY);

  clear_screen();
  
}

//
// Logo Display 
// 

void show_logo() {
  print_string(0,0,"  Microtronic "); 
  print_string(0,1," PicoRAM 2090 "); 
  print_string(0,2," v1.0 (C)2023 "); 
  print_string(0,3,"  LambdaMikel "); 
  sleep_ms(750); 
}

//
// UI Loop & Extended Op-Codes 
//

void display_loop() {    

  bool armed = false; 

  bool disp_speak_echo = false; 
  disp_mode cur_disp_mode = OP_AND_MNEM; 
  button_state buttons = NONE; 
  bool refresh = true;

  //
  //
  // 
  
  arg_mode cur_arg_mode = ARG1_LOW;
  disp_refresh_mode cur_disp_refresh_mode = DISP_AUTO_REFRESH;
  arg_mode_hex_or_dec cur_arg_mode_hex_or_dec = ARGS_HEX;
  ext_op_code_mode cur_ext_op_code_mode = NOP;

  //
  //
  //

  int arg1 = 0; 
  int arg2 = 0; 
  int arg3 = 0; 
  int arg4 = 0;
  int cur_disp_x = 0; // text cursor 
  int cur_disp_y = 0;

  int cur_disp_x1 = 0; // graphics cursor 
  int cur_disp_y1 = 0;
  int cur_disp_x2 = 0;
  int cur_disp_y2 = 0;
  int cur_note_octave = 2;
  int cur_note_number = 0;
  int reg_value = 0; 

  reg_load_active = false; 
  reg_load_disarm = false; 


  //
  //
  // 

  while (true) {

    //
    //
    //

    if (refresh || (m_adr != l_adr ) ) {

      if (m_adr != l_adr ) {
	
	l_adr = m_adr; 
	l_op1 = m_op1; 
	l_op2 = m_op2;
	l_op3 = m_op3;

	if (armed) { // address changed, but use cached values

	  //
	  // REGLOAD LOADER 
	  //	      

	  if (reg_load_disarm) {

	    // switch back to old RAM bank
	    reg_load_active = false;
	    reg_load_disarm = false;

	  } else if (reg_load_active) {

	    if (  cur_reg_load_bank == 0x10 ) { // C01 from JUMP BANK has been execute; now switch to requested REGLOAD bank! 

	      cur_reg_load_bank = req_reg_load_bank;
	      
	    } else if (  l_adr == 0x31 ) { // intercept before F00 at 0x31 in REGLOAD program to return to JMP 

	      // execute GOTO then switch back to old RAM bank at next address 
	      reg_load_disarm = true;

	      // materialize jump to return address in calling program at 0x32 
	      reg_load_ram[cur_reg_load_bank][0x32] = 0xC; 
	      reg_load_ram[cur_reg_load_bank][0x32 | 1 << 9] = (regload_return_adr & 0xf0) >> 4; 
	      reg_load_ram[cur_reg_load_bank][0x32 | 1 << 8] = regload_return_adr & 0x0f;
	      
	    } else {

	      // check for REGLOAD program address -> infer register content

	      if (l_adr < 0x30) { 

		reg_value = reg_load_target_addresses[l_adr];

		if ( reg_value != 255) { // detected address and reg value it represents
	      
		  // now "simulate" a MOV xx instruction for inferred register content x
	      
		  l_op1 = 0;
		  l_op2 = reg_value;
		  l_op3 = reg_value;
		
		  //print_string(0,3,"DETECTED %02x %1x  ", l_adr, reg_value); 

		}
	      }
	    }
	    
	  } else if (! reg_load_active && l_op1 == 0x3 && l_op2 == 0xF) { // 3Fx ENTER DATA FROM REG x 

	    regload_return_adr = l_adr + 1; 
	    req_reg_load_bank = l_op3;
	    // switch to GOTO C01 bank 
	    cur_reg_load_bank = 0x10;     

	    reg_load_active = true;
	    
	  }
	  

	  //
	  // RTC LOADER
	  //

	  else if (rtc_load_disarm) {

	    // switch back to old RAM bank
	    rtc_load_active = false;
	    rtc_load_disarm = false;
	    rtc_loaded = false; 
	    
	  } else if (rtc_load_active) {

	    if (  cur_reg_load_bank == 0x10 ) { // C01 from JUMP BANK has been execute; now switch to requested REGLOAD bank! 

	      cur_reg_load_bank = RTC_LOAD_BANK;

	    } else if(  cur_reg_load_bank == RTC_LOAD_BANK ) { // STARTED CLOCK LOAD PROGRAM; MICROTRONIC IS EXECUTING SOME NOPS, GIVE TIME, WRITE MEMORY! 

	      if (! rtc_loaded ) {
		
		rtc_loaded = true; 
		ds3231_get(); 

		//
		// now, getting the RTC and writing the REGLOAD program takes time
		// Microtronic is currently executing NOPs from 01 to ... here 
		// so pick start time = address that will match its speeed, eg, at 0x20
		///
		
		reg_load_ram[RTC_LOAD_BANK][0x20] = 0x1; 
		reg_load_ram[RTC_LOAD_BANK][0x20 | 1 << 9] = rtc_secs_dec % 10; 
		reg_load_ram[RTC_LOAD_BANK][0x20 | 1 << 8] = 0xa;
		
		reg_load_ram[RTC_LOAD_BANK][0x21] = 0x1; 
		reg_load_ram[RTC_LOAD_BANK][0x21 | 1 << 9] = rtc_secs_dec / 10; 
		reg_load_ram[RTC_LOAD_BANK][0x21 | 1 << 8] = 0xb;
		
		reg_load_ram[RTC_LOAD_BANK][0x22] = 0x1; 
		reg_load_ram[RTC_LOAD_BANK][0x22 | 1 << 9] = rtc_mins_dec % 10;
		reg_load_ram[RTC_LOAD_BANK][0x22 | 1 << 8] = 0xc;
		
		reg_load_ram[RTC_LOAD_BANK][0x23] = 0x1; 
		reg_load_ram[RTC_LOAD_BANK][0x23 | 1 << 9] = rtc_mins_dec / 10; 
		reg_load_ram[RTC_LOAD_BANK][0x23 | 1 << 8] = 0xd; 
		
		reg_load_ram[RTC_LOAD_BANK][0x24] = 0x1; 
		reg_load_ram[RTC_LOAD_BANK][0x24 | 1 << 9] = rtc_hours_dec % 10;
		reg_load_ram[RTC_LOAD_BANK][0x24 | 1 << 8] = 0xe;
		
		reg_load_ram[RTC_LOAD_BANK][0x25] = 0x1; 
		reg_load_ram[RTC_LOAD_BANK][0x25 | 1 << 9] = rtc_hours_dec / 10;
		reg_load_ram[RTC_LOAD_BANK][0x25 | 1 << 8] = 0xf;
		
		// after that, materialize jump back to return address at 2F / 30
		
	      } else if (  l_adr == 0x2f ) { // intercept CLOCK program to return to JMP 

		rtc_load_disarm = true; 
		
		// materialize jump to return address in calling program;
		// give Pico some time, hence return at 0x30 
		reg_load_ram[RTC_LOAD_BANK][0x30] = 0xC; 
		reg_load_ram[RTC_LOAD_BANK][0x30 | 1 << 9] = (regload_return_adr & 0xf0) >> 4; 
		reg_load_ram[RTC_LOAD_BANK][0x30 | 1 << 8] = regload_return_adr & 0x0f;

	      }
	    }

	  } else if (! rtc_load_active && l_op1 == 0xF && l_op2 == 0x00 && l_op3 == 0x06) { // F06 - TIME - LOAD FROM RTC

	    regload_return_adr = l_adr + 1; 
	    // switch to GOTO C01 bank 
	    cur_reg_load_bank = 0x10;     

	    rtc_load_active = true;
	    rtc_loaded = false;
	    rtc_load_disarm = false; 
	    
	  } 

	
	  //
	  //
	  //

	  if (! l_op1 && l_op2 == l_op3) { // literal data input MOV 0xx = 0xx 
	  
	    switch (cur_arg_mode) {
	    case ARG1_LOW  : arg1 = l_op2; break;
	    case ARG1_HIGH : arg1 += (cur_arg_mode_hex_or_dec == ARGS_HEX ? 16* l_op2 : 10 * l_op2) ; break;
	    case ARG2_LOW  : arg2 = l_op2; break;
	    case ARG2_HIGH : arg2 += (cur_arg_mode_hex_or_dec == ARGS_HEX ? 16* l_op2 : 10 * l_op2) ; break;
	    case ARG3_LOW  : arg3 = l_op2; break;
	    case ARG3_HIGH : arg3 += (cur_arg_mode_hex_or_dec == ARGS_HEX ? 16* l_op2 : 10 * l_op2) ; break;
	    case ARG4_LOW  : arg4 = l_op2; break;
	    case ARG4_HIGH : arg4 += (cur_arg_mode_hex_or_dec == ARGS_HEX ? 16* l_op2 : 10 * l_op2) ; break;
	    }

	    //
	    // process args -> route args to currently enabled op-code and state machine 
	    // 

	    switch (cur_ext_op_code_mode) {
	    
	    case DISP_CLEAR_LINE :
	      // one <low> argument 
	      if (cur_arg_mode == ARG1_LOW) {
		arg1 %= 4; 
		if  (cur_disp_refresh_mode == DISP_AUTO_REFRESH)
		  clear_line(arg1);
		else
		  clear_line0(arg1);
		cur_disp_x = 0;
		cur_disp_y = arg1; 
	      }
	      break;
	    
	    case DISP_SHOW_CHAR :
	      // <low><high> char argument
	      if (cur_arg_mode == ARG1_LOW) {
		cur_arg_mode = ARG1_HIGH;
	      } else if (cur_arg_mode == ARG1_HIGH) {
		cur_arg_mode = ARG1_LOW;
		if (arg1 == 10) {
		  cur_disp_y++;
		  if (disp_speak_echo) {
		    speak(current_tts); 	   
		    cur_tts_index = 0;
		    memset(&current_tts, 0, TTS_BUFF_SIZE);
		  }
		} else if (arg1 == 13) {
		  cur_disp_x = 0;
		  if (disp_speak_echo) {		  
		    speak(current_tts); 	   
		    cur_tts_index = 0;
		    memset(&current_tts, 0, TTS_BUFF_SIZE);
		  }
		} else {
		  if (cur_disp_refresh_mode == DISP_AUTO_REFRESH)
		    print_char(cur_disp_x, cur_disp_y, arg1); 
		  else
		    print_char0(cur_disp_x, cur_disp_y, arg1);
		  
		  if (disp_speak_echo)
		    current_tts[cur_tts_index++] = arg1;
		  
		  cur_disp_x++;		
		  if (cur_disp_x == 16) {
		    cur_disp_y++;
		    cur_disp_x = 0;

		    if (disp_speak_echo)
		      current_tts[cur_tts_index++] = ' ';
		  }
		}
		
		if (cur_disp_y == 4) {
		  cur_disp_y = 0;
		  
		}
	      } 
	      break;
	    
	    case DISP_SET_CURSOR_AT_LINE :
	      // one single <low> argument
	      if (cur_arg_mode == ARG1_LOW) {
		cur_disp_y = arg1;
		cur_disp_x = 0; 
	      }
	      break;
	    
	    case DISP_SET_CURSOR_XY :
	      // two single <low> arguments (x 0..15, y 0..3)
	      // use ARG1_LOW and ARG2_LOW! 
	      if (cur_arg_mode == ARG1_LOW) {
		cur_disp_x = arg1 % 16;
		cur_arg_mode = ARG2_LOW; 
	      } else if (cur_arg_mode == ARG2_LOW) {
		cur_disp_y = arg2 % 4;
		cur_arg_mode = ARG1_LOW;
	      }
	      break;
	    
	    case DISP_PLOT :
	      // two <low><high> arguments 
	      // full ARG1 and ARG2
	      if (cur_arg_mode == ARG1_LOW) {
		cur_arg_mode = ARG1_HIGH;
	      } else if (cur_arg_mode == ARG1_HIGH) {
		cur_disp_x1 = arg1 % 0x80;
		cur_disp_x2 = cur_disp_x1;
		cur_arg_mode = ARG2_LOW;
	      } else if  (cur_arg_mode == ARG2_LOW) {
		cur_arg_mode = ARG2_HIGH;
	      } else if (cur_arg_mode == ARG2_HIGH) {
		cur_disp_y1 = arg2 % 0x20;
		cur_disp_y2 = cur_disp_y1;
		cur_arg_mode = ARG1_LOW;
	      
		if  (cur_disp_refresh_mode == DISP_AUTO_REFRESH) {
		  disp_plot(cur_disp_x1, cur_disp_y1);
		} else {
		  disp_plot0(cur_disp_x1, cur_disp_y1);
		}
	      }
	      break;
	    
	    case DISP_LINE_FROM : 
	      // two <low><high> arguments x2, y2 
	      // use x2 so we can refer to previous line arg x1
	      if (cur_arg_mode == ARG1_LOW) {
		cur_arg_mode = ARG1_HIGH;
	      } else if (cur_arg_mode == ARG1_HIGH) {
		cur_disp_x2 = arg1 % 0x80;
		cur_arg_mode = ARG2_LOW;
	      } else if (cur_arg_mode == ARG2_LOW) {
		cur_arg_mode = ARG2_HIGH;
	      } else if (cur_arg_mode == ARG2_HIGH) {
		cur_disp_y2 = arg2 % 0x20;
		cur_arg_mode = ARG1_LOW;
	      
		if  (cur_disp_refresh_mode == DISP_AUTO_REFRESH) {
		  disp_line(cur_disp_x1, cur_disp_y1, cur_disp_x2, cur_disp_y2 );
		} else {
		  disp_line0(cur_disp_x1, cur_disp_y1, cur_disp_x2, cur_disp_y2);
		}
	      } 
	      break;
	      
	    case DISP_LINE_TO : 
	      // two <low><high> arguments x1, y1 
	      // use x1 so we can refer to previous line arg x2
	      if (cur_arg_mode == ARG1_LOW) {
		cur_arg_mode = ARG1_HIGH;
	      } else if (cur_arg_mode == ARG1_HIGH) {
		cur_disp_x1 = arg1 % 0x80;
		cur_arg_mode = ARG2_LOW;
	      } else if (cur_arg_mode == ARG2_LOW) {
		cur_arg_mode = ARG2_HIGH;
	      } else if (cur_arg_mode == ARG2_HIGH) {
		cur_disp_y1 = arg2 % 0x20;
		cur_arg_mode = ARG1_LOW;
	      
		if  (cur_disp_refresh_mode == DISP_AUTO_REFRESH) {
		  disp_line(cur_disp_x2, cur_disp_y2, cur_disp_x1, cur_disp_y1 );
		} else {
		  disp_line0(cur_disp_x2, cur_disp_y2, cur_disp_x1, cur_disp_y1);
		}
		cur_disp_x2 = cur_disp_x1; 
		cur_disp_y2 = cur_disp_y1; 
	      } 
	      break;
	      
	    case DISP_LINE :
	      // four <low><high> arguments x1, x2, x2, y2
	      if (cur_arg_mode == ARG1_LOW) {
		cur_arg_mode = ARG1_HIGH; 
	      } else if (cur_arg_mode == ARG1_HIGH) {
		cur_disp_x1 = arg1 % 0x80;
		cur_arg_mode = ARG2_LOW;
	      } else if (cur_arg_mode == ARG2_LOW) {
		cur_arg_mode = ARG2_HIGH; 
	      } else if (cur_arg_mode == ARG2_HIGH) {
		cur_disp_y1 = arg2 % 0x20;
		cur_arg_mode = ARG3_LOW;
	      } else if (cur_arg_mode == ARG3_LOW) {
		cur_arg_mode = ARG3_HIGH; 
	      } else if (cur_arg_mode == ARG3_HIGH) {
		cur_disp_x2 = arg3 % 0x80;
		cur_arg_mode = ARG4_LOW;
	      } else if (cur_arg_mode == ARG4_LOW) {
		cur_arg_mode = ARG4_HIGH; 
	      } else if (cur_arg_mode == ARG4_HIGH) {
		cur_disp_y2 = arg4 % 0x20;
		cur_arg_mode = ARG1_LOW;
	      
		if  (cur_disp_refresh_mode == DISP_AUTO_REFRESH) {
		  disp_line(cur_disp_x1, cur_disp_y1, cur_disp_x2, cur_disp_y2 );
		} else {
		  disp_line0(cur_disp_x1, cur_disp_y1, cur_disp_x2, cur_disp_y2);
		}
		// so that we can continue from the endpoint via line-to: 
		cur_disp_x1 = cur_disp_x2; 
		cur_disp_y1 = cur_disp_y2; 
	      }  
	      break;
	    
	    case SOUND_PLAY_NOTE :
	      // two single <low> arguments: octave, note number 
	      // use ARG1_LOW and ARG2_LOW!	      
	      if (cur_arg_mode == ARG1_LOW) {
		cur_note_octave = arg1 % 9;
		cur_arg_mode = ARG2_LOW;
	      } else if (cur_arg_mode == ARG2_LOW) {
		cur_note_number = arg2;
		cur_arg_mode = ARG1_LOW;

		//play_tone_1(slice_num, 0); 
		play_tone_1(slice_num, NOTES_ARR[cur_note_octave][cur_note_number]); 
	      } 
	      break;
	    	    	    
	    case SPEAK_BYTE :
	      if (cur_arg_mode == ARG1_LOW) {
		cur_arg_mode = ARG1_HIGH;
	      } else if (cur_arg_mode == ARG1_HIGH) {
		cur_arg_mode = ARG1_LOW;

		if (arg1 == 10 || arg1 == 13 || cur_tts_index == (TTS_BUFF_SIZE - 2)) {
		  //tts_speech(current_tts, 1);
		  //current_tts[cur_tts_index] = 0; 
		  speak(current_tts); 	   
		  cur_tts_index = 0;
		  memset(&current_tts, 0, TTS_BUFF_SIZE);
		} else {
		  current_tts[cur_tts_index++] = arg1;
		}	      
	      }
	      break;
	    	    
	    default:
	      break; 
	    
	    }
	  }

	  //
	  // extended op-code decoding
	  //

	  else if ( l_op1 == 5 && ! l_op2) {

	    cur_arg_mode = ARG1_LOW; 

	    if ( ! l_op3 ) { // 500 HEX DATA ENTRY MODE
	      cur_arg_mode_hex_or_dec = ARGS_HEX;
	    } else if ( l_op3 == 1 ) { // 501 DEC DATA ENTRY MODE
	      cur_arg_mode_hex_or_dec = ARGS_HEX;
	    } else if ( l_op3 == 2 ) { // 502 DISP CLEAR SCREEN
	      cur_disp_x = 0;
	      cur_disp_y = 0;
	      if ( cur_disp_refresh_mode == DISP_AUTO_REFRESH)
		clear_screen();
	      else
		clear_screen0();
	    } else if ( l_op3 == 3 ) { // 503 DISP TOGGLE UPDATE
	      if ( cur_disp_refresh_mode == DISP_AUTO_REFRESH)
		cur_disp_refresh_mode = DISP_MANUAL_REFRESH;
	      else
		cur_disp_refresh_mode = DISP_AUTO_REFRESH;
	    } else if ( l_op3 == 4 ) { // 504 DISP REFRESH
	      render_display();
	    } else if ( l_op3 == 5 ) { // 505 DISP CLEAR LINE <X>
	      cur_ext_op_code_mode = DISP_CLEAR_LINE;
	    } else if ( l_op3 == 6 ) { // 506 DISP SHOW CHAR <LOW><HIGH>
	      cur_ext_op_code_mode = DISP_SHOW_CHAR;
	    } else if ( l_op3 == 7 ) { // 507 DISP CURSOR SET CURSOR LINE <X>
	      cur_ext_op_code_mode = DISP_SET_CURSOR_AT_LINE;
	    } else if ( l_op3 == 8 ) { // 508 DISP SET CURSOR <X> <Y>
	      cur_ext_op_code_mode = DISP_SET_CURSOR_XY;
	    } else if ( l_op3 == 9 ) { // 509 DISP PLOT <X> <Y> 
	      cur_ext_op_code_mode = DISP_PLOT;
	    } else if ( l_op3 == 0xa ) { // 50A DISP LINE <X1> <Y1> <X2> <Y2>
	      cur_ext_op_code_mode = DISP_LINE;
	    } else if ( l_op3 == 0xb ) { // 50B DISP LINE FROM <X> <Y> 
	      cur_ext_op_code_mode = DISP_LINE_FROM;
	    } else if ( l_op3 == 0xc ) { // 50C DISP LINE TO <X> <Y> 
	      cur_ext_op_code_mode = DISP_LINE_TO;
	    } else if ( l_op3 == 0xd ) { // 50D SOUND PLAY NOTE <X><Y>
	      if (SOUND_OR_TTS) {
		cur_ext_op_code_mode = SOUND_PLAY_NOTE;
		play_tone_1(slice_num, 0);
	      }
	    } else if ( l_op3 == 0xe ) { // 50E DISP SPEAK ECHO 
	      if (!SOUND_OR_TTS) {
		//speak("Echo on"); 
		disp_speak_echo = true;
		tts_stop();
		cur_tts_index = 0;
		memset(&current_tts, 0, TTS_BUFF_SIZE);
	      }
	    } else if ( l_op3 == 0xf ) { // 50F SPEAK BYTE <LOW><HIGH>
	      if (!SOUND_OR_TTS) {
		//memset(&current_tts, 0, TTS_BUFF_SIZE);
		cur_ext_op_code_mode = SPEAK_BYTE;
		//cur_tts_index = 0;		
	      }
	    }
	  }

	  // bank switching 

	  else if ( l_op1 == 7 && ! l_op2) { // 70x SWITCH MEMORY BANK x
	  
	    cur_bank = l_op3;
	    cur_arg_mode = ARG1_LOW; 

	  } 
	}
      }
      
      //
      //
      //
      
      if (cur_disp_mode != OFF && ! reg_load_active && ! reg_load_disarm ) { // REGLOAD is timing critical, disable!

	sprintf(text_buffer, "#%1x:%02x %1x%1x%1x %s %s", cur_bank, m_adr, m_op1, m_op2, m_op3, SOUND_OR_TTS ? "SND" : (disp_speak_echo ? "TTSE" : "TTS-"), armed ? "*" : "-");
	WriteString(buf, 0, 0, text_buffer);
	            
	if (cur_disp_mode == OP_AND_MNEM) {
	  get_mnem(1);
	  sprintf(text_buffer, "#%1x:%02x %s", cur_bank, m_adr, mnemonic);
	  WriteString(buf, 0, 8, text_buffer);
	}

	if (armed) {
	  sprintf(text_buffer, "OP:%1x  %1x>%02x%02x%02x%02x", cur_ext_op_code_mode, cur_arg_mode, arg1, arg2, arg3, arg4); 
	  WriteString(buf, 0, 16, text_buffer);
	}

	render(buf, &frame_area);  
      }
    }

    //
    //
    //

    buttons = read_button_state(); 

    if (buttons != NONE ) {
      
      refresh = true;
      
      /* if (SOUND_OR_TTS) 
	 play_tone_1(slice_num, 0); */ 
	
      switch (buttons) {
      case UP: 
	// LOAD

	if ( wait_for_button_release()) { // short press?
	  if (! SOUND_OR_TTS)
	    speak("Testing"); 
	  else {
	    play_test_sound();
	  }
	} else {
	  pgm1();
	}

	sleep_ms(DISPLAY_DELAY);
	sleep_ms(DISPLAY_DELAY);
	clear_screen(); 

	break;
	
      case DOWN: 
	// SAVE
	if ( wait_for_button_release()) { // short press?
	  clear_screen();
	  ls();
	} else {
	  pgm2();
	}
	
	sleep_ms(DISPLAY_DELAY);
	sleep_ms(DISPLAY_DELAY);
	clear_screen();
	
	break;
	
      case BACK:
	// CHANGE CUR BANK
	if (! wait_for_button_release()) {
	  cur_bank = (cur_bank + 1) % (MAX_BANKS);

	  clear_screen();
	  sprintf(text_buffer, "BANK #%1x", cur_bank); 
	  WriteString(buf, 0, 0, text_buffer);
	  render(buf, &frame_area);
	  sleep_ms(DISPLAY_DELAY);
	  clear_screen();
	} else {

	  if (! SOUND_OR_TTS) 
	    speak_time();
	  
	}
	  	
	break;

      case OK: 
	// ARM
	if (! wait_for_button_release()) {

	  cur_disp_x = 0;
	  cur_disp_y = 0;

	  cur_disp_x1 = 0;
	  cur_disp_y1 = 0;
	  cur_disp_x2 = 0;
	  cur_disp_y2 = 0;
	  cur_note_octave = 2;
	  cur_note_number = 0;
	
	  cur_arg_mode = ARG1_LOW;      
	  cur_disp_refresh_mode = DISP_AUTO_REFRESH;
	  disp_speak_echo = false; 
	  cur_arg_mode_hex_or_dec = ARGS_HEX;
	  cur_ext_op_code_mode = NOP;

	  reg_load_active = false; 
	  reg_load_disarm = false; 
	  rtc_load_active = false; 
	  rtc_load_disarm = false;
	  rtc_loaded = false;	 
      
	  if (SOUND_OR_TTS) 
	    play_tone_1(slice_num, 0);

	  armed = !armed;
	
	  clear_screen(); 
	  sprintf(text_buffer, "%s", armed ? "  OP-EXT ON (*)" : "  OP-EXT OFF (-)");
	  WriteString(buf, 0, 0, text_buffer);
	  render(buf, &frame_area);
	  sleep_ms(DISPLAY_DELAY);	


	} else {

	  get_time(text_buffer);
	  sprintf(text_buffer, "TIME  %02d:%02d%:%02d  ", rtc_hours_dec, rtc_mins_dec, rtc_secs_dec); 
	  WriteString(buf, 0, 16, text_buffer);
	  render(buf, &frame_area);
	  sleep_ms(DISPLAY_DELAY);	
	  sleep_ms(DISPLAY_DELAY);	
	  sleep_ms(DISPLAY_DELAY);	
	  
	}
	
	sleep_ms(DISPLAY_DELAY);	
	clear_screen();

	break; 

      case CANCEL:

	if ( ! wait_for_button_release() ) {
	  
	  cur_disp_mode = (cur_disp_mode + 1) % (OP_AND_MNEM + 1); 
	
	  if (cur_disp_mode == OFF ) 
	    clear_screen();
	  
	} else {

	  set_time(); 	  
	  
	}
	
	break;

      default: break; 
	
      }
    }

    // while (1) 
  }

  // display_loop
  
}

//
// Main Loop 
//

int main() {

  // Set system clock speed.
  // 125 MHz
  //vreg_set_voltage(VREG_VOLTAGE_1_30);
  set_sys_clock_pll(1000000000, 4, 1);

  //
  // Configure Hardware Extensions 
  //
    
  stdio_init_all();

#ifdef TTS
  gpio_init(EPS_CS);
  gpio_put(EPS_CS, 1); 
  gpio_set_dir(EPS_CS, GPIO_OUT);

  gpio_init(EPS_RESET);
  gpio_put(EPS_RESET, 1); 
  gpio_set_dir(EPS_RESET, GPIO_OUT);
#endif

  //
  // Onboard LED 
  //
  
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);      

  //
  //
  //

  adc_init();    
  adc_gpio_init(ADC_KEYS_INPUT);

  //
  // SD Card Init 
  //

  ssd1306_setup();   
  sd_test(); 
  //ls(); 

  //
  // 
  //

#ifdef TTS
  gpio_init(EPS_READY);
  gpio_set_dir(EPS_READY, GPIO_IN);
  gpio_set_inover(EPS_READY, GPIO_OVERRIDE_NORMAL);
#endif 

  //
  // Show Logo 
  // 
 
  sleep_ms(100);
  clear_screen(); 
  show_logo();

  //
  // Configure Sound or TTS 
  // 

#ifdef TTS
  SOUND_OR_TTS = gpio_get(EPS_READY);
#else
  SOUND_OR_TTS = 1;
#endif 

  if (! SOUND_OR_TTS) {
    tts_setup();
  } else {
    gpio_set_function(SOUND_OUTPUT, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(SOUND_OUTPUT);
    play_test_sound(); 
  }

  //
  //
  // 
    
  clear_screen();

  //
  // Configure GPIO Pins - Adress But, Data Bus, WE, DISP Signal 
  // 
  
  for (gpio = ADR_INPUTS_START; gpio < DATA_GPIO_START; gpio++) {
    addr_mask |= (1 << gpio);
    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO); 
    gpio_set_dir(gpio, GPIO_IN);
    gpio_set_inover(gpio, GPIO_OVERRIDE_NORMAL);
  }

  for (gpio = DATA_GPIO_START; gpio < DATA_GPIO_END; gpio++) {
    data_mask |= (1 << gpio);
    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO); 
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_set_outover(gpio, GPIO_OVERRIDE_NORMAL);
  }

  gpio_init(WE_INPUT);
  gpio_set_function(WE_INPUT, GPIO_FUNC_SIO); 
  gpio_set_dir(WE_INPUT, GPIO_IN);
  gpio_pull_up(WE_INPUT); 
  //gpio_set_inover(WE_INPUT, GPIO_OVERRIDE_PULLUP);

  gpio_init(DISP_INPUT);
  gpio_set_function(DISP_INPUT, GPIO_FUNC_SIO); 
  gpio_set_dir(DISP_INPUT, GPIO_IN);
  gpio_pull_up(DISP_INPUT); 
  //gpio_set_inover(WE_INPUT, GPIO_OVERRIDE_PULLUP);
    
  //
  // Helper Vars (Required to Identify Current Op-Code) 
  //

  uint32_t adr  = 0; 
  uint32_t adr1 = 0; 
  uint32_t adr2 = 0; 
  uint32_t adr3 = 0; 
  uint32_t adr4 = 0; 

  uint64_t val = 0;    

  //
  //
  //

  bool change = false;     
  bool disp = false; 
  bool we = false; 
  bool last_we = false; 

  //
  // Prepare SRAM Memory Banks 
  //

  cur_bank = 0; 
    
  for (uint8_t pgm = 0; pgm < MAX_PGMS; pgm++) 
    enter_program(pgm);  
    
  for (uint8_t pgm = MAX_PGMS; pgm < MAX_BANKS; pgm++) {
    clear_bank(pgm); 
  }

  //
  //
  //

  prepare_regload_banks(); 

  //
  // Initialize RTC and Get Time 
  //

  ds3231_get();
  
  //
  // Launch Display Loop / UI and Extended Op-Codes on 2nd Core 
  //

  multicore_launch_core1(display_loop);

  //
  // Main Core Tight Busy Loop - 
  // Emulate SRAM, and Identify Current Address and Current Op-Code / Instruction 
  // 

  while (true) {  
       
    disp = true; 
    change = false; 

    while (disp) {
      disp = gpio_get(DISP_INPUT); 
    }

    adr = (gpio_get_all() & addr_mask) >> ADR_INPUTS_START ;        
    we = gpio_get(WE_INPUT); 

    if (adr != adr1 || we != last_we ) {
      adr4 = adr3;   
      adr3 = adr2; 
      adr2 = adr1; 
      adr1 = adr; 
      change = true; 
      last_we = we; 
    } else {
      change = false; 
    }

    // change GPIO dir if required 
                
    if (change ) {

      gpio_put(LED_PIN, 0);

      last_we = we; 
    
      if (we) {
	// READ SRAM - SET ALL DATA LINES TO OUTPUT (1 IN MASK = OUTPUT)
	gpio_set_dir_masked(data_mask, data_mask); 
	sleep_us(3);

	if (reg_load_active || rtc_load_active) { // THIS ALSO LOAD RTC LOADER PROGRAM!
	  val = reg_load_ram[cur_reg_load_bank][adr];
	} else {
	  val = ram[cur_bank][adr];
	}

	// val = reverse_bits(val); 
	
	gpio_put_masked(data_mask, ~ (val << DATA_GPIO_START));

	if (adr4) { // to do: make the detection more robust, even when F02 holds...
	  if (adr & (1 << 8)) {
	    if (adr3 & (1 << 9)) {
	      if ( (adr & adr3 ) == adr4) {                    
		m_adr = adr4 & 0xff;
		if (! reg_load_active ) {
		  gpio_put(LED_PIN, 1);		
		  m_op1 = ram[cur_bank][adr4];
		  m_op2 = ram[cur_bank][adr3];
		  m_op3 = ram[cur_bank][adr];
		}
	      }
	    }
	  }
	}
                
      } else {
	// WRITE SRAM 
	gpio_set_dir_masked(data_mask, 0);
	sleep_us(3);

	val = gpio_get(12) | ( gpio_get(13) <<  1) | (gpio_get(14) << 2) | (gpio_get(15) << 3);	
	// val = gpio_get(12) << 3 | ( gpio_get(13) <<  2) | (gpio_get(14) << 1) | gpio_get(15);
	// val = reverse_bits(val);
	
	ram[cur_bank][adr] = (~ val) & 0xf; 

	gpio_put(LED_PIN, 1);
      }
    }
  }
}

