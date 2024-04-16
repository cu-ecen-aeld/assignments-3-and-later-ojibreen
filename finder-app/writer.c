#include <stdio.h>
#include <syslog.h>

int main( int argc, char *argv[] ) {
    // Setup logging.
    openlog(NULL, 0, LOG_USER);


    // Check for the right number of arguments.
    if( argc < 3 ) {
        syslog(LOG_ERR, "Invalid number of arguments. %d arguments given.", argc);
        printf("Two arguments are required.\n Argument 1: File path.\n Arguemtn 2: Text to write.");
        return 1;
    }
    
    // Open a file and print the contents of argv[2] to it.
    FILE *f = fopen(argv[1], "w");
    if (f == NULL) {
        printf("Error opening file!\n");
        syslog(LOG_ERR, "Error opening file!");
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
    fprintf(f, "%s\n", argv[2]);
    fclose(f);
    return 0;
}
