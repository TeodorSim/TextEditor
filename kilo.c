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
#include <ctype.h>
#include <stdio.h>

/*defines*/
#define CTRL_KEY(k) ((k) & 0x11)
/*data*/
struct termios orig_termios;
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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    /*
     -->now assure that after at the ending of this program the
        terminal comes back to normal
     */
    atexit(disableRawMode);
    
    /*
     -->takes everithing from terminal
     */
    struct termios raw=orig_termios;
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

/*** output ***/

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

int main() {
  enableRawMode();
  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == CTRL_KEY('y')) break;
  }
  return 0;
}
