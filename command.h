#ifndef COMMAND_H
#define COMMAND_H
#define MAX_MODIFIERS 10
class SimpleCommand {
public:
    SimpleCommand();                         // Constructor
    void insertArgument(char *argument);     // Insert argument to command
    void clear();                            // Clear arguments
    int _numberOfAvailableArguments;         // Available space for arguments
    int _numberOfArguments;                  // Current number of arguments
    char **_arguments;                       // Array of arguments (command + args)
};

struct Command {
    Command();                               // Constructor
    
    static void changeDirectory(const char *dir) ; // for "cd"
    
    struct Modifier {
        char* type;     // "input", "output", or "append"
        char* fileName; // The file associated with the modifier
    };

    Modifier _modifiers[MAX_MODIFIERS]; // Array to hold modifiers
    int _numberOfModifiers = 0;          // Number of modifiers

    void insertModifier(const char* modifierType, const char* fileName);
    void handleCdCommand();
    void insertSimpleCommand(SimpleCommand *simpleCommand); // Insert simple command
    void clear();                            // Clear commands
    void print();                            // Print command details
    void execute();                          // Execute commands
    void prompt();                           // Print prompt

    SimpleCommand **_simpleCommands;         // Array of simple commands
    int _numberOfAvailableSimpleCommands;    // Available space for simple commands
    int _numberOfSimpleCommands;             // Current number of simple commands
    char *_outFile;                          // Output file (for redirection)
    char *_inputFile;                        // Input file (for redirection)
    char *_errFile;                          // Error file (for redirection)
    int _background;                         // Background execution flag
    bool _append;                            // Append output flag (>>)
   
    
    static Command _currentCommand;          // Current command instance , keep track of current cmd in shell
    static SimpleCommand *_currentSimpleCommand; // Current simple command instance ,individual without pipes or redirection 
};

#endif 

