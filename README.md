# heat-text-editor

<img src="https://github.com/hdinh77/heat-text-editor/blob/master/heat.JPG"></img>

things learned about C
- low-level terminal / character key processing
- escape sequences, and how each of them correlate to different keypresses and how they can be changed
- #define creates a global constant that is replaced with the following string
- when we defined CTRL_KEY, this replaces every instance of CTRL_KEY(k) with ((k) & 0x1f) which is the expression
  "whatever character you enter which is represented as k" bitwise-AND with 0x1f (which is 00011111), essentially
  just making the first three digits 0, indicating a control key
- termios is a structure that basically is just the terminal from the termios.h header file
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
- c_cc is an array of special characters (Control Characters) that has symbolic indices that start with V
- VMIN is the minimum of characters to read, VTIME is timeout in deciseconds; here we're changing those values
- VMIN set to 0 so that the read function does it right away with every character
- max time that read has before it returns the "ECHO" feature, 100 ms here when it times out, 
    it returns 0, which is what char c was first defined
- tcgetattr is the function to get all the settings and variables
- tcsetattr is the function to set the changes
- if either of these functions fails, they return -1 and set a global variable errno to EAGAIN, which indicates an error
- read function takes input from the user and is used to store a single character each time, some low-level character handling
- instead of having a bunch of write statements, we append everything onto one character array (string) and then have one big write
- write(STDOUT_FILENO, ...) writes the string to the terminal using the string and its length
- when we want to append, we create a new character array with a larger length, using the realloc function, which basically
  creates a new char array with the previous contents, but adds the length of the string we want to add
 - then we copy the new string to the end of this new array and set the buffer string to this
 - don't forget to deallocate or free up the original string after
 - escape sequences can be used to do various things, like command to clear the screen, or move the cursor
 - basic format: "\x1b[" <-uses two bytes (1 for \x1b and 1 for [), then you put in a number that serves as a parameter,
    and the letter that signifies what command you are calling (J for clear screen command, and H to move the cursor)
- to check if a character is an escape character, check that it equals '\x1b' (w/o the '\' because that's a separate byte)
- after this, if there is a '[', then it's an escape sequence and you can read the following parameters/commands
- if there isnt, it's probably just the escape character
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
  