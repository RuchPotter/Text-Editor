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

/* Functions used to perform operations on the string buffer in the currently open editor window ( the editor itself being a struct as well) */

void clear_undo(struct editor *ed) {
  struct undo *undo = ed->undohead;
  while (undo) {
    struct undo *next = undo->next;
    free(undo->undobuf);
    free(undo->redobuf);
    free(undo);
    undo = next;
  }
  ed->undohead = ed->undotail = ed->undo = NULL;
}

void reset_undo(struct editor *ed) {
  while (ed->undotail != ed->undo) {
    struct undo *undo = ed->undotail;
    if (!undo) {
      ed->undohead = NULL;
      ed->undotail = NULL;
      break;
    }
    ed->undotail = undo->prev;
    if (undo->prev) undo->prev->next = NULL;
    free(undo->undobuf);
    free(undo->redobuf);
    free(undo);
  }
  ed->undo = ed->undotail;
}

struct editor *create_editor(struct env *env) {
  struct editor *ed = (struct editor *) malloc(sizeof(struct editor));
  memset(ed, 0, sizeof(struct editor));
  if (env->current) {
    ed->next = env->current->next;
    ed->prev = env->current;
    env->current->next->prev = ed;
    env->current->next = ed;
  } else {
    ed->next = ed->prev = ed;
  }
  ed->env = env;
  env->current = ed;
  return ed;
}

void delete_editor(struct editor *ed) {
  if (ed->next == ed) {
    ed->env->current = NULL;
  } else {
    ed->env->current = ed->prev;
  }
  ed->next->prev = ed->prev;
  ed->prev->next = ed->next;
  if (ed->start) free(ed->start);
  clear_undo(ed);
  free(ed);
}

struct editor *find_editor(struct env *env, char *filename) {
  char fn[FILENAME_MAX];
  struct editor *ed = env->current;
  struct editor *start = ed;
  
  if (!realpath(filename, fn)) strcpy(fn, filename);

  do {
    if (strcmp(fn, ed->filename) == 0) return ed;
    ed = ed->next;
  } while (ed != start);
  return NULL;  
}

int new_file(struct editor *ed, char *filename) {
  if (*filename) {
    strcpy(ed->filename, filename);
  } else {
    sprintf(ed->filename, "Untitled-%d", ++ed->env->untitled);
    ed->newfile = 1;
  }
  ed->permissions = 0644;

  ed->start = (unsigned char *) malloc(MINEXTEND);
  if (!ed->start) return -1;
#ifdef DEBUG
  memset(ed->start, 0, MINEXTEND);
#endif

  ed->gap = ed->start;
  ed->rest = ed->end = ed->gap + MINEXTEND;
  ed->anchor = -1;
  
  return 0;
}

int load_file(struct editor *ed, char *filename) {
  struct stat statbuf;
  int length;
  int f;

  if (!realpath(filename, ed->filename)) return -1;
  f = open(ed->filename, O_RDONLY | O_BINARY);
  if (f < 0) return -1;

  if (fstat(f, &statbuf) < 0) goto err;
  length = statbuf.st_size;
  ed->permissions = statbuf.st_mode & 0777;

  ed->start = (unsigned char *) malloc(length + MINEXTEND);
  if (!ed->start) goto err;
#ifdef DEBUG
  memset(ed->start, 0, length + MINEXTEND);
#endif
  if (read(f, ed->start, length) != length) goto err;

  ed->gap = ed->start + length;
  ed->rest = ed->end = ed->gap + MINEXTEND;
  ed->anchor = -1;

  close(f);
  return 0;

err:
  close(f);
  if (ed->start) {
    free(ed->start);
    ed->start = NULL;
  }
  return -1;
}

int save_file(struct editor *ed) {
  int f;

  f = open(ed->filename, O_CREAT | O_TRUNC | O_WRONLY, ed->permissions);
  if (f < 0) return -1;

  if (write(f, ed->start, ed->gap - ed->start) != ed->gap - ed->start) goto err;
  if (write(f, ed->rest, ed->end - ed->rest) != ed->end - ed->rest) goto err;

  close(f);
  ed->dirty = 0;
  clear_undo(ed);
  return 0;

err:
  close(f);
  return -1;
}

int text_length(struct editor *ed) {
  return (ed->gap - ed->start) + (ed->end - ed->rest);
}

unsigned char *text_ptr(struct editor *ed, int pos) {
  unsigned char *p = ed->start + pos;
  if (p >= ed->gap) p += (ed->rest - ed->gap);
  return p;
}

void move_gap(struct editor *ed, int pos, int minsize) {
  int gapsize = ed->rest - ed->gap;
  unsigned char *p = text_ptr(ed, pos);
  if (minsize < 0) minsize = 0;

  if (minsize <= gapsize) {
    if (p != ed->rest) {
      if (p < ed->gap) {
	memmove(p + gapsize, p, ed->gap - p);
      } else {
	memmove(ed->gap, ed->rest, p - ed->rest);
      }
      ed->gap = ed->start + pos;
      ed->rest = ed->gap + gapsize;
    }
  } else {
    int newsize;
    unsigned char *start;
    unsigned char *gap;
    unsigned char *rest;
    unsigned char *end;

    if (gapsize + MINEXTEND > minsize) minsize = gapsize + MINEXTEND;
    newsize = (ed->end - ed->start) - gapsize + minsize;
    start = (unsigned char *) malloc(newsize); 
    gap = start + pos;
    rest = gap + minsize;
    end = start + newsize;

    if (p < ed->gap) {
      memcpy(start, ed->start, pos);
      memcpy(rest, p, ed->gap - p);
      memcpy(end - (ed->end - ed->rest), ed->rest, ed->end - ed->rest);
    } else {
      memcpy(start, ed->start, ed->gap - ed->start);
      memcpy(start + (ed->gap - ed->start), ed->rest, p - ed->rest);
      memcpy(rest, p, ed->end - p);
    }

    free(ed->start);
    ed->start = start;
    ed->gap = gap;
    ed->rest = rest;
    ed->end = end;
  }

#ifdef DEBUG
  memset(ed->gap, 0, ed->rest - ed->gap);
#endif
}

void close_gap(struct editor *ed) {
  int len = text_length(ed);
  move_gap(ed, len, 1);
  ed->start[len] = 0;
}

int get(struct editor *ed, int pos) {
  unsigned char *p = text_ptr(ed, pos);
  if (p >= ed->end) return -1;
  return *p;
}

int compare(struct editor *ed, unsigned char *buf, int pos, int len) {
  unsigned char *bufptr = buf;
  unsigned char *p = ed->start + pos;
  if (p >= ed->gap) p += (ed->rest - ed->gap);

  while (len > 0) {
    if (p == ed->end) return 0;
    if (*bufptr++ != *p) return 0;
    len--;
    if (++p == ed->gap) p = ed->rest;
  }

  return 1;
}

int copy(struct editor *ed, unsigned char *buf, int pos, int len) {
  unsigned char *bufptr = buf;
  unsigned char *p = ed->start + pos;
  if (p >= ed->gap) p += (ed->rest - ed->gap);

  while (len > 0) {
    if (p == ed->end) break;
    *bufptr++ = *p;
    len--;
    if (++p == ed->gap) p = ed->rest;
  }

  return bufptr - buf;
}

void replace(struct editor *ed, int pos, int len, unsigned char *buf, int bufsize, int doundo) {
  unsigned char *p;
  struct undo *undo;

  // Store undo information
  if (doundo) {
    reset_undo(ed);
    undo = ed->undotail;
    if (undo && len == 0 && bufsize == 1 && undo->erased == 0 && pos == undo->pos + undo->inserted) {
      // Insert character at end of current redo buffer
      undo->redobuf = realloc(undo->redobuf, undo->inserted + 1);
      undo->redobuf[undo->inserted] = *buf;
      undo->inserted++;
    } else if (undo && len == 1 && bufsize == 0 && undo->inserted == 0 && pos == undo->pos) {
      // Erase character at end of current undo buffer
      undo->undobuf = realloc(undo->undobuf, undo->erased + 1);
      undo->undobuf[undo->erased] = get(ed, pos);
      undo->erased++;
    } else if (undo && len == 1 && bufsize == 0 && undo->inserted == 0 && pos == undo->pos - 1) {
      // Erase character at beginning of current undo buffer
      undo->pos--;
      undo->undobuf = realloc(undo->undobuf, undo->erased + 1);
      memmove(undo->undobuf + 1, undo->undobuf, undo->erased);
      undo->undobuf[0] = get(ed, pos);
      undo->erased++;
    } else {
      // Create new undo buffer
      undo = (struct undo *) malloc(sizeof(struct undo));
      if (ed->undotail) ed->undotail->next = undo;
      undo->prev = ed->undotail;
      undo->next = NULL;
      ed->undotail = ed->undo = undo;
      if (!ed->undohead) ed->undohead = undo;

      undo->pos = pos;
      undo->erased = len;
      undo->inserted = bufsize;
      undo->undobuf = undo->redobuf = NULL;
      if (len > 0) {
	undo->undobuf = malloc(len);
	copy(ed, undo->undobuf, pos, len);      
      }
      if (bufsize > 0) {
	undo->redobuf = malloc(bufsize);
	memcpy(undo->redobuf, buf, bufsize);
      }
    }
  }

  p = ed->start + pos;
  if (bufsize == 0 && p <= ed->gap && p + len >= ed->gap) {
    // Handle deletions at the edges of the gap
    ed->rest += len - (ed->gap - p);
    ed->gap = p;
  } else {
    // Move the gap
    move_gap(ed, pos + len, bufsize - len);

    // Replace contents
    memcpy(ed->start + pos, buf, bufsize);
    ed->gap = ed->start + pos + bufsize;
  }

  // Mark buffer as dirty
  ed->dirty = 1;
}

void insert(struct editor *ed, int pos, unsigned char *buf, int bufsize) {
  replace(ed, pos, 0, buf, bufsize, 1);
}

void erase(struct editor *ed, int pos, int len) {
  replace(ed, pos, len, NULL, 0, 1);
}

/* Functions that enable navigation through the editor window in the terminal */

int line_length(struct editor *ed, int linepos) {
  int pos = linepos;
  while (1) {
    int ch = get(ed, pos);
    if (ch < 0 || ch == '\n' || ch == '\r') break;
    pos++;
  }

  return pos - linepos;
}

int line_start(struct editor *ed, int pos) {
  while (1) {
    if (pos == 0) break;
    if (get(ed, pos - 1) == '\n') break;
    pos--;
  }

  return pos;
}

int next_line(struct editor *ed, int pos) {
  while (1) {
    int ch = get(ed, pos);
    if (ch < 0) return -1;
    pos++;
    if (ch == '\n') return pos;
  }
}

int prev_line(struct editor *ed, int pos) {
  if (pos == 0) return -1;

  while (pos > 0) {
    int ch = get(ed, --pos);
    if (ch == '\n') break;
  }

  while (pos > 0) {
    int ch = get(ed, --pos);
    if (ch == '\n') return pos + 1;
  }

  return 0;
}

int column(struct editor *ed, int linepos, int col) {
  unsigned char *p = text_ptr(ed, linepos);
  int c = 0;
  while (col > 0) {
    if (p == ed->end) break;
    if (*p == '\t') {
      int spaces = TABSIZE - c % TABSIZE;
      c += spaces;
    } else {
      c++;
    }
    col--;
    if (++p == ed->gap) p = ed->rest;
  }
  return c;
}

void moveto(struct editor *ed, int pos, int center) {
  int scroll = 0;
  for (;;) {
    int cur = ed->linepos + ed->col;
    if (pos < cur) {
      if (pos >= ed->linepos) {
	ed->col = pos - ed->linepos;
      } else {
	ed->col = 0;
	ed->linepos = prev_line(ed, ed->linepos);
	ed->line--;

	if (ed->topline > ed->line) {
	  ed->toppos = ed->linepos;
	  ed->topline--;
	  ed->refresh = 1;
	  scroll = 1;
	}
      }
    } else if (pos > cur) {
      int next = next_line(ed, ed->linepos);
      if (next == -1) {
	ed->col = line_length(ed, ed->linepos);
	break;
      } else if (pos < next) {
	ed->col = pos - ed->linepos;
      } else {
	ed->col = 0;
	ed->linepos = next;
	ed->line++;

	if (ed->line >= ed->topline + ed->env->lines) {
	  ed->toppos = next_line(ed, ed->toppos);
	  ed->topline++;
	  ed->refresh = 1;
	  scroll = 1;
	}
      }
    } else {
      break;
    }
  }

  if (scroll && center) {
    int tl = ed->line - ed->env->lines / 2;
    if (tl < 0) tl = 0;
    for (;;) {
      if (ed->topline > tl) {
	ed->toppos = prev_line(ed, ed->toppos);
	ed->topline--;
      } else if (ed->topline < tl) {
	ed->toppos = next_line(ed, ed->toppos);
	ed->topline++;
      } else {
	break;
      }
    }
  }
}

/* Functions to deal with the selection of text, for use in the later undo, redo, movement functions */

int get_selection(struct editor *ed, int *start, int *end) {
  if (ed->anchor == -1) {
    *start = *end = -1;
    return 0;
  } else {
    int pos = ed->linepos + ed->col;
    if (pos == ed->anchor) {
      *start = *end = -1;
      return 0;
    } else if (pos < ed->anchor) {
      *start = pos;
      *end = ed->anchor;
    } else {
      *start = ed->anchor;
      *end = pos;
    }
  }
  return 1;
}

int get_selected_text(struct editor *ed, char *buffer, int size) {
  int selstart, selend, len;

  if (!get_selection(ed, &selstart, &selend)) return 0;
  len = selend - selstart;
  if (len >= size) return 0;
  copy(ed, buffer, selstart, len);
  buffer[len] = 0;
  return len;
}

void update_selection(struct editor *ed, int select) {
  if (select) {
    if (ed->anchor == -1) ed->anchor = ed->linepos + ed->col;
    ed->refresh = 1;
  } else {
    if (ed->anchor != -1) ed->refresh = 1;
    ed->anchor = -1;
  }
}

int erase_selection(struct editor *ed) {
  int selstart, selend;
  
  if (!get_selection(ed, &selstart, &selend)) return 0;
  moveto(ed, selstart, 0);
  erase(ed, selstart, selend - selstart);
  ed->anchor = -1;
  ed->refresh = 1;
  return 1;
}

void select_all(struct editor *ed) {
  ed->anchor = 0;
  ed->refresh = 1;
  moveto(ed, text_length(ed), 0);
}


/* Functions for manipulating the size of the console window, outputs, and movement of the cursor */

void get_console_size(struct env *env) {
#ifdef __linux__
  struct winsize ws;

  ioctl(0, TIOCGWINSZ, &ws);
  env->cols = ws.ws_col;
  env->lines = ws.ws_row - 1;
#else
  struct term *term = gettib()->proc->term;
  env->cols = term->cols;
  env->lines = term->lines - 1;
#endif
  env->linebuf = realloc(env->linebuf, env->cols + LINEBUF_EXTRA);
}

void outch(char c) {
  putchar(c);
}

void outbuf(unsigned char *buf, int len) {
  fwrite(buf, 1, len, stdout);
}

void outstr(char *str) {
  fputs(str, stdout);
}

void clear_screen() {
  outstr(CLRSCR);
}

void gotoxy(int col, int line) {
  char buf[32];

  sprintf(buf, GOTOXY, line + 1, col + 1);
  outstr(buf);
}


/* Functions for display on the terminal window of messages, alerts, prompts, etc. */

void display_message(struct editor *ed, char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  gotoxy(0, ed->env->lines);
  outstr(STATUS_COLOR);
  vprintf(fmt, args);
  outstr(CLREOL TEXT_COLOR);
  fflush(stdout);
  va_end(args);
}

void draw_full_statusline(struct editor *ed) {
  struct env *env = ed->env;
  int namewidth = env->cols - 28;
  gotoxy(0, env->lines);
  sprintf(env->linebuf, STATUS_COLOR "%*.*sF1=Help %c Ln %-6dCol %-4d" CLREOL TEXT_COLOR, -namewidth, namewidth, ed->filename, ed->dirty ? '*' : ' ', ed->line + 1, column(ed, ed->linepos, ed->col) + 1);
  outstr(env->linebuf);
}

void draw_statusline(struct editor *ed) {
  gotoxy(ed->env->cols - 20, ed->env->lines);
  sprintf(ed->env->linebuf, STATUS_COLOR "%c Ln %-6dCol %-4d" CLREOL TEXT_COLOR, ed->dirty ? '*' : ' ', ed->line + 1, column(ed, ed->linepos, ed->col) + 1);
  outstr(ed->env->linebuf);
}

