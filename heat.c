//heat.c, a text editor created by Heather Dinh
//3/23/20

//feature test macros
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define HEAT_VERSION "0.0.1"
#define HEAT_TAB_STOP 8
#define HEAT_QUIT_TIMES 3

//this CTRL_KEY & bitwises the character with 00011111
//basically making the first three 0 so we know the CTRL is pressed
//this replaces every instance of CTRL_KEY(k) with ((k) & 0x1f) which is the expression
//"whatever character you enter" bitwised with 0x1f which is 00011111
#define CTRL_KEY(k) ((k) & 0x1f)

//this kind of sets these arrows to be an int, instead of representing awsd to keep 
//it from conflicting, so now these constants are distinguishable
//the first constant is set 1000, the following ones will be iterated to 1001...
enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY,
    PAGE_UP,
    PAGE_DOWN
};

//this will store a row of text in the editor
//this typedef lets us identify erow as a struct erow, basically an abbreviation
typedef struct erow {
    int size;
    char* chars;
    char* render;           //contains the actual characters that are drawn to text
    int rsize;
}erow;

//this just puts our terminal into a global struct so we can add in the width and height
struct editorConfig {
    struct termios orig_termios;    //the actual screen
    int rows, cols;                 //screen rows and columns
    int cursorX, cursorY;           //cursor x and y
    int rx;                         //render index        
    int numRows;
    erow* row;
    int dirty;
    int rowoff;                     //keeps track of what row on currently, offset
    int coloff;
    char* filename;                 //to display filename in the status bar
    char statusmsg[80];             //to display the messages to the user
    time_t statusmsg_time;          //timestamp to see how long to display messages
};
struct editorConfig E;


/*---------------------------------------------------PROTOTYPES-----------------------------------------------------*/

void editorSetStatusMessage(const char* fmt, ...);
void editorRefreshScreen();
char* editorPrompt(char* prompt);

/*--------------------------------------------------TERMINAL--------------------------------------------------------*/
//this will Print Error of whatever the string that is inserted
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {   //-1 because when it fails, is -1
                                                                    // and sets errno to EAGAIN
        die("tcsetattr");
    }
}

void enableRawMode() {
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;

    /*
    basically when we're doing this &= ~ thing, we're inverting with ~,
    so ECHO in bits is 00000000000000000000000000001000 and this thing inverts it to
    11111111111111111111111111110111. Then when you do &= which is basically 
    raw.c_lflag = raw.c_lflag & ~ECHO, it translates to this:
    11111111111111111111111111111111 = 11111111111111111111111111111111 & 11111111111111111111111111110111;
    All the 1s are compared (1 & 1) and then set to 1 because they are equal
    Then when it comes across the 0, it compares (0 & 1) which evaluates false and sets it to 0
    so now raw.c_lflag has the value 11111111111111111111111111110111
    */
    raw.c_iflag &= ~(BRKINT | IXON | ICRNL | ISTRIP); //input flag field
    raw.c_oflag &= ~(OPOST); //output flag field, turns off the things that makes the cursor
                             //automatically go to beginning of newline, now we manually do it
    raw.c_cflag |= (CS8); // the |= sets the Character Size to 8 bits if it isn't already
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); //local flag field

    //setting a timeout for the read function
    //c_cc is an array of special characters (Control Characters) that has symbolic indices that start with V
    //VMIN is the minimum of characters to read, VTIME is timeout in deciseconds
    //so we're setting the values in that array right now
    raw.c_cc[VMIN] = 0; //set to 0 so that the read function does it right away with every character
    raw.c_cc[VTIME] = 1; //max time that read has before it returns the "ECHO" feature, 100 ms here
                         //when it times out, it returns 0, which is what char c was first defined as

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

