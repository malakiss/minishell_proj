#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <glob.h>

#include "command.h"

SimpleCommand::SimpleCommand() {
    // Create available space for 5 arguments
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void handleSIGINT(int sig) {
    // Ignore Ctrl-C
    printf("\n");  // Move to a new line
    Command::_currentCommand.prompt();  // Redisplay prompt
}


void SimpleCommand::insertArgument(char *argument) {
    // Check for wildcard characters
    if (strchr(argument, '*') || strchr(argument, '?')) {
        glob_t glob_result;
        glob(argument, GLOB_TILDE, NULL, &glob_result); // GLOB_TILDE expands '~' to home dir

        // Insert matched files as arguments
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            if (_numberOfAvailableArguments == _numberOfArguments + 1) {
                // Double available space
                _numberOfAvailableArguments *= 2;
                _arguments = (char **)realloc(_arguments, _numberOfAvailableArguments * sizeof(char *));
            }
            // Duplicate each matched file name and insert
            _arguments[_numberOfArguments++] = strdup(glob_result.gl_pathv[i]);
        }
        
        globfree(&glob_result); // Clean up glob result

    } else { // No wildcards, insert argument as is
        if (_numberOfAvailableArguments == _numberOfArguments + 1) {
            _numberOfAvailableArguments *= 2;
            _arguments = (char **)realloc(_arguments, _numberOfAvailableArguments * sizeof(char *));
        }
        _arguments[_numberOfArguments++] = argument;
    }

    // Ensure NULL termination for execvp
    _arguments[_numberOfArguments] = NULL;
}

Command::Command() {
    // Create available space for one simple command
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = nullptr;
    _inputFile = nullptr;
    _errFile = nullptr;
    _background = 0;
}

void Command::insertModifier(const char* modifierType, const char* fileName) {
    if (_numberOfModifiers < MAX_MODIFIERS) {
        _modifiers[_numberOfModifiers].type = strdup(modifierType);
        _modifiers[_numberOfModifiers].fileName = strdup(fileName);
        _numberOfModifiers++;
    } else {
        fprintf(stderr, "Maximum number of modifiers reached.\n");
    }
}



void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands) {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **) realloc(_simpleCommands, _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
        if (_simpleCommands == NULL) {
            perror("realloc");
            exit(1);
        }
    }
    
    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}


void Command::clear() {
    for (int i = 0; i < _numberOfSimpleCommands; i++) {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++) {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile) {
        free(_outFile);
    }

    if (_inputFile) {
        free(_inputFile);
    }

    if (_errFile) {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = nullptr;
    _inputFile = nullptr;
    _errFile = nullptr;
    _background = 0;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++) {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++) {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n");
    }

    printf("\n\n");
    printf("  Output       Input        Error        Background\n");
    printf("  ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s\n",
           _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default",
           _errFile ? _errFile : "default",
           _background ? "YES" : "NO");
    printf("\n\n");
}
void Command::changeDirectory(const char *dir) {
    if (dir == nullptr) {
        dir = getenv("HOME");  // Use HOME if no directory specified
    }

    if (chdir(dir) != 0) {
        perror("cd failed");  // Print error if directory change fails
    }
}

void Command::execute() {
    if (_numberOfSimpleCommands == 0) {
        prompt();
        return;
    }
  if (_numberOfSimpleCommands == 1 && strcmp(_simpleCommands[0]->_arguments[0], "cd") == 0) {
        const char *dir = (_simpleCommands[0]->_numberOfArguments > 1) ? _simpleCommands[0]->_arguments[1] : nullptr;
        changeDirectory(dir);  // Call changeDirectory with specified dir or HOME
        clear();
        prompt();
        return;
    }

    print();  // Print the command table
    
    // Create pipes for inter-process communication
    int pipefds[2 * (_numberOfSimpleCommands - 1)];
    for (int i = 0; i < _numberOfSimpleCommands - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    for (int i = 0; i < _numberOfSimpleCommands; i++) {
        pid_t pid = fork();

        if (pid == 0) {  // Child process
            // Input redirection for the first command
            if (i == 0 && _inputFile) {
                int inputFd = open(_inputFile, O_RDONLY);
                if (inputFd < 0) {
                    perror("open input file");
                    exit(1);
                }
                dup2(inputFd, 0);
                close(inputFd);
            }

            // Output redirection for the last command
            if (i == _numberOfSimpleCommands - 1 && _outFile) {
                int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
                int outputFd = open(_outFile, flags, 0666);
                if (outputFd < 0) {
                    perror("open output file");
                    exit(1);
                }
                dup2(outputFd, 1);
                close(outputFd);
            }

            // Set up pipes for intermediate commands
            if (i < _numberOfSimpleCommands - 1) {
                dup2(pipefds[i * 2 + 1], 1);  // stdout to pipe
            }

            if (i > 0) {
                dup2(pipefds[(i - 1) * 2], 0);  // stdin from previous pipe
            }

            // Close all pipes in the child process
            for (int j = 0; j < 2 * (_numberOfSimpleCommands - 1); j++) {
                close(pipefds[j]);
            }

            // Execute the command
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            perror("execvp");  // Print error if execvp fails
            exit(1);
        } else if (pid < 0) {
            perror("fork");  // Print error if fork fails
            exit(1);
        }
    }

    // Close all pipes in the parent process
    for (int i = 0; i < 2 * (_numberOfSimpleCommands - 1); i++) {
        close(pipefds[i]);
    }

    // Wait for child processes to complete if not in background mode
    if (!_background) {
        for (int i = 0; i < _numberOfSimpleCommands; i++) {
            waitpid(-1, NULL, 0);
        }
    }

    clear();  // Clear commands after execution
    prompt(); // Display prompt for next command
}

void Command::prompt() {
    printf("myshell> ");
    fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand = nullptr;

int yyparse(void);
void logChildTermination(int sig) {
    int logFile = open("child_termination.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (logFile == -1) {
        perror("Failed to open log file");
        return;
    }

    // Loop to handle all terminated children
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        char buffer[256];
        if (WIFEXITED(status)) {
            snprintf(buffer, sizeof(buffer), "Child %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            snprintf(buffer, sizeof(buffer), "Child %d killed by signal %d\n", pid, WTERMSIG(status));
        } else {
            snprintf(buffer, sizeof(buffer), "Child %d terminated unexpectedly\n", pid);
        }

        // Write the log to the file
        write(logFile, buffer, strlen(buffer));
    }

    close(logFile);
}

int main() {
    signal(SIGINT, handleSIGINT);

    // Set up SIGCHLD handler to log child terminations
    struct sigaction sa;
    sa.sa_handler = logChildTermination;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    } Command::_currentCommand.prompt();
    yyparse();
    return 0;
}

