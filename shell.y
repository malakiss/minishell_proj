%token <string_val> WORD
%token NOTOKEN GREAT NEWLINE LESS GREATGREAT AMPERSAND PIPE

%union {
    char *string_val;
}

%{
extern "C" 
{
	int yylex();
	void yyerror (char const *s);
}
#define yylex yylex
#include <stdio.h>
#include "command.h"
#include <string.h>
#include <unistd.h>
%}

%%

// Starting point of the parser
goal:
    commands
    ;

commands:
    command
    | commands command
    ;

command:
    pipeline iomodifier_opt background_opt NEWLINE {
        printf("   Yacc: Execute command\n");
        Command::_currentCommand.execute();
    }
    | NEWLINE
    | error NEWLINE { yyerrok; }
    ;

pipeline:
    simple_command {
        Command::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
        Command::_currentSimpleCommand = nullptr; // Reset after insertion to avoid duplicates
    }
    | pipeline PIPE simple_command {
        Command::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
        Command::_currentSimpleCommand = nullptr; // Reset after insertion to avoid duplicates
        printf("   Yacc: Added pipe\n");
    }
    ;

simple_command:
    command_and_args
    ;

command_and_args:
    command_word arg_list
    ;

arg_list:
    arg_list argument
    | /* can be empty */
    ;

argument:
    WORD {
        printf("   Yacc: insert argument \"%s\"\n", $1);
        Command::_currentSimpleCommand->insertArgument($1);
    }
    ;

command_word:
    WORD {
        if (strcmp($1, "exit") == 0) {
            printf("Goodbye!\n");
            Command::_currentCommand.clear(); // Clear the command
            exit(0); // Exit the shell
        } else {
            printf("   Yacc: insert command \"%s\"\n", $1);
            Command::_currentSimpleCommand = new SimpleCommand();
            Command::_currentSimpleCommand->insertArgument(strdup($1));
        }
    }
    ;

iomodifier_opt:
    iomodifier_opt GREAT WORD {
        printf("   Yacc: redirect output to \"%s\"\n", $3);
        Command::_currentCommand._outFile = $3;
    }
    | iomodifier_opt GREATGREAT WORD {
        printf("   Yacc: append output to \"%s\"\n", $3);
        Command::_currentCommand._outFile = $3;
        Command::_currentCommand._append = true;
    }
    | iomodifier_opt LESS WORD {
        printf("   Yacc: redirect input from \"%s\"\n", $3);
        Command::_currentCommand._inputFile = $3;
    }
    | /* empty */
    ;

background_opt:
    AMPERSAND {
        Command::_currentCommand._background = 1;
    }
    | /* empty */
    ;

%%

// Error handling
void yyerror(const char *s) {
    fprintf(stderr, "Error: %s\n", s);
}

#if 0
main()
{
	yyparse();
}
#endif