void display_line(struct editor *ed, int pos, int fullline) {
  int hilite = 0;
  int col = 0;
  int margin = ed->margin;
  int maxcol = ed->env->cols + margin;
  unsigned char *bufptr = ed->env->linebuf;
  unsigned char *p = text_ptr(ed, pos);
  int selstart, selend, ch;
  char *s;

  
  while (col < maxcol) {
    if (margin == 0) {
      if (!hilite && pos >= selstart && pos < selend) {
	for (s = SELECT_COLOR; *s; s++) *bufptr++ = *s;
	hilite = 1;
      } else if (hilite && pos >= selend) {
	for (s = TEXT_COLOR; *s; s++) *bufptr++ = *s;
	hilite = 0;
      }
    }

    if (p == ed->end) break;
    ch = *p;
    if (ch == '\r' || ch == '\n') break;

    if (ch == '\t') {
      int spaces = TABSIZE - col % TABSIZE;
      while (spaces > 0 && col < maxcol) {
	if (margin > 0) {
	  margin--;
	} else {
	  *bufptr++ = ' ';
	}
	col++;
	spaces--;
      }
    } else {
      if (margin > 0) {
	margin--;
      } else {
	*bufptr++ = ch;
      }
      col++;
    }

    if (++p == ed->gap) p = ed->rest;
    pos++;
  }

#ifdef __linux__
  if (hilite) {
    while (col < maxcol) {
      *bufptr++ = ' ';
      col++;
    }
  } else {
    if (col == margin) *bufptr++ = ' ';
  }
#endif

  if (col < maxcol) {
    for (s = CLREOL; *s; s++) *bufptr++ = *s;
    if (fullline) {
      memcpy(bufptr, "\r\n", 2);
      bufptr += 2;
    }
  }

  if (hilite) {
    for (s = TEXT_COLOR; *s; s++) *bufptr++ = *s;
  }

  outbuf(ed->env->linebuf, bufptr - ed->env->linebuf);
}

void update_line(struct editor *ed) {
  gotoxy(0, ed->line - ed->topline);
  display_line(ed, ed->linepos, 0);
}

void draw_screen(struct editor *ed) {
  int pos;
  int i;

  gotoxy(0, 0);
  outstr(TEXT_COLOR);
  pos = ed->toppos;
  for (i = 0; i < ed->env->lines; i++) {
    if (pos < 0) {
      outstr(CLREOL "\r\n");
    } else {
      display_line(ed, pos, 1);
      pos = next_line(ed, pos);
    }
  }
}

void position_cursor(struct editor *ed) {
  int col = column(ed, ed->linepos, ed->col);
  gotoxy(col - ed->margin, ed->line - ed->topline);
}

/* Functions that control the movement of the terminal cursor when keyboard commands are inputted */

void adjust(struct editor *ed) {
  int col;
  int ll = line_length(ed, ed->linepos);
  ed->col = ed->lastcol;
  if (ed->col > ll) ed->col = ll;

  col = column(ed, ed->linepos, ed->col);
  while (col < ed->margin) {
    ed->margin -= 4;
    if (ed->margin < 0) ed->margin = 0;
    ed->refresh = 1;
  }

  while (col - ed->margin >= ed->env->cols) {
    ed->margin += 4;
    ed->refresh = 1;
  }
}

void up(struct editor *ed, int select) {
  int newpos;
  
  update_selection(ed, select);

  newpos = prev_line(ed, ed->linepos);
  if (newpos < 0) return;

  ed->linepos = newpos;
  ed->line--;
  if (ed->line < ed->topline) {
    ed->toppos = ed->linepos;
    ed->topline = ed->line;
    ed->refresh = 1;
  }

  adjust(ed);
}

void down(struct editor *ed, int select) {
  int newpos;
  
  update_selection(ed, select);

  newpos = next_line(ed, ed->linepos);
  if (newpos < 0) return;

  ed->linepos = newpos;
  ed->line++;

  if (ed->line >= ed->topline + ed->env->lines) {
    ed->toppos = next_line(ed, ed->toppos);
    ed->topline++;
    ed->refresh = 1;
  }

  adjust(ed);
}

void left(struct editor *ed, int select) {
  update_selection(ed, select);
  if (ed->col > 0) {
    ed->col--;
  } else {
    int newpos = prev_line(ed, ed->linepos);
    if (newpos < 0) return;

    ed->col = line_length(ed, newpos);
    ed->linepos = newpos;
    ed->line--;
    if (ed->line < ed->topline) {
      ed->toppos = ed->linepos;
      ed->topline = ed->line;
      ed->refresh = 1;
    }
  }

  ed->lastcol = ed->col;
  adjust(ed);
}

void right(struct editor *ed, int select) {
  update_selection(ed, select);
  if (ed->col < line_length(ed, ed->linepos)) {
    ed->col++;
  } else {
    int newpos = next_line(ed, ed->linepos);
    if (newpos < 0) return;

    ed->col = 0;
    ed->linepos = newpos;
    ed->line++;

    if (ed->line >= ed->topline + ed->env->lines) {
      ed->toppos = next_line(ed, ed->toppos);
      ed->topline++;
      ed->refresh = 1;
    }
  }

  ed->lastcol = ed->col;
  adjust(ed);
}

int wordchar(int ch) {
  return ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || ch >= '0' && ch <= '9';
}

