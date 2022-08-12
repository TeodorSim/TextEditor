//
//  kilo.c
//  Text Editor
//
//  Created by Simionescu Teodor on 29.07.2022.
//

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>

/*defines*/
/*
  --->terminate key == Q !!!!!
 */
#define CTRL_KEY(k) ((k) & 0x51)
/*data*/

struct editorConfig{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};
struct editorConfig E;
/*terminal*/
void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(s);
    exit(1);
}

void disableRawMode(){
    /*
     -->get the terminal back to normal
     */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    /*
     -->now assure that after at the ending of this program the
        terminal comes back to normal
     */
    atexit(disableRawMode);
    
    /*
     -->takes everithing from terminal
     */
    struct termios raw=E.orig_termios;
    /*
     -->deactivate all the safety macros, makes the terminal 'RAW'
     */
    raw.c_iflag &=~(BRKINT | ICRNL | ISTRIP |IXON);
    raw.c_oflag &=~(OPOST);
    raw.c_cflag &=~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=1;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i=0;
    
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    while(i<sizeof(buf)-1){
        if(read(STDIN_FILENO, &buf[i], 1)!= 1) break;
        if(buf[i]=='R') break;
        i++;
    }
    buf[i]='\0';
    
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;
}

int getWindowSize(int *rows, int *cols){
    struct winsize ws;
    
    if( ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)== -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12)!= 12) return -1;
        editorReadKey();
        return getCursorPosition(rows, cols);
    } else {
        *cols=ws.ws_col;
        *rows=ws.ws_row;
        return 0;
    }
}
/*** output ***/

void editorDrawRows(){
    int y;
    for(y=0; y<E.screenrows; y++){
        write(STDOUT_FILENO, "~", 1);
        
        if(y<E.screenrows-1){
            write(STDOUT_FILENO, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('Q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

void initEditor(){
    if(getWindowSize(&E.screenrows, &E.screencols)==-1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}
