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
#include "protos.h"

/* Functions for capturing keyboard inputs. The captured input is stored in a variable. It is then compared with the hex addresses of all the keys ( which are known ), and operations are then performed accordingly */ 

void get_modifier_keys(int *shift, int *ctrl) {
  *shift = *ctrl = 0;
    char modifiers = 6;
    if (ioctl(0, TIOCLINUX, &modifiers) >= 0) {
      if (modifiers & 1) *shift = 1;
      if (modifiers & 4) *ctrl = 1;
    }
}

int getkey() {
  int ch, shift, ctrl;

  ch = getchar();
  if (ch < 0) return ch;

  switch (ch) {
    case 0x08: return KEY_BACKSPACE;
    case 0x09:
      get_modifier_keys(&shift, &ctrl);
      if (shift) return KEY_SHIFT_TAB;
      if (ctrl) return KEY_CTRL_TAB;
      return KEY_TAB;
    case 0x0D: return KEY_ENTER;
    case 0x0A: return KEY_ENTER;

    case 0x1B:
      ch = getchar();
      switch (ch) {
        case 0x1B: return KEY_ESC;
        case 0x4F:
          ch = getchar();
          switch (ch) {
            case 0x46: return KEY_END;
            case 0x48: return KEY_HOME;
            case 0x50: return KEY_F1;
            case 0x52: return KEY_F3;
            case 0x54: return KEY_F5;
            default: return KEY_UNKNOWN;
          }
          break;

        case 0x5B:
          get_modifier_keys(&shift, &ctrl);
          ch = getchar();
          if (ch == 0x31) {
            ch = getchar();
            switch (ch) {
              case 0x3B:
                ch = getchar();
                if (ch == 0x32) shift = 1;
                if (ch == 0x35) ctrl = 1;
                if (ch == 0x36) shift = ctrl = 1;
                ch = getchar();
                break;
              case 0x35: 
                return getchar() == 0x7E ? KEY_F5 : KEY_UNKNOWN;
              case 0x7E:
                if (shift && ctrl) return KEY_SHIFT_CTRL_HOME;
                if (shift) return KEY_SHIFT_HOME;
                if (ctrl) return KEY_CTRL_HOME;
                return KEY_HOME;
              default:
                return KEY_UNKNOWN;
            }
          }

          switch (ch) {
            case 0x31: 
              if (getchar() != 0x7E) return KEY_UNKNOWN;
              if (shift && ctrl) return KEY_SHIFT_CTRL_HOME;
              if (shift) return KEY_SHIFT_HOME;
              if (ctrl) return KEY_CTRL_HOME;
              return KEY_HOME;
            case 0x32: return getchar() == 0x7E ? KEY_INS : KEY_UNKNOWN;
            case 0x33: return getchar() == 0x7E ? KEY_DEL : KEY_UNKNOWN;
            case 0x34:
              if (getchar() != 0x7E) return KEY_UNKNOWN;
              if (shift && ctrl) return KEY_SHIFT_CTRL_END;
              if (shift) return KEY_SHIFT_END;
              if (ctrl) return KEY_CTRL_END;
              return KEY_END;
            case 0x35:
              if (getchar() != 0x7E) return KEY_UNKNOWN;
              if (shift) return KEY_SHIFT_PGUP;
              return KEY_PGUP;
            case 0x36:
              if (getchar() != 0x7E) return KEY_UNKNOWN;
              if (shift) return KEY_SHIFT_PGDN;
              return KEY_PGDN;
            case 0x41: 
              if (shift && ctrl) return KEY_SHIFT_CTRL_UP;
              if (shift) return KEY_SHIFT_UP;
              if (ctrl) return KEY_CTRL_UP;
              return KEY_UP;
            case 0x42: 
              if (shift && ctrl) return KEY_SHIFT_CTRL_DOWN;
              if (shift) return KEY_SHIFT_DOWN;
              if (ctrl) return KEY_CTRL_DOWN;
              return KEY_DOWN;
            case 0x43: 
              if (shift && ctrl) return KEY_SHIFT_CTRL_RIGHT;
              if (shift) return KEY_SHIFT_RIGHT;
              if (ctrl) return KEY_CTRL_RIGHT;
              return KEY_RIGHT;
            case 0x44:
              if (shift && ctrl) return KEY_SHIFT_CTRL_LEFT;
              if (shift) return KEY_SHIFT_LEFT;
              if (ctrl) return KEY_CTRL_LEFT;
              return KEY_LEFT;
            case 0x46:
              if (shift && ctrl) return KEY_SHIFT_CTRL_END;
              if (shift) return KEY_SHIFT_END;
              if (ctrl) return KEY_CTRL_END;
              return KEY_END;
            case 0x48:
              if (shift && ctrl) return KEY_SHIFT_CTRL_HOME;
              if (shift) return KEY_SHIFT_HOME;
              if (ctrl) return KEY_CTRL_HOME;
              return KEY_HOME;
            case 0x5A: 
              return KEY_SHIFT_TAB;
            case 0x5B:
              ch = getchar();
              switch (ch) {
                case 0x41: return KEY_F1;
                case 0x43: return KEY_F3;
                case 0x45: return KEY_F5;
              }
              return KEY_UNKNOWN;

            default: return KEY_UNKNOWN;
          }
          break;

        default: return KEY_UNKNOWN;
      }
      break;

    case 0x00:
    case 0xE0:
      ch = getchar();
      switch (ch) {
        case 0x0F: return KEY_SHIFT_TAB;
        case 0x3B: return KEY_F1;
        case 0x3D: return KEY_F3;
        case 0x3F: return KEY_F5;
        case 0x47: return KEY_HOME;
        case 0x48: return KEY_UP;
        case 0x49: return KEY_PGUP;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
        case 0x4F: return KEY_END;
        case 0x50: return KEY_DOWN;
        case 0x51: return KEY_PGDN;
        case 0x52: return KEY_INS;
        case 0x53: return KEY_DEL;
        case 0x73: return KEY_CTRL_LEFT;
        case 0x74: return KEY_CTRL_RIGHT;
        case 0x75: return KEY_CTRL_END;
        case 0x77: return KEY_CTRL_HOME;
        case 0x8D: return KEY_CTRL_UP;
        case 0x91: return KEY_CTRL_DOWN;
        case 0x94: return KEY_CTRL_TAB;
        case 0xB8: return KEY_SHIFT_UP;
        case 0xB7: return KEY_SHIFT_HOME;
        case 0xBF: return KEY_SHIFT_END;
        case 0xB9: return KEY_SHIFT_PGUP;
        case 0xBB: return KEY_SHIFT_LEFT;
        case 0xBD: return KEY_SHIFT_RIGHT;
        case 0xC0: return KEY_SHIFT_DOWN;
        case 0xC1: return KEY_SHIFT_PGDN;
        case 0xDB: return KEY_SHIFT_CTRL_LEFT;
        case 0xDD: return KEY_SHIFT_CTRL_RIGHT;
        case 0xD8: return KEY_SHIFT_CTRL_UP;
        case 0xE0: return KEY_SHIFT_CTRL_DOWN;
        case 0xD7: return KEY_SHIFT_CTRL_HOME;
        case 0xDF: return KEY_SHIFT_CTRL_END;

        default: return KEY_UNKNOWN;
      }
      break;

    case 0x7F: return KEY_BACKSPACE;

    default: return ch;
  }
}

int prompt(struct editor *ed, char *msg,int selection) {
  int maxlen, len, ch;
  char *buf = ed->env->linebuf;

  gotoxy(0, ed->env->lines);
  outstr(STATUS_COLOR);
  outstr(msg);
  outstr(CLREOL);

  len = 0;
  maxlen = ed->env->cols - strlen(msg) - 1;
  if (selection) {
    len = get_selected_text(ed, buf, maxlen);
    outbuf(buf, len);
  }
  for (;;) {
    fflush(stdout);
    ch = getkey();
    if (ch == KEY_ESC) {
      return 0;
    } else if (ch == KEY_ENTER) {
      buf[len] = 0;
      return len > 0;
    } else if (ch == KEY_BACKSPACE) {
      if (len > 0) {
        outstr("\b \b");
        len--;
      }
    } else if (ch >= ' ' && ch < 0x100 && len < maxlen) {
      outch(ch);
      buf[len++] = ch;
    }
  }
}

int ask() {
  int ch = getchar();
  return ch == 'y' || ch == 'Y';
}

