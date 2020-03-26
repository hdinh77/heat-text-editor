//heat.c, a text editor created by Heather Dinh
//3/23/20

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define HEAT_VERSION "0.0.1"

//this CTRL_KEY & bitwises the character with 00011111
//basically making the first three 0 so we know the CTRL is pressed
//this replaces every instance of CTRL_KEY(k) with ((k) & 0x1f) which is the expression
//"whatever character you enter" bitwised with 0x1f which is 00011111
#define CTRL_KEY(k) ((k) & 0x1f)

//this just puts our terminal into a global struct so we can add in the width and height
struct editorConfig {
    struct termios orig_termios;
    int rows;
    int cols;
};
struct editorConfig E;


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
char editorReadKey() {
    int nread;
    char c;
    //while the nread is not 1 means that there is no character input from the keyboard yet
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
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

//draws a tilde on each line, and a welcome message
void editorDrawRows(struct abuf* ab) {
    int i;
    for(i = 0; i < E.rows; i++) {
        //displays a welcome message
        if(i == E.rows / 3) {
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

        //this K command erases part of a line, its parameter is 0, so it will erase to right of the line
        abAppend(ab, "\x1b[K", 3);

        if(i < E.rows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

//clears the screen
void editorRefreshScreen() {
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

    abAppend(&ab, "\x1b[?25h", 6);

    editorDrawRows(&ab);
    abAppend(&ab, "\x1b[H", 3);

    write(STDOUT_FILENO, ab.bufferString, ab.length);
    abFree(&ab);
}

/*-----------------------------------------------------INPUT-----------------------------------------------------*/

//processes the input, maps keys to different functions
void editorProcessKeypress() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('z'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*--------------------------------------------------INITIALIZATION----------------------------------------------*/

void initEditor() {
    if(getWindowSize(&E.rows, &E.cols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();
    
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 5;
}