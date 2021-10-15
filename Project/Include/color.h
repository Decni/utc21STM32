#ifndef __COLOR_H
#define __COLOR_H
/* 
    ANSI Color
*/
#define COLOR_START "\033["
#define COLOR_STOP "\033[0m"

#define COLOR_TYPE_BOLD "1m"
#define COLOR_TYPE_UNDERLINE "4m"
#define COLOR_TYPE_BLINK "5m"
#define COLOR_TYPE_INVERSE "7m"
#define COLOR_TYPE_INVISIBLE "8m"
#define COLOR_TYPE_NONE "2m"
 
#define COLOR_BG_BLACK    "40;"
#define COLOR_BG_RED    "41;"
#define COLOR_BG_GREEN    "42;"
#define COLOR_BG_BROWN    "43;"
#define COLOR_BG_BLUE    "44;"
#define COLOR_BG_PURPLE    "45;"
#define COLOR_BG_BLUE2    "46;"
#define COLOR_BG_GREY    "47;"
 
#define COLOR_FG_BLACK    "30;"
#define COLOR_FG_RED    "31;"
#define COLOR_FG_GREEN    "32;"
#define COLOR_FG_YELLOW    "33;"
#define COLOR_FG_BLUE    "34;"
#define COLOR_FG_PURPLE    "35;"
#define COLOR_FG_BLUE2    "36;"
#define COLOR_FG_WHITE    "37;"

#endif