void wordleft(struct editor *ed, int select) {
  int pos, phase;
  
  update_selection(ed, select);
  pos = ed->linepos + ed->col;
  phase = 0;
  while (pos > 0) {
    int ch = get(ed, pos - 1);
    if (phase == 0) {
      if (wordchar(ch)) phase = 1;
    } else {
      if (!wordchar(ch)) break;
    }

    pos--;
    if (pos < ed->linepos) {
      ed->linepos = prev_line(ed, ed->linepos);
      ed->line--;
      ed->refresh = 1;
    }
  }
  ed->col = pos - ed->linepos;
  if (ed->line < ed->topline) {
    ed->toppos = ed->linepos;
    ed->topline = ed->line;
  }

  ed->lastcol = ed->col;
  adjust(ed);
}

void wordright(struct editor *ed, int select) {
  int pos, end, phase, next;
  
  update_selection(ed, select);
  pos = ed->linepos + ed->col;
  end = text_length(ed);
  next = next_line(ed, ed->linepos);
  phase = 0;
  while (pos < end) {
    int ch = get(ed, pos);
    if (phase == 0) {
      if (wordchar(ch)) phase = 1;
    } else {
      if (!wordchar(ch)) break;
    }

    pos++;
    if (pos == next) {
      ed->linepos = next;
      next = next_line(ed, ed->linepos);
      ed->line++;
      ed->refresh = 1;
    }
  }
  ed->col = pos - ed->linepos;
  if (ed->line >= ed->topline + ed->env->lines) {
    ed->toppos = next_line(ed, ed->toppos);
    ed->topline++;
  }

  ed->lastcol = ed->col;
  adjust(ed);
}

void home(struct editor *ed, int select) {
  update_selection(ed, select);
  ed->col = ed->lastcol = 0;
  adjust(ed);
}

void end(struct editor *ed, int select) {
  update_selection(ed, select);
  ed->col = ed->lastcol = line_length(ed, ed->linepos);
  adjust(ed);
}

void top(struct editor *ed, int select) {
  update_selection(ed, select);
  ed->toppos = ed->topline = ed->margin = 0;
  ed->linepos = ed->line = ed->col = ed->lastcol = 0;
  ed->refresh = 1;
}

void bottom(struct editor *ed, int select) {
  update_selection(ed, select);
  for (;;) {
    int newpos = next_line(ed, ed->linepos);
    if (newpos < 0) break;

    ed->linepos = newpos;
    ed->line++;

    if (ed->line >= ed->topline + ed->env->lines) {
      ed->toppos = next_line(ed, ed->toppos);
      ed->topline++;
      ed->refresh = 1;
    }
  }
  ed->col = ed->lastcol = line_length(ed, ed->linepos);
  adjust(ed);
}

void pageup(struct editor *ed, int select) {
  int i;

  update_selection(ed, select);
  if (ed->line < ed->env->lines) {
    ed->linepos = ed->toppos = 0;
    ed->line = ed->topline = 0;
  } else {
    for (i = 0; i < ed->env->lines; i++) {
      int newpos = prev_line(ed, ed->linepos);
      if (newpos < 0) return;

      ed->linepos = newpos;
      ed->line--;

      if (ed->topline > 0) {
	ed->toppos = prev_line(ed, ed->toppos);
	ed->topline--;
      }
    }
  }

  ed->refresh = 1;
  adjust(ed);
}

void pagedown(struct editor *ed, int select) {
  int i;

  update_selection(ed, select);
  for (i = 0; i < ed->env->lines; i++) {
    int newpos = next_line(ed, ed->linepos);
    if (newpos < 0) break;

    ed->linepos = newpos;
    ed->line++;

    ed->toppos = next_line(ed, ed->toppos);
    ed->topline++;
  }

  ed->refresh = 1;
  adjust(ed);
}

/* Functions that control the actual text editing proces. Functions for insertion of characters, backspace deletion, indenting, etc, are provide */

void insert_char(struct editor *ed, unsigned char ch) {
  erase_selection(ed);
  insert(ed, ed->linepos + ed->col, &ch, 1);
  ed->col++;
  ed->lastcol = ed->col;
  adjust(ed);
  if (!ed->refresh) ed->lineupdate = 1;
}

void newline(struct editor *ed) {
  int p;
  unsigned char ch;

  erase_selection(ed);
#ifdef __linux__
  insert(ed, ed->linepos + ed->col, "\n", 1);
#else
  insert(ed, ed->linepos + ed->col, "\r\n", 2);
#endif
  ed->col = ed->lastcol = 0;
  ed->line++;
  p = ed->linepos;
  ed->linepos = next_line(ed, ed->linepos);
  for (;;) {
    ch = get(ed, p++);
    if (ch == ' ' || ch == '\t') {
      insert(ed, ed->linepos + ed->col, &ch, 1);
      ed->col++;
    } else {
      break;
    }
  }
  ed->lastcol = ed->col;
  
  ed->refresh = 1;

  if (ed->line >= ed->topline + ed->env->lines) {
    ed->toppos = next_line(ed, ed->toppos);
    ed->topline++;
    ed->refresh = 1;
  }

  adjust(ed);
}

void backspace(struct editor *ed) {
  if (erase_selection(ed)) return;
  if (ed->linepos + ed->col == 0) return;
  if (ed->col == 0) {
    int pos = ed->linepos;
    erase(ed, --pos, 1);
    if (get(ed, pos - 1) == '\r') erase(ed, --pos, 1);

    ed->line--;
    ed->linepos = line_start(ed, pos);
    ed->col = pos - ed->linepos;
    ed->refresh = 1;

    if (ed->line < ed->topline) {
      ed->toppos = ed->linepos;
      ed->topline = ed->line;
    }
  } else {
    ed->col--;
    erase(ed, ed->linepos + ed->col, 1);
    ed->lineupdate = 1;
  }

  ed->lastcol = ed->col;
  adjust(ed);
}