//asks for the input from the keyboard
int editorReadKey() {
    int nread;
    char c;
    //while the nread is not 1 means that there is no character input from the keyboard yet
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    //if the key read is an escape character, we look at the next two bytes provided
    if(c == '\x1b') {
        char seq[3];

        //these next two if statements check if there is a parameter or command called
        //if not, then assume that it's just the escape key and return
        if(read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if(read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        //checks if it is an escape sequence, indicated by the [, then checks if they are arrows
        //the ~ character is used for the page up and down, so check for that
        if(seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= '9') {
                //here, it checks the next character if it's a number, then checks that last character if
                //it's a ~, identifying it as a specific command
                if(read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }

                if(seq[2] == '~') {
                    switch(seq[1]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                }
            }else if(seq[0] == 'O') {
                //alternative for these keys that doesn't use '['
                switch(seq[1]) {
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }else {
                switch(seq[1]) {
                    case 'A': 
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H': 
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        }
    }
    return c;
}

//used to store the number of rows and cols using the pointers
int getWindowSize(int* rows, int* cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*-------------------------------------------------ROW OPERATIONS-----------------------------------------------*/

//calculates the render position correctly in the tabs
int editorRowCursorXToRx(erow* row, int cx) {
    int rx = 0;
    int j;
    for(j = 0; j < cx; j++) {
        if(row->chars[j] == '\t') {
            rx += (HEAT_TAB_STOP - 1) - (rx % HEAT_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

void editorUpdateRow(erow* row) {
    int tabs = 0;
    int i = 0;

    for(i = 0; i < row->size; i++) {
        if(row->chars[i] == '\t') {
            tabs++;
        }
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(HEAT_TAB_STOP - 1) + 1);

    int idx = 0;
    for(i = 0; i < row->size; i++) {
        if(row->chars[i] == '\t') {
            row->render[idx++] = ' ';
            while(idx % HEAT_TAB_STOP != 0) {
                row->render[idx++] = ' ';
            }
        }else {
            row->render[idx++] = row->chars[i];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorFreeRow(erow* row) {
    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    if(at < 0 || at >= E.numRows) return;

    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numRows - at - 1));
    E.numRows--;
    E.dirty++;
}

//reallocates the entire text so it uses more memory (# characters in each row, multiplied by # rows)
//set the at at the row we're looking at
void editorInsertRow(int at, char* s, size_t length) {
    if(at < 0 || at > E.numRows) return;
    E.row = realloc(E.row, sizeof(erow) * (E.numRows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numRows - at));

    //using row[at] because now row is a pointer to an erow, so dereference to see which part of row it is
    E.row[at].size = length;
    //malloc just allocates the memory needed, then memcpy copies the line into the erow
    E.row[at].chars = malloc(length + 1);    
    memcpy(E.row[at].chars, s, length);
    E.row[at].chars[length] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numRows++;
    E.dirty++;
}

// inserts a character into erow "row" at a position "at"
void editorRowInsertChar(erow* row, int at, int c) {
    if(at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);         // add 2 here because need space for a null byte
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow* row, char* s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow* row, int at) {
    if(at < 0 || at >= row->size) return;

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}


/*---------------------------------------------------EDITOR OPERATIONS----------------------------------------*/

// takes a character and inserts into the position of cursor
void editorInsertChar(int c) {
    if(E.cursorY == E.numRows) {                 // need to append a new row
        editorInsertRow(E.numRows, "", 0);
    }

    editorRowInsertChar(&E.row[E.cursorY], E.cursorX, c);
    E.cursorX++;
}

// if on the beginning of line, just insert a new row
// otherwise, need to move around line so correct part is at the front
void editorInsertNewline() {
    if(E.cursorX == 0) {
        editorInsertRow(E.cursorY, "", 0);
    }else {
        erow* row = &E.row[E.cursorY];
        editorInsertRow(E.cursorY + 1, &row->chars[E.cursorX], row->size - E.cursorX);
        row = &E.row[E.cursorY];
        row->size = E.cursorX;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }

    E.cursorY++;
    E.cursorX = 0;
}

void editorDelChar() {
    if(E.cursorY == E.numRows) return;
    if(E.cursorX == 0 && E.cursorY == 0) return;

    erow* row = &E.row[E.cursorY];
    if(E.cursorX > 0) {
        editorRowDelChar(row, E.cursorX - 1);
        E.cursorX--;
    }else {
        E.cursorX = E.row[E.cursorY - 1].size;
        editorRowAppendString(&E.row[E.cursorY - 1], row->chars, row->size);
        editorDelRow(E.cursorY);
        E.cursorY--;
    }
}

/*--------------------------------------------------FILE I/O---------------------------------------------------*/
void editorOpen(char* filename) {
    free(E.filename);
    E.filename = strdup(filename);
    //takes in a file, using getline to add all contents into of file into a char*
    FILE* fp = fopen(filename, "r");
    if(!fp) {
        die("fopen");
    }

    char* line = NULL;
    size_t lineCapacity = 0;
    ssize_t lineLength;
    while((lineLength = getline(&line, &lineCapacity, fp)) != -1) {
        while(lineLength > 0 && (line[lineLength - 1] == '\n' || line[lineLength - 1] == '\r')) {
            lineLength--;
        }

        editorInsertRow(E.numRows, line, lineLength);
    }

    free(line);
    fclose(fp);
    E.dirty = 0;
}

// returns a string of the entire text file
char* editorRowsToString(int* buflen) {
    int totalLen = 0;
    int j;
    for(j = 0; j < E.numRows; j++) {
        totalLen += E.row[j].size + 1;
    }
    *buflen = totalLen;

    char* buf = malloc(totalLen);
    char* p = buf;
    for(j = 0; j < E.numRows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;

}

// uses string from function above and saves to disk
void editorSave() {
    if(E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)");
        if(E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
    }

    int len;
    char* buf = editorRowsToString(&len);
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);      // using the fcntl.h library

    if(fd != -1) {
        if(ftruncate(fd, len) != -1) {
            if(write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
       }
       close(fd);
    }

    free(buf);
    editorSetStatusMessage("Can't save, I/O error: %s", strerror(errno));
}

/*----------------------------------------------APPEND BUFFER--------------------------------------------------*/

//instead of having a bunch of write statements, we're appending everything onto
//this buffer string and then doing one big write
struct abuf {
    //char* is used instead of *string because dynamic string not supported in C
    char* bufferString;
    int length;
};

//this statement says that ABUF_INIT is an empty abuf struct; kind of like a constructor
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf* ab, const char* s, int length) {
    //this reallocates more space (adding the length) to memory 
    char* new = realloc(ab->bufferString, ab->length + length);

    if(new == NULL) {
        return;
    }

    //now we copy the original string to the end of the larger, newly created one
    memcpy(&new[ab->length], s, length);
    ab->bufferString = new;
    ab->length += length;
}

//destructor
void abFree(struct abuf* ab) {
    free(ab->bufferString);
}


/*---------------------------------------------------OUTPUT------------------------------------------------------*/

//lets the person scroll down on the editor
void editorScroll() {
    E.rx = 0;
    if(E.cursorY < E.numRows) {
        E.rx = editorRowCursorXToRx(&E.row[E.cursorY], E.cursorX);
    }

    //if the cursor is going up the screen
    if(E.cursorY < E.rowoff) {
        E.rowoff = E.cursorY;
    }
    //if the cursor is going down the screen
    if(E.cursorY >= E.rowoff + E.rows) {
        E.rowoff = E.cursorY - E.rows + 1;
    }
    if(E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if(E.rx >= E.coloff + E.cols) {
        E.coloff = E.rx - E.cols + 1;
    }

}

//draws a tilde on each line, and a welcome message
void editorDrawRows(struct abuf* ab) {
    int i;
    for(i = 0; i < E.rows; i++) {
        //displays a welcome message
        int filerow = i + E.rowoff;
        if(filerow >= E.numRows) {
            //this if statement ^^^ checks if anything has been written yet
            if(E.numRows == 0 && i == E.rows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Heat editor -- version %s", HEAT_VERSION);
                if(welcomelen > E.cols) {
                    welcomelen = E.cols;
                }

                //centering the welcome text in the middle
                int padding = (E.cols - welcomelen) / 2;
                if(padding) {
                    //this statement just makes sure the first line has the tilde
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) {
                    //this keep adding spaces until the padding is 0
                    abAppend(ab, " ", 1);
                }

                abAppend(ab, welcome, welcomelen);
            }else {
                abAppend(ab, "~", 1);
            }
        } else {
            //in the case that there has already been something written already
            int length = E.row[filerow].rsize - E.coloff;
            if(length < 0) {
                length = 0;
            }
            if(length > E.cols) {
                length = E.cols;
            }
            abAppend(ab, &E.row[filerow].render[E.coloff], length);
        }

        //this K command erases part of a line, its parameter is 0, so it will erase to right of the line
        abAppend(ab, "\x1b[K", 3);

        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf* ab) {
    abAppend(ab, "\x1b[7m", 4);
    //stores the status bar stuff, rstatus is the current line number aligned to the right
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numRows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d%d", E.cursorY + 1, E.numRows);
    
    if(len > E.cols) {
        len = E.cols;
    }

    abAppend(ab, status, len);

    while(len < E.cols) {
        if(E.cols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf* ab) {
    abAppend(ab, "\x1b[K", 3);             //clears the message bar with Ctrl+K
    int messageLen = strlen(E.statusmsg);
    if(messageLen > E.cols) {
        messageLen = E.cols;
    }
    if(messageLen && time(NULL) - E.statusmsg_time < 5) {
        abAppend(ab, E.statusmsg, messageLen);
    }
}

//clears the screen
void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;

    //this hides the cursor when it draws
    abAppend(&ab, "\x1b[?25l", 6);

    //abAppend(&ab, "\x1b[2J", 4);      //now we don't need because we'll be clearing line by line
    /*
    here we are writing an escape sequence which always start with 27 or \x1b in binary
    and is followed by a [
    writing 4 bytes out to the terminal
    1) \x1b is the escape character
    2) [ is what comes next
    3) the 2 is an argument to the 'J' command, which specifically says to clear the ENTIRE screen
    4) the J is the command to clear the screen, specified in a way by the argument above
    */

    abAppend(&ab, "\x1b[H", 3);
    /*
    this does the command H that positions the cursor to the top
    there's no argument because the default is the row and column 1, 1 which is already at the top left
    */

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    /*
    this places the cursor after the character that was entered
    add one because the terminal starts at 1, but indices still start at 0 in C
    */
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cursorY - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.bufferString, ab.length);
    abFree(&ab);
}

void editorSetStatusMessage(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*-----------------------------------------------------INPUT-----------------------------------------------------*/

//
char* editorPrompt(char* prompt) {
    size_t bufsize = 128;
    char* buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while(1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if(buflen != 0) {
                buf[--buflen] = '\0';
            }
        }
        else if(c == '\x1b') {
            editorSetStatusMessage("");
            free(buf);
            return NULL;
        }
        else if(c == '\r') {
            if(buflen != 0) {
                editorSetStatusMessage("");
                return buf;
            }
        }else if(!iscntrl(c) && c < 128) {
            if(buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
    }
}

//lets the user move the cursor
void editorMoveCursor(int key) {
    erow *row = (E.cursorY >= E.numRows) ? NULL: &E.row[E.cursorY];     //makes sure that cursorY is still in file

    switch(key){
        case ARROW_LEFT:
            if(E.cursorX != 0) {
                E.cursorX--;
            }else if(E.cursorY > 0) {
                E.cursorY--;
                E.cursorX = E.row[E.cursorY].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cursorX < row->size) {
                //just checks that the x cursor is to the left
                E.cursorX++;
            }else if(row && E.cursorX == row->size) {
                E.cursorY++;
                E.cursorX = 0;
            }
            break;
        case ARROW_UP:
            if(E.cursorY != 0) {
                E.cursorY--;
            }
            break;
        case ARROW_DOWN:
            if(E.cursorY < E.numRows) {
                E.cursorY++;
            }
            break;
    }

    //stops the cursor if it reaches the end of the line
    row = (E.cursorY >= E.numRows) ? NULL: &E.row[E.cursorY];
    int rowlen = row ? row->size : 0;
    if(E.cursorX > rowlen) {
        E.cursorX = rowlen;
    }
}

//processes the input, maps keys to different functions
void editorProcessKeypress() {
    static int quit_times = HEAT_QUIT_TIMES;

    int c = editorReadKey();

    switch(c) {
        case '\r':
            editorInsertNewline();
            break;
        case CTRL_KEY('z'):
            if(E.dirty && quit_times > 0) {
                editorSetStatusMessage("Warning. File has unsaved changes. Press Ctrl-Z %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('s'):
            editorSave();
            break;
        case HOME_KEY:
            E.cursorX = 0;
            break;
        case END_KEY:
            if(E.cursorY < E.numRows) {
                E.cursorX = E.row[E.cursorY].size;
            }
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            //All the same thing to delete a character
            if(c == DEL_KEY) {
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
            break;
        case PAGE_UP: case PAGE_DOWN:
            {
                if(c == PAGE_UP) {
                    E.cursorY = E.rowoff;
                } else if(c == PAGE_DOWN) {
                    E.cursorY = E.rowoff + E.rows - 1;
                    if(E.cursorY > E.numRows) {
                        E.cursorY = E.numRows;
                    }
                }
                //times can't be declared in the case, so must make it a block using brackets
                int times = E.rows;
                while(times--) {
                    //ternary operator
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case ARROW_LEFT: case ARROW_RIGHT: case ARROW_UP: case ARROW_DOWN:
            editorMoveCursor(c);
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            // refresh/escape, don't do anything
            break;

        default:
            editorInsertChar(c);
            break;
    }

    quit_times = HEAT_QUIT_TIMES;
}

/*--------------------------------------------------INITIALIZATION----------------------------------------------*/

void initEditor() {
    E.cursorX = 0;
    E.cursorY = 0;
    E.rx = 0;
    E.numRows = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.row = NULL;
    E.dirty = 0;
    //E.rows -= 1;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    if(getWindowSize(&E.rows, &E.cols) == -1) die("getWindowSize");
    E.rows -= 2;
}

int main(int argc, char* argv[]) {
    printf("\033[0;36m");
    enableRawMode();
    initEditor();
    if(argc >= 2) {
        editorOpen(argv[1]);
    }
    
    editorSetStatusMessage("HELP: Ctrl-Z = quit");
    
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 5;
}