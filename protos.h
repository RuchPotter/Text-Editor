/*Copyright 2016 Ruchira Paranjpe paranjper15.it@coep.ac.in

This program is a part of the project Charlie.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>

int linux_console;
#define O_BINARY 0


#define MINEXTEND      32768
#define LINEBUF_EXTRA  32

#ifndef TABSIZE
#define TABSIZE        8
#endif

#ifndef INDENT
#define INDENT "  "
#endif

#define CLRSCR           "\033[0J"
#define CLREOL           "\033[K"
#define GOTOXY           "\033[%d;%dH"
#define RESET_COLOR      "\033[0m"

#ifdef COLOR
#define TEXT_COLOR       "\033[44m\033[37m\033[1m"
#define SELECT_COLOR     "\033[47m\033[37m\033[1m"
#define STATUS_COLOR     "\033[0m\033[47m\033[30m"
#else
#define TEXT_COLOR       "\033[0m"
#define SELECT_COLOR     "\033[7m\033[1m"
#define STATUS_COLOR     "\033[1m\033[7m"
#endif

#define KEY_BACKSPACE        0x101
#define KEY_ESC              0x102
#define KEY_INS              0x103
#define KEY_DEL              0x104
#define KEY_LEFT             0x105
#define KEY_RIGHT            0x106
#define KEY_UP               0x107
#define KEY_DOWN             0x108
#define KEY_HOME             0x109
#define KEY_END              0x10A
#define KEY_ENTER            0x10B
#define KEY_TAB              0x10C
#define KEY_PGUP             0x10D
#define KEY_PGDN             0x10E

#define KEY_CTRL_LEFT        0x10F
#define KEY_CTRL_RIGHT       0x110
#define KEY_CTRL_UP          0x111
#define KEY_CTRL_DOWN        0x112
#define KEY_CTRL_HOME        0x113
#define KEY_CTRL_END         0x114
#define KEY_CTRL_TAB         0x115

#define KEY_SHIFT_LEFT       0x116
#define KEY_SHIFT_RIGHT      0x117
#define KEY_SHIFT_UP         0x118
#define KEY_SHIFT_DOWN       0x119
#define KEY_SHIFT_PGUP       0x11A
#define KEY_SHIFT_PGDN       0x11B
#define KEY_SHIFT_HOME       0x11C
#define KEY_SHIFT_END        0x11D
#define KEY_SHIFT_TAB        0x11E

#define KEY_SHIFT_CTRL_LEFT  0x11F
#define KEY_SHIFT_CTRL_RIGHT 0x120
#define KEY_SHIFT_CTRL_UP    0x121
#define KEY_SHIFT_CTRL_DOWN  0x122
#define KEY_SHIFT_CTRL_HOME  0x123
#define KEY_SHIFT_CTRL_END   0x124

#define KEY_F1               0x125
#define KEY_F3               0x126
#define KEY_F5               0x127

#define KEY_UNKNOWN          0xFFF

#define ctrl(c) ((c) - 0x60)

struct env;

 /* Undo/ Redo currently not working properly */

struct undo {
  int pos;                 // Editor position
  int erased;              // Size of erased contents
  int inserted;            // Size of inserted contents
  unsigned char *undobuf;  // Erased contents for undo
  unsigned char *redobuf;  // Inserted contents for redo
  struct undo *next;       // Next undo buffer
  struct undo *prev;       // Previous undo buffer
};

struct editor {
  unsigned char *start;      // Start of text buffer
  unsigned char *gap;        // Start of gap
  unsigned char *rest;       // End of gap
  unsigned char *end;        // End of text buffer

  int toppos;                // Text position for current top screen line
  int topline;               // Line number for top of screen
  int margin;                // Position for leftmost column on screen

  int linepos;               // Text position for current line
  int line;                  // Current document line
  int col;                   // Current document column
  int lastcol;               // Remembered column from last horizontal navigation
  int anchor;                // Anchor position for selection
  
  struct undo *undohead;     // Start of undo buffer list
  struct undo *undotail;     // End of undo buffer list
  struct undo *undo;         // Undo/redo boundary

  int refresh;               // Flag to trigger screen redraw
  int lineupdate;            // Flag to trigger redraw of current line
  int dirty;                 // Dirty flag is set when the editor buffer has been changed

  int newfile;               // File is a new file
  int permissions;           // File permissions

  struct env *env;           // Reference to global editor environment
  struct editor *next;       // Next editor
  struct editor *prev;       // Previous editor

  char filename[FILENAME_MAX];
};

struct env {
  struct editor *current;   // Current editor

  unsigned char *clipboard; // Clipboard
  int clipsize;             // Clipboard size

  unsigned char *search;    // Search text
  unsigned char *linebuf;   // Scratch buffer

  int cols;                 // Console columns
  int lines;                // Console lines
 
  int untitled;             // Counter for untitled files
};

void get_modifier_keys(int *shift, int *ctrl);
int getkey();
int prompt(struct editor *ed, char *msg, int selection);
int ask();
void outch(char c);

void outbuf(unsigned char *buf, int len);

void outstr(char *str);
void clear_screen();
void gotoxy(int col, int line);