void del(struct editor *ed) {
  int pos, ch;
  
  if (erase_selection(ed)) return;
  pos = ed->linepos + ed->col;
  ch = get(ed, pos);
  if (ch < 0) return;

  erase(ed, pos, 1);
  if (ch == '\r') {
    ch = get(ed, pos);
    if (ch == '\n') erase(ed, pos, 1);
  }

  if (ch == '\n') {
    ed->refresh = 1;
  } else {
    ed->lineupdate = 1;
  }
}

void indent(struct editor *ed, unsigned char *indentation) {
  int start, end, i, lines, toplines, newline, ch;
  unsigned char *buffer, *p;
  int buflen;
  int width = strlen(indentation);
  int pos = ed->linepos + ed->col;

  if (!get_selection(ed, &start, &end)) {
    insert_char(ed, '\t');
    return;
  }

  lines = 0;
  toplines = 0;
  newline = 1;
  for (i = start; i < end; i++) {
    if (i == ed->toppos) toplines = lines;
    if (newline) {
      lines++;
      newline = 0;
    }
    if (get(ed, i) == '\n') newline = 1;
  }
  buflen = end - start + lines * width;
  buffer = malloc(buflen);
  if (!buffer) return;

  newline = 1;
  p = buffer;
  for (i = start; i < end; i++) {
    if (newline) {
      memcpy(p, indentation, width);
      p += width;
      newline = 0;
    }
    ch = get(ed, i);
    *p++ = ch;
    if (ch == '\n') newline = 1;
  }

  replace(ed, start, end - start, buffer, buflen, 1);
  free(buffer);  

  if (ed->anchor < pos) {
    pos += width * lines;
  } else {
    ed->anchor += width * lines;
  }

  ed->toppos += width * toplines;
  ed->linepos = line_start(ed, pos);
  ed->col = ed->lastcol = pos - ed->linepos;

  adjust(ed);
  ed->refresh = 1;
}


void undo(struct editor *ed) {
  if (!ed->undo) return;
  moveto(ed, ed->undo->pos, 0);
  replace(ed, ed->undo->pos, ed->undo->inserted, ed->undo->undobuf, ed->undo->erased, 0);
  ed->undo = ed->undo->prev;
  if (!ed->undo) ed->dirty = 0;
  ed->anchor = -1;
  ed->lastcol = ed->col;
  ed->refresh = 1;
}

void redo(struct editor *ed) {
  if (ed->undo) {
    if (!ed->undo->next) return;
    ed->undo = ed->undo->next;
  } else {
    if (!ed->undohead) return;
    ed->undo = ed->undohead;
  }
  replace(ed, ed->undo->pos, ed->undo->erased, ed->undo->redobuf, ed->undo->inserted, 0);
  moveto(ed, ed->undo->pos, 0);
  ed->dirty = 1;
  ed->anchor = -1;
  ed->lastcol = ed->col;
  ed->refresh = 1;
}

/* Functions that govern the opening of new editor windows and opening/saving files via the editor */

void open_editor(struct editor *ed) {
  int rc;
  char *filename;
  struct env *env = ed->env;

  if (!prompt(ed, "Open file: ", 1)) {
    ed->refresh = 1;
    return;
  }
  filename = ed->env->linebuf;
  
  ed = find_editor(ed->env, filename);
  if (ed) {
    env->current = ed;
  } else {
    ed = create_editor(env);
    rc = load_file(ed, filename);
    if (rc < 0) {
      display_message(ed, "Error %d opening %s (%s)", errno, filename, strerror(errno));
      sleep(5);
      delete_editor(ed);
      ed = env->current;
    }
  }
  ed->refresh = 1;
}

void new_editor(struct editor *ed) {
  ed = create_editor(ed->env);
  new_file(ed, "");
  ed->refresh = 1;
}

void read_from_stdin(struct editor *ed) {
  char buffer[512];
  int n, pos;

  pos = 0;
  while ((n = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
    insert(ed, pos, buffer, n);
    pos += n;
  }
  strcpy(ed->filename, "<stdin>");  
  ed->newfile = 1;
  ed->dirty = 0;
}

void save_editor(struct editor *ed) {
  int rc;
  
  if (!ed->dirty && !ed->newfile) return;
  
  if (ed->newfile) {
    if (!prompt(ed, "Save as: ", 1)) {
      ed->refresh = 1;
      return;
    }

    if (access(ed->env->linebuf, F_OK) == 0) {
      display_message(ed, "Overwrite %s (y/n)? ", ed->env->linebuf);
      if (!ask()) {
	ed->refresh = 1;
	return;
      }
    }
    strcpy(ed->filename, ed->env->linebuf);
    ed->newfile = 0;
  }

  rc = save_file(ed);
  if (rc < 0) {
    display_message(ed, "Error %d saving document (%s)", errno, strerror(errno));
    sleep(5);
  }

  ed->refresh = 1;
}

void close_editor(struct editor *ed) {
  struct env *env = ed->env;
  
  if (ed->dirty) {
    display_message(ed, "Close %s without saving changes (y/n)? ", ed->filename);
    if (!ask()) {
      ed->refresh = 1;
      return;
    }
  }
  
  delete_editor(ed);

  ed = env->current;
  if (!ed) {
    ed = create_editor(env);
    new_file(ed, "");
  }
  ed->refresh = 1;
}

void pipe_command(struct editor *ed) {
  FILE *f;
  char buffer[512];
  int n;
  int pos;

  if (!prompt(ed, "Command: ", 1)) {
    ed->refresh = 1;
    return;
  }

  f = popen(ed->env->linebuf, "r");

  if (!f) {
    display_message(ed, "Error %d running command (%s)", errno, strerror(errno));
    sleep(5);
  } else {
    erase_selection(ed);
    pos = ed->linepos + ed->col;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
      insert(ed, pos, buffer, n);
      pos += n;
    }
    moveto(ed, pos, 0);
    pclose(f);
  }
  ed->refresh = 1;
}

void find_text(struct editor *ed, int next) {
  int slen;

  if (!next) {
    if (!prompt(ed, "Find: ", 1)) {
      ed->refresh = 1;
      return;
    }    
    if (ed->env->search) free(ed->env->search);
    ed->env->search = strdup(ed->env->linebuf);
  }

  if (!ed->env->search) return;
  slen = strlen(ed->env->search);
  if (slen > 0) {
    unsigned char *match;
    
    close_gap(ed);
    match = strstr(ed->start + ed->linepos + ed->col, ed->env->search);
    if (match != NULL) {
      int pos = match - ed->start;
      ed->anchor = pos;
      moveto(ed, pos + slen, 1);
    } else {
      outch('\007');
    }
  }
  ed->refresh = 1;
}

void goto_line(struct editor *ed) {
  int lineno, l, pos;

  ed->anchor = -1;
  if (prompt(ed, "Goto line: ", 1)) {
    lineno = atoi(ed->env->linebuf);
    if (lineno > 0) {
      pos = 0;
      for (l = 0; l < lineno - 1; l++) {
	pos = next_line(ed, pos);
	if (pos < 0) break;
      }
    } else {
      pos = -1;
    }

    if (pos >= 0) {
      moveto(ed, pos, 1);
    } else {
      outch('\007');
    }
  }
  ed->refresh = 1;
}

struct editor *next_file(struct editor *ed) {
  ed = ed->env->current = ed->next;
  ed->refresh = 1;
  return ed;
}

void jump_to_editor(struct editor *ed) {
  struct env *env = ed->env;
  char filename[FILENAME_MAX];
  int lineno = 0;

  if (!get_selected_text(ed, filename, FILENAME_MAX)) {
    int pos = ed->linepos + ed->col;
    char *p = filename;
    int left = FILENAME_MAX - 1;
    while (left > 0) {
      int ch = get(ed, pos);
      if (ch < 0) break;
      if (strchr("!@\"'#%&()[]{}*?+:;\r\n\t ", ch)) break;
      *p++ = ch;
      left--;
      pos++;
    }
    *p = 0;

    if (get(ed, pos) == ':') {
      pos++;
      for (;;) {
	int ch = get(ed, pos);
	if (ch < 0) break;
	if (ch >= '0' && ch <= '9') {
	  lineno = lineno * 10 + (ch - '0');
	} else {
	  break;
	}
	pos++;
      }
    }
  }
  if (!*filename) return;
  
  ed = find_editor(env, filename);
  if (ed) {
    env->current = ed;
  } else {
    ed = create_editor(env);
    if (load_file(ed, filename) < 0) {
      outch('\007');
      delete_editor(ed);
      ed = env->current;
    }
  }
  
  if (lineno > 0) {
    int pos = 0;
    while (--lineno > 0) {
      pos = next_line(ed, pos);
      if (pos < 0) break;
    }
    if (pos >= 0) moveto(ed, pos, 1);
  }

  ed->refresh = 1;
}

void redraw_screen(struct editor *ed) {
  get_console_size(ed->env);
  draw_screen(ed);
}

int quit(struct env *env) {
  struct editor *ed = env->current;
  struct editor *start = ed;

  do {
    if (ed->dirty) {
      display_message(ed, "Close %s without saving changes (y/n)? ", ed->filename);
      if (!ask()) return 0;
    }
    ed = ed->next;
  } while (ed != start);

  return 1;
}

/* Help function - provides useful summary of all the operations that can be performed by the editor*/
void help(struct editor *ed) {
  gotoxy(0, 0);
  clear_screen();
  outstr("Mini-Editor v1.0 commands \r\n");
  outstr("======================\r\n\r\n");
  outstr("<up>         Move one line up (*)                          \r\n");
  outstr("<down>       Move one line down (*)                        \r\n");
  outstr("<left>       Move one character left (*)                   \r\n");
  outstr("<right>      Move one character right (*) Ctrl+W  Close file\r\n");
  outstr("<pgup>       Move one page up (*)         Ctrl+Q  Quit\r\n");
  outstr("<pgdn>       Move one page down (*)       Ctrl+P  Pipe command\r\n");
  outstr("Ctrl+<left>  Move to previous word (*)    Ctrl+A  Select all\r\n");
  outstr("Ctrl+Z  Undo                                                \r\n");
  outstr("Ctrl+R  Redo                                                \r\n");
  outstr("<backspace>  Delete previous character    Ctrl+F  Find text\r\n");
  outstr("<delete>     Delete current character     Ctrl+G  Find next\r\n"); 
  outstr("<tab>        Indent selection             Ctrl+L  Goto line\r\n");
  outstr("Ctrl+N  New file                          F1      Help\r\n");
  outstr("Ctrl+O  Open file		            F3      Navigate to file\r\n");
  outstr("Ctrl+S  Save file                         F5      Redraw screen\r\n");
  outstr("\r\n");
  outstr("\r\nPress any key to continue...");
  fflush(stdout);

  getkey();
  draw_screen(ed);
  draw_full_statusline(ed);
}

void edit(struct editor *ed) {
  int done = 0;
  int key;

  ed->refresh = 1;
  while (!done) {
    if (ed->refresh) {
      draw_screen(ed);
      draw_full_statusline(ed);
      ed->refresh = 0;
      ed->lineupdate = 0;
    } else if (ed->lineupdate) {
      update_line(ed);
      ed->lineupdate = 0;
      draw_statusline(ed);
    } else {
      draw_statusline(ed);
    }

    position_cursor(ed);
    fflush(stdout);
    key = getkey();

    if (key >= ' ' && key <= 0x7F) {
#ifdef LESS
      switch (key) {
	case 'q': done = 1; break;
	case '/': find_text(ed, 0); break;
      }
#else
      insert_char(ed, (unsigned char) key);
#endif
    } else {
      switch (key) {
	case KEY_F1: help(ed); break;
	case KEY_F3: jump_to_editor(ed); ed = ed->env->current; break;
	case KEY_F5: redraw_screen(ed); break;
	case ctrl('y'): help(ed); break;
	case ctrl('t'): top(ed, 0); break;
	case ctrl('b'): bottom(ed, 0); break;
	case KEY_UP: up(ed, 0); break;
	case KEY_DOWN: down(ed, 0); break;
	case KEY_LEFT: left(ed, 0); break;
	case KEY_RIGHT: right(ed, 0); break;
	case KEY_HOME: home(ed, 0); break;
	case KEY_END: end(ed, 0); break;
	case KEY_PGUP: pageup(ed, 0); break;
	case KEY_PGDN: pagedown(ed, 0); break;

	case KEY_CTRL_RIGHT: wordright(ed, 0); break;
	case KEY_CTRL_LEFT: wordleft(ed, 0); break;
	case KEY_CTRL_HOME: top(ed, 0); break;
	case KEY_CTRL_END: bottom(ed, 0); break;

	case KEY_SHIFT_UP: up(ed, 1); break;
	case KEY_SHIFT_DOWN: down(ed, 1); break;
	case KEY_SHIFT_LEFT: left(ed, 1); break;
	case KEY_SHIFT_RIGHT: right(ed, 1); break;
	case KEY_SHIFT_PGUP: pageup(ed, 1); break;
	case KEY_SHIFT_PGDN: pagedown(ed, 1); break;
	case KEY_SHIFT_HOME: home(ed, 1); break;
	case KEY_SHIFT_END: end(ed, 1); break;

	case KEY_SHIFT_CTRL_RIGHT: wordright(ed, 1); break;
	case KEY_SHIFT_CTRL_LEFT: wordleft(ed, 1); break;
	case KEY_SHIFT_CTRL_HOME: top(ed, 1); break;
	case KEY_SHIFT_CTRL_END: bottom(ed, 1); break;

	case KEY_CTRL_TAB: ed = next_file(ed); break;

	case ctrl('a'): select_all(ed); break;
	case ctrl('f'): find_text(ed, 0); break;
	case ctrl('l'): goto_line(ed); break;
	case ctrl('g'): find_text(ed, 1); break;
	case ctrl('q'): done = 1; break;
#ifdef LESS
	case KEY_ESC: done = 1; break;
#else
	case KEY_TAB: indent(ed, INDENT); break;
	case KEY_ENTER: newline(ed); break;
	case KEY_BACKSPACE: backspace(ed); break;
	case KEY_DEL: del(ed); break;
	case ctrl('z'): undo(ed); break;
	case ctrl('r'): redo(ed); break;
	case ctrl('o'): open_editor(ed); ed = ed->env->current; break;
	case ctrl('n'): new_editor(ed); ed = ed->env->current; break;
	case ctrl('s'): save_editor(ed); break;
	case ctrl('p'): pipe_command(ed); break;
#endif
	case ctrl('w'): close_editor(ed); ed = ed->env->current; break;
      }
    }
  }
}

//
// main
//

int main(int argc, char *argv[]) {
  struct env env;
  int rc;
  int i;
  sigset_t blocked_sigmask, orig_sigmask;
  struct termios tio;
  struct termios orig_tio;
  memset(&env, 0, sizeof(env));
  for (i = 1; i < argc; i++) {
    struct editor *ed = create_editor(&env);
    rc = load_file(ed, argv[i]);
    if (rc < 0 && errno == ENOENT) rc = new_file(ed, argv[i]);
    if (rc < 0) {
      perror(argv[i]);
      return 0;
    }
  }
  if (env.current == NULL) {
    struct editor *ed = create_editor(&env);
    if (isatty(fileno(stdin))) {
      new_file(ed, "");
    } else {
      read_from_stdin(ed);
    }    
  }
  env.current = env.current->next;
  if (!isatty(fileno(stdin))) {
    if (!freopen("/dev/tty", "r", stdin)) perror("/dev/tty");
  }
  setvbuf(stdout, NULL, 0, 8192);
  tcgetattr(0, &orig_tio);
  cfmakeraw(&tio);  
  tcsetattr(0, TCSANOW, &tio);
  if (getenv("TERM") && strcmp(getenv("TERM"), "linux") == 0) {
    linux_console = 1;
  } else {
    outstr("\033[3 q");  
    outstr("\033]50;CursorShape=2\a"); 
  }
  get_console_size(&env);
  sigemptyset(&blocked_sigmask);
  sigaddset(&blocked_sigmask, SIGINT);
  sigaddset(&blocked_sigmask, SIGTSTP);
  sigaddset(&blocked_sigmask, SIGABRT);
  sigprocmask(SIG_BLOCK, &blocked_sigmask, &orig_sigmask);

  for (;;) {
    if (!env.current) break;
    edit(env.current);
    if (quit(&env)) break;
  }

  gotoxy(0, env.lines + 1);
  outstr(RESET_COLOR CLREOL);
  tcsetattr(0, TCSANOW, &orig_tio);   

  while (env.current) delete_editor(env.current);

  if (env.clipboard) free(env.clipboard);
  if (env.search) free(env.search);
  if (env.linebuf) free(env.linebuf);

  setbuf(stdout, NULL);
  sigprocmask(SIG_SETMASK, &orig_sigmask, NULL);
  return 0;
}
