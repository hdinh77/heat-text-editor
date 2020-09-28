# "Heat" Text Editor

<img src="https://github.com/hdinh77/heat-text-editor/blob/master/heat.JPG"></img>

# Notes on topics learned about C

## Low-level terminal / character key processing
- escape sequences, and how each of them correlate to different keypresses and how they can be changed
- #define creates a global constant that is replaced with the following string
- when we defined CTRL_KEY, this replaces every instance of CTRL_KEY(k) with ((k) & 0x1f) which is the expression
  "whatever character you enter which is represented as k" bitwise-AND with 0x1f (which is 00011111), essentially
  just making the first three digits 0, indicating a control key

## Termios
  termios is a structure that basically is just the terminal from the termios.h header file
- in the termios struct, has flags that that can control the settings including the input, output, and local fields
- to access these flags, have to do termiosName.c_iflag etc.
- basically when we're doing this &= ~ thing, we're inverting with ~,
    so ECHO in bits is 00000000000000000000000000001000 and this thing inverts it to
    11111111111111111111111111110111. Then when you do &= which is basically 
    raw.c_lflag = raw.c_lflag & ~ECHO, it translates to this:
    11111111111111111111111111111111 = 11111111111111111111111111111111 & 11111111111111111111111111110111;
    All the 1s are compared (1 & 1) and then set to 1 because they are equal
    Then when it comes across the 0, it compares (0 & 1) which evaluates false and sets it to 0
    so now raw.c_lflag has the value 11111111111111111111111111110111
- tcgetattr is the function to get all the settings and variables
- tcsetattr is the function to set the changes
- if either of these functions fails, they return -1 and set a global variable errno to EAGAIN, which indicates an error

## Control Characters
- c_cc is an array of special characters (Control Characters) that has symbolic indices that start with V
- VMIN is the minimum of characters to read, VTIME is timeout in deciseconds; here we're changing those values
- VMIN set to 0 so that the read function does it right away with every character
- max time that read has before it returns the "ECHO" feature, 100 ms here when it times out, 
    it returns 0, which is what char c was first defined

## Writing to the terminal
- read function takes input from the user and is used to store a single character each time, some low-level character handling
- instead of having a bunch of write statements, we append everything onto one character array (string) and then have one big write
- write(STDOUT_FILENO, ...) writes the string to the terminal using the string and its length
- when we want to append, we create a new character array with a larger length, using the realloc function, which basically
  creates a new char array with the previous contents, but adds the length of the string we want to add
 - then we copy the new string to the end of this new array and set the buffer string to this
 - don't forget to deallocate or free up the original string after

## Escape sequences
 - escape sequences can be used to do various things, like command to clear the screen, or move the cursor
 - basic format: "\x1b[" <-uses two bytes (1 for \x1b and 1 for [), then you put in a number that serves as a parameter,
    and the letter that signifies what command you are calling (J for clear screen command, and H to move the cursor)
- to check if a character is an escape character, check that it equals '\x1b' (w/o the '\' because that's a separate byte)
- after this, if there is a '[', then it's an escape sequence and you can read the following parameters/commands
- if there isnt, it's probably just the escape character

## Saving text file to a disk
 - basically have a single string that can be written into a file
 - convert editor rows to a string by first allocating enough space for length of the rows, make sure to add 1 for newline character
 - then copy over the rows using memcpy()
 - then have editorSave function to write to disk
 - <fcntl.h> library for open(), O_RDWR, O_CREAT
 - open() creates a new file, O_RDWR flag indicates for it to read and write, O_CREAT flag means the file doesn't already exist
 - ```open(E.filename, O_RDWR | O_CREAT, 0644);```
 - the 0644 is standard permissions for the text file, lets owner read and write and ecerybody else can only read
 - ftruncate() sets file size to the data written in the text editor, by make it the same length
 - O_TRUNC can make the file empty, but not using here because the write() could fail and lose all text
 - then map the Ctrl-S to save

## Save as function
 - hold the user input in the buf variable
 - keep having the screen refresh and print out the status
 - if "enter" is pressed and nothing in input, will keep going; else it will return the name inputted
 - if something other than the "enter" and CNTRL is pressed, append it to the buf 
 - using 128 because all the other CNTRL numbers are higher than it
 - if the escape key '\x1b' is pressed, free the buf and stop inserting a name

## Syntax highlighting
 - now we have to iterate through every character as it is entered and change the color if it is a number
 - escape key "\x1b[39m" sets the terminal back to the default color
 - the "m" command at the end of the escape key lets us set the text color
 - creating another array in each row that stores the highlighting, called hl so it updates each time
 - created an enum to store the colors in the syntax highlighting
 - everytime we do this, need to reallocate the memory because the render might be bigger now so the hl has to match
 - memset basically all the elements in this newly reallocated memory to the default color, then loop through to change color
 - store a current_color so it is more efficient and doesn't have to keep changing colors every character

## Detecting file type (like C++ or txt)
 - using an editorSyntax struct to display filetype, match the file, and also set a flag to see if need to highlight
 - strrchr() returns pointer to last occurance of a character in the string, so here we look for the '.' for filetype
 - strcmp() compares two strings and returns 0 if they are equal
 - if there is no extension, then you can check if that pattern ("c" or "cpp") is anywhere in the name by using strstr()
 - want to do this whenever the user opens or saves a document

## Miscellaneous C information
- an enum is a good way to assign names to constants, kind of like define
- in an enum, if the first constant is set 1000, the following ones will be iterated
- when checking for certain escape sequences, can check each individual character by reading one character,
  - and storing that at the address of an element in a character array
- ternary operator: using the ? and : to evaluate a boolean and return an expression (not a statement)
- condition ? value_if_true : value_if_false ---- this is used as (c == PAGE_UP ? ARROW_UP : ARROW_DOWN) in the program to 
  - simply evaluate whether the page up or page down is being used
- variables can't be declared in a switch statement or its cases, so enclose it in a block using the brackets
- typedef is used as an abbreviation for a specific type, here we use it so we can write just erow instead of struct erow every time
- from the <stdio.h> library, FILE is the file object, to open a file use fopen(pointer_to_filename, pointer_to_mode)
- this is using the "r" mode, which opens the file for reading
- function getline() sets the first param to point to the memory, and lineCapacity to how much memory allocated
- realloc function takes in the original variable, and allocates the following space for it to copy into that 
  - you specify in the function
- <stdarg.h>, macros provide a portable way to access the arguments to a function when the function takes a variable number of arguments
- ... argument is a variadic function, meaning you can take a lot of arguments
- memmove() is from <string.h>, like memcpy()
- the backspace key does not have a backslash-escape representation (like \n or \t) so we need to give it a value
- '\r' is the enter key
- dirty flag indicates if the file has been saved yet
  
