# heat-text-editor

things learned about C
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
- read function takes input from the user and is used to store a single character each time
- instead of having a bunch of write statements, we append everything onto one character array (string) and then have one big write
- write(STDOUT_FILENO, ...) writes the string to the terminal using the string and its length
- when we want to append, we create a new character array with a larger length, using the realloc function, which basically
  creates a new char array with the previous contents, but adds the length of the string we want to add
 - then we copy the new string to the end of this new array and set the buffer string to this
 - don't forget to deallocate or free up the original string after
 - escape sequences can be used to do various things, like command to clear the screen, or move the cursor
 - basic format: "\x1b[" <-uses two bytes (1 for \x1b and 1 for [), then you put in a number that serves as a parameter,
    and the letter that signifies what command you are calling (J for clear screen command, and H to move the cursor)
