//
//  kilo.c
//  Text Editor
//
//  Created by Simionescu Teodor on 29.07.2022.
//

/* includes */

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <stdio.h>

/*defines*/
/*
  --->terminate key == Q !!!!!
 */
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define SHIFT_Q(k) ((k) & 0x51) // end
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey{ // using int type with a high value to avoid confussion with chars
    FIND_KEY=70,
    SAVE_KEY=83, //save key
    BACKSPACE=127,
    ARROW_RIGHT = 1000,
    ARROW_LEFT ,
    ARROW_UP ,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

enum editorHighlight{
    HL_NORMAL=0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRING (1<<1)

/*data*/

struct editorSyntax{ //--used for highlighting
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};

typedef struct erow{ //editor row -> storest a line of text as a pointer to the dynamically-allocated character data and length.
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hl_open_comment;
}erow;

struct editorConfig{ //--global editor struct
    int cx, cy; //coordonates for x and y on the terminal
    int rx; //coordonate for showing TABS/etc
    int rowoff; //vertical scrolling
    int coloff; //horizontal scrolling
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios orig_termios;
};
struct editorConfig E;

/* filetypes */

char *C_HL_extensions[] = {".c", ".cpp", ".h", NULL};

char *C_HL_keywords[]={
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL
};

struct editorSyntax HDLB[] = { //--hl database for C language
    {
        "c", //filetype
        C_HL_extensions, //the extensions
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRING //flag field
    },
};

#define HDLB_ENTRIES (sizeof(HDLB)/sizeof(HDLB[0]))

/*prototypes*/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char*, int));

/*terminal*/

void die(const char *s){ //EH
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

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if(c=='\x1b'){
        char seq[3];
        
        if(read(STDIN_FILENO, &seq[0], 1)!= 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1)!= 1) return '\x1b';
        if(seq[0]=='['){
            if(seq[1]>='0' && seq[1]<='9'){
                if(read(STDIN_FILENO, &seq[2], 1)!=1) return '\x1b';
                if(seq[2]=='~'){
                    switch(seq[1]){
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }else{
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O'){
            switch(seq[1]){
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        
        return '\x1b';
    }else{
        return c;
    }
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

/* syntax highlighting*/

int is_separator(int c){
    return isspace(c) || c=='\0' || strchr(",.()+-/*=~%<>[];", c) !=NULL;
}

void editorUpdateSyntax(erow *row){
    row->hl = realloc(row->hl, row->size);
    memset(row->hl, HL_NORMAL, row->rsize);//--an unlighted charachter will have a
    //value of HL_NORMAL in hl
    
    if(E.syntax == NULL) return;
    
    char **keywords = E.syntax->keywords; //alias
    
    char *scs = E.syntax->singleline_comment_start; //alias
    char *mcs =E.syntax->multiline_comment_start;
    char *mce =E.syntax->multiline_comment_end;
    
    int scs_len= scs ? strlen(scs) : 0;
    int mcs_len= mcs ? strlen(mcs) : 0;
    int mce_len= mce ? strlen(mce) : 0;
    
    int prev_step=1; //--the begginig of a line is a separator
    int in_string=0;
    int in_comment=(row->idx >0 && E.row[row->idx-1].hl_open_comment);//--boolean to keep track if we are in a multiline comment. True if the previous line has unclosed multiline comment
    
    int i=0;
    while(i<row->rsize){
        char c=row->render[i];
        unsigned char prev_hl = (i>0)? row->hl[i-1] : HL_NORMAL;
        
        //--decide if we should hl single-line comments and also check if we re not in a string
        if(scs_len && !in_string && !in_comment){
            if(!strncmp(&row->render[i], scs, scs_len)){
                memset(&row->hl[i], HL_COMMENT, row->rsize-1);
                break;
            }
        }
        
        //--both should be non NULL to hl a multicomment. Is necessary to not be in a string
        if(mcs_len && mce_len && !in_string){
            if(in_comment){
                //--if we're inside a multiline comment, just hl the content
                row->hl[i] = HL_MLCOMMENT;
                //--checking the final state of the multiline comment
                if(!strncmp(&row->render[i], mce, mce_len)){
                    //--if so, hl the whole mce stirng
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i+=mce_len; //--consume it
                    in_comment=0;
                    prev_step=1;
                    continue;
                }else{ //--if we're not at the end, simply consume the current character
                    i++;
                    continue;
                }
            }else if(!strncmp(&row->render[i], mcs, mcs_len)){
                //--we're at the start of a multiline comment
                memset(&row->render[i], HL_MLCOMMENT, mcs_len);
                i+=mcs_len;
                in_comment=1;
                continue;
            }
        }
        
        if(E.syntax->flags & HL_HIGHLIGHT_STRING){
            //--if set, the current character can be hl with HL_STRING
            //if not, check if we're at the beggining of a string by checking single/double quote
            if(in_string){
                //--if we're in a string and the current ch is a backslach \ and
                //there is at least one more ch line afte \, we highlight the ch that comes afte \.
                row->hl[i]=HL_STRING;
                if(c=='\\' && i+1<row->rsize){
                    row->hl[i+1] =HL_STRING;
                    i+=2;
                    continue;
                }
                //--if the current character is closing quote, reset the in_string
                if(c==in_string) in_string=0;
                i++;//--consume the character
                prev_step = 1; //--closing character is considered a separator
                continue;
            }else{
                if(c=='"' || c=='\''){
                    in_string = c;
                    row->hl[i]=HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        
        //--check if the numbers should be hl in this text
        if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
            //--to highlight a digit is required that the prev character is either a separator
            //or to be already highlighted
            if((isdigit(c) && (prev_step || prev_hl == HL_NUMBER)) ||
               (c=='.' && prev_hl==HL_NUMBER)){
                row->hl[i] = HL_NUMBER;
                i++; //--consume the character
                prev_step=0; //--this indicate that we are in the middle of highlighting something
                continue;
            }
        }
        
        //--only if a separator came before, then we can consider a data type
        if(prev_step){
            int j;
            for(j=0; keywords[j]; j++){
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen-1]=='|';
                if(kw2) klen--;
                
                //--check if a keyword exists at our position in the text and we check
                //to see if a separator character comes after the keyword
                if(!strncmp(&row->render[i], keywords[j], klen) &&
                   is_separator(row->render[i+klen])){
                    //--passed, meaning that we have a word to hl
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i+=klen; //--consume the entire keyword
                    break;
                }
            }
            if(keywords[j]!=NULL){
                prev_step=0;
                continue; //if the for loop broke
            }
        }
        
        prev_step=is_separator(c);
        i++;
    }
    
    int changed =(row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment; //--tells if the row ended as an unclosed multiline comment or not
    //--if there is a next line and the state of hl_open_comment changed, call again
    if(changed && row->idx+1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx+1]);
}

int editorSyntaxToColor(int hl){
    switch (hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT:
            return 36; //--cyan
        case HL_KEYWORD1:
            return 33; //--yellow
        case HL_KEYWORD2:
            return 32; //--green
        case HL_STRING:
            return 35; //--magneta
        case HL_NUMBER:
            return 31;
        case HL_MATCH:
            return 34;
        default:
            return 37;
    }
}

void editorSelectSyntaxHighlight(){
    E.syntax = NULL; //--if nothing matches there will be no filename/filetype
    if(E.filename == NULL) return;
    
    char *ext = strrchr(E.filename, '.'); //--last position of '.' in filename
    
    for(unsigned int j=0; j<HDLB_ENTRIES; j++){
        struct editorSyntax *s= &HDLB[j];
        unsigned int i=0;
        while(s->filematch[i]){
            int is_ext= (s->filematch[i][0] == '.'); //--if is a file extension
            //--if the file ends with the same extension or at least if
            //the pattern exists anywhere in the filename
            if((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
               (!is_ext && strstr(E.filename, s->filematch[i]))){
                E.syntax = s;
                
                //--the hl immediately changes when the filetype changes
                int filerow;
                for(filerow=0; filerow< E.numrows; filerow++){
                    editorUpdateSyntax(&E.row[filerow]);
                }
                
                return;
            }
            i++;
        }
    }
}

/* row operations*/

int editorRowCxToRx(erow *row, int cx){
    int rx=0;
    int j;
    for(j=0;j<cx; j++){
        if(row->chars[j]=='\t') // if we are on a tab character, we're going right in the back of the next TAB character
            rx+=(KILO_TAB_STOP-1)-(rx%KILO_TAB_STOP);
        rx++;
    }
    return rx;
}

int editorRowRxtoCx(erow *row, int rx){
    int cur_rx = 0;
    int cx;
    for(cx=0; cx<row->size; cx++){
        if(row->chars[cx] == '\t')
            cur_rx += (KILO_TAB_STOP -1) - (cur_rx%KILO_TAB_STOP);
        cur_rx++;
        
        if(cur_rx >rx) return cx;
    }
    return cx;
}

void editorUpdateRow(erow *row){
    free(row->render);
    row->render = malloc(row->size +1);
    
    int tabs=0;
    int j;
    for(j=0; j<row->size; j++)
        if(row->chars[j]=='\t') tabs++;
    
    free(row->render);
    row->render=malloc(row->size+tabs*(KILO_TAB_STOP-1)+1);
    
    int idx=0;
    for(j=0; j<row->size; j++){
        if(row->chars[j]=='\t'){
            row->render[idx++]=' ';
            while(idx% KILO_TAB_STOP !=0) row->render[idx++] = ' ';
        }else{
            row->render[idx++]=row->chars[j];
        }
    }
    row->render[idx]='\0';
    row->rsize=idx;
    
    editorUpdateSyntax(row); //makes sense to update the hl array here
}

void editorInsertRow(int at, char *s, size_t len){
    if(at<0 || at> E.numrows) return;
    
    E.row = realloc(E.row, sizeof(erow)*(E.numrows+1));
    memmove(&E.row[at+1], &E.row[at], sizeof(erow)*(E.numrows-at));
    for(int j=at+1; j<E.numrows; j++) E.row[j].idx++;
    
    E.row[at].idx=at; //--initialize to the rows index in the file at the time is inserted
    
    E.row[at].size = len;
    E.row[at].chars = malloc(len+1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize=0;
    E.row[at].render=NULL;
    E.row[at].hl=NULL;
    E.row[at].hl_open_comment=0;
    editorUpdateRow(&E.row[at]);
    
    E.numrows++;
    E.dirty++;
}

void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at){
    if(at<0 || at>=E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at+1], sizeof(erow)*(E.numrows-at-1));
    for(int j=at; j<E.numrows-1; j++) E.row[j].idx--;
    E.numrows--;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c){
    if( at<0 || at > row->size) at= row->size;
    row->chars = realloc(row->chars, row->size*2);
    memmove(&row->chars[at+1], &row->chars[at], row->size-at+1);
    row->size++;
    row->chars[at]=c;
    editorUpdateRow(row); //update render & rsize
    E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len){
    row->chars = realloc(row->chars, row->size+len+1);
    memcpy(&row->chars[row->size], s, len);
    row->size+=len;
    row->chars[row->size]='\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at){
    if(at<0 || at>=row->size) return;
    memmove(&row->chars[at], &row->chars[at+1],row->size-at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/*editor Operations*/

void editorInsertChar(int c){
    if(E.cy == E.numrows) editorInsertRow(E.numrows, "", 0); //finale line
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewline(){
    if(E.cx==0){
        editorInsertRow(E.cy, "", 0);
    }else{
        erow *row=&E.row[E.cy];
        editorInsertRow(E.cy+1, &row->chars[E.cx], row->size-E.cx);
        row=&E.row[E.cy];
        row->size=E.cx;
        row->chars[row->size]='\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx=0;
}

void editorDelChar(){
    if(E.cy==E.numrows) return;
    if(E.cx==0 && E.cy==0) return;
    
    erow *row = &E.row[E.cy];
    if(E.cx>0){
        editorRowDelChar(row, E.cx-1);
        E.cx--;
    }else{
        E.cx =E.row[E.cy-1].size;
        editorRowAppendString(&E.row[E.cy-1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

/*file i/o*/

char *editorRowtoString(int *buflen){
    int totlen=0;
    int j;
    for(j=0; j<E.numrows;j++)
        totlen += E.row[j].size +1;
    *buflen = totlen;
    
    char *buf = malloc(totlen);
    char *p=buf;
    for(j=0;j<E.numrows; j++){
        memcpy(p, E.row[j].chars, E.row[j].size);
        p+=E.row[j].size;
        *p='\n';
        p++;
    }
    
    return buf;
}

void editorOpen(char *filename){
    free(E.filename);
    E.filename= strdup(filename);
    
    editorSelectSyntaxHighlight();
    
    FILE *fp= fopen(filename, "r");
    if(!fp) die("fopen");
    
    char *line= NULL;
    size_t linecap=0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp))!= -1){
        while(linelen>0 && (line[linelen-1]=='\n' || line[linelen-1]=='\r'))
            linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    
    free(line);
    fclose(fp);
    E.dirty=0;
}

void editorSave(){
    if(E.filename==NULL){
        E.filename=editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if(E.filename==NULL){
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }
    
    int len;
    char *buf=editorRowtoString(&len);
    
    int fd= open(E.filename, O_RDWR | O_CREAT, 0644);
    if(fd!=-1){
        if((ftruncate(fd, len))!=-1){
            if((write(fd, buf, len))==len){
                close(fd);
                free(buf);
                E.dirty=0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/* find */

void editorFindCallback(char *query, int key){
    static int last_match = -1;
    static int direction = 1;
    
    static int saved_hl_line;
    static char *saved_hl=NULL;
    
    if(saved_hl){
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl=NULL;
    }
    
    if(key=='\r' || key=='\x1b'){
        last_match = -1;
        direction =1;
        return;
    }
    else if(key == ARROW_RIGHT || key ==ARROW_DOWN) direction =1;
    else if(key ==ARROW_LEFT || key ==ARROW_UP) direction =-1;
    else{
        last_match =-1;
        direction =1;
    }
    
    if(last_match ==-1) direction =1;
    int current = last_match;
    int i;
    for(i=0; i<E.numrows; i++){
        current += direction;
        if(current==-1) current = E.numrows-1;
        else if(current == E.numrows) current=0;
        
        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if(match){
            last_match = current;
            E.cy =current;
            E.cx = editorRowRxtoCx(row, match- row->render);
            E.rowoff = E.numrows;
            
            saved_hl_line= current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match-row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind(){
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;
    
    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
    
    if(query) free(query);
    else{
        E.cx = saved_cx;
        E.cy= saved_cy;
        E.rowoff = saved_rowoff;
        E.coloff = saved_coloff;
    }
}

/*abbend buffer*/

struct abuf{
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);
    
    if(new==NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b=new;
    ab->len+=len;
}

void abFree(struct abuf *ab){
    free(ab->b);
}

/*** output ***/

void editorScroll(){
    E.rx=0;
    if(E.cy<E.numrows){
        E.rx=editorRowCxToRx(&E.row[E.cy], E.cx);
    }
    
    if(E.cy < E.rowoff){
        E.rowoff=E.cy;
    }
    if(E.cy>=E.rowoff+E.screenrows){
        E.rowoff = E.cy- E.screenrows + 1;
    }
    if(E.rx < E.coloff){
        E.coloff = E.rx;
    }
    if(E.rx >=E.coloff + E.screencols){
        E.coloff = E.rx - E.screencols+1;
    }
}

void editorDrawRows(struct abuf *ab){
    int y;
    for(y=0; y<E.screenrows; y++){
        int filerow = y+E.rowoff;
        if(filerow >= E.numrows){ //if we draw a new row
            if(E.numrows==0 && y==E.screenrows/3){
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                                         "KILO -- version %s", KILO_VERSION);
                if(welcomelen > E.screencols) welcomelen=E.screencols;
                int padding = (E.screencols-welcomelen)/2;
                if(padding){
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else{
                abAppend(ab, "~", 1);
            }
        } else{ // if we draw a row that is part of the text buffer
            int len= E.row[filerow].rsize - E.coloff;
            if(len<0) len =0;
            if(len>E.screencols) len=E.screencols;
            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color=-1;
            int j;
            for(j=0;j<len; j++){
                //if is a control character
                if(iscntrl(c[j])){
                    char sym= (c[j]<=26) ? '@' + c[j] : '?'; //--in ascii, the capital letter
                    //comes after @
                    abAppend(ab, "\x1b[7m", 4); //--switch to inverted colors
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3); //--turn off inverted colors again
                    if(current_color !=-1){
                        char buf[16];
                        int clen = snprintf ( buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                }else if(hl[j]==HL_NORMAL){
                    if(current_color!=-1){
                        abAppend(ab, "\x1b[39m", 5);
                        current_color=-1;}
                    abAppend(ab, &c[j], 1);
                }else{
                    int color= editorSyntaxToColor(hl[j]);
                    if(color!=current_color){
                        current_color= color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }
        
        abAppend(ab, "\x1b[K", 3);
        if(y<E.screenrows-1){
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorDrawStatusBar(struct abuf *ab){
    abAppend(ab, "\x1b[7m", 4); // text will be printed with inverted colors
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s- %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.dirty ? "modified": "");
    int rlen= snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
                       E.syntax ? E.syntax->filetype : "no ft" ,E.cy+1, E.numrows);
    if(len > E.screencols) len= E.screencols;
    abAppend(ab, status, len);
    while(len < E.screencols){
        if(E.screencols - len == rlen){
            abAppend(ab, rstatus, rlen);
            break;
        }else{
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3); // text back to normal
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab){
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screencols) msglen = E.screencols;
    if(msglen && time(NULL)-E.statusmsg_time<5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
    editorScroll();
    
    struct abuf ab = ABUF_INIT;
    
    abAppend(&ab, "\x1b[?25l", 6);
    //abAppend(&ab, "\x1b[2J", 4); //not required anymore, cleaning 1 row at the time
    abAppend(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy-E.rowoff)+1, (E.rx-E.coloff)+1);
    abAppend(&ab, buf, strlen(buf));
    
    abAppend(&ab, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...){
    va_list ap;
    va_start (ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time= time(NULL);
} //variadic function

/*** input ***/

char *editorPrompt(char *prompt, void(*callback)(char *, int)){
    size_t bufsize=128;
    char *buf=malloc(bufsize);
    
    size_t bufflen=0;
    buf[0]='\0';
    
    while(1){
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();
        
        int c=editorReadKey();
        if(c==DEL_KEY || c==CTRL_KEY('h') || c==BACKSPACE){
            if(bufflen!=0) buf[--bufflen] = '\0';
        }else if(c=='\x1b'){
            editorSetStatusMessage("");
            if(callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if(c=='\r'){
            if(bufflen!=0){
                editorSetStatusMessage("");
                if(callback) callback(buf, c);
                return buf;
            }
        }else if(!iscntrl(c) && c<128){
            if(bufflen == bufsize-1){
                bufsize *=2;
                buf=realloc(buf, bufsize);
            }
            buf[bufflen++]=c;
            buf[bufflen]='\0';
        }
        
        if(callback) callback(buf, c);
    }
}

void editorMoveCursor(int key){
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    
    switch(key){
        case ARROW_LEFT:
            if(E.cx!=0){
                E.cx--;
            }else if(E.cy>0){
                E.cy--;
                E.cx= E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size){
                E.cx++;
            }else if(row && E.cx==row->size){
                E.cy++;
                E.cx=0;
            }
            break;
        case ARROW_UP:
            if(E.cy!=0){
                E.cy--;}
            break;
        case ARROW_DOWN:
            if(E.cy<E.numrows){
                E.cy++;}
            break;
    }
    
    row= (E.cy>=E.numrows) ? NULL: &E.row[E.cy];
    int rowlen = row ? row->size:0;
    if(E.cx > rowlen){
        E.cx = rowlen;
    }
}

void editorProcessKeypress() {
    static int quit_times=KILO_QUIT_TIMES;
    int c = editorReadKey();

  switch (c) {
      case '\r':
          editorInsertNewline();
          break;
          
      case SHIFT_Q('Q'):
          if(E.dirty && quit_times>0){
              editorSetStatusMessage("WARNING! ! ! File has unsaved changes. "
                                     "Press Q %d more times to quit.", quit_times);
              quit_times--;
              return;
          }
          write(STDOUT_FILENO, "\x1b[2J", 4);
          write(STDOUT_FILENO, "\x1b[H", 3);
          exit(0);
          break;
          
      case SAVE_KEY:
          editorSave();
          break;
          
      case HOME_KEY:
          E.cx=0;
          break;
          
      case END_KEY:
          if(E.cy< E.numrows)
              E.cx = E.row[E.cy].size;
          break;
          
      case FIND_KEY:
          editorFind();
          break;
          
      case BACKSPACE:
      case CTRL_KEY('h'):
      case DEL_KEY:
          if(c==DEL_KEY) editorMoveCursor(ARROW_RIGHT);
          editorDelChar();
          break;
          
      case PAGE_UP:
      case PAGE_DOWN:
        {
            if(c==PAGE_UP){
                E.cy=E.rowoff;
            }else if(c==PAGE_DOWN){
                E.cy=E.rowoff + E.screenrows-1;
                if(E.cy>E.numrows) E.cy = E.numrows;
            }
            
            int times = E.screenrows;
            while(times--)
                editorMoveCursor(c==PAGE_UP ? ARROW_UP: ARROW_DOWN);
        } break;
          
      case ARROW_UP:
      case ARROW_DOWN:
      case ARROW_RIGHT:
      case ARROW_LEFT:
          editorMoveCursor(c);
          break;
          
      case CTRL_KEY('l'):
      case '\x1b':
          break;
          
      default:
          editorInsertChar(c);
          break;
  }
}

/*** init ***/

void initEditor(){
    E.cx=0;
    E.cy=0;
    E.rx=0;
    E.rowoff=0;
    E.coloff=0;
    E.numrows=0;
    E.row=NULL;
    E.dirty = 0;
    E.filename=NULL;
    E.statusmsg[0]='\0';
    E.statusmsg_time=0;
    E.syntax=NULL;
    
    if(getWindowSize(&E.screenrows, &E.screencols)==-1) die("getWindowSize");
    E.screenrows-=2;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if(argc>=2){
        editorOpen(argv[1]);
    }
    
    editorSetStatusMessage("HELP: S = save | Q = quit | CTRL-F = find");
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}
