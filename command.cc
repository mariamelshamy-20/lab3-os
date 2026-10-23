
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"
#include <errno.h>

static void findfiles(SimpleCommand *sc, const std::string &word) //takes command and a word from the token
{
    if (word.find('*') == std::string::npos &&
    word.find('?') == std::string::npos) 
    {
        sc->insertArgument(strdup(word.c_str())); //c_str coverts c++ to c string to use c functions
        return;
    }

    glob_t g; // variable of type glob_t to hold the results of the globbing operation
    int num = glob(word.c_str(), 0, NULL, &g);

    if (num == 0) // at least one file matched as glob returns 0 lama ykon fi match                           
    {
        for (size_t i = 0; i < g.gl_pathc; i++)
            sc->insertArgument(strdup(g.gl_pathv[i]));
    }
    else                                
    {
        sc->insertArgument(strdup(word.c_str()));
    }

    globfree(&g);
}

bool parse(std::vector<Token> &tokens)
{
    Command &cmd = Command::_currentCommand;
    SimpleCommand *currentsc = new SimpleCommand();  

    for (int i=0; i<(int)tokens.size(); i++)
    {
        TokenType type = tokens[i].type;

        if (type == TOKEN_COMMAND || type == TOKEN_ARGUMENT)
        {
            findfiles(currentsc, tokens[i].value);
        }

        else if (type == TOKEN_PIPE)
        {
            if (currentsc->_numberOfArguments == 0)   
            {
                fprintf(stderr, "a command has to be before the pipe (|)\n");
                cmd.insertSimpleCommand(currentsc);    
                return false;
            }
            cmd.insertSimpleCommand(currentsc);
            currentsc = new SimpleCommand();
        }

        else if (type == TOKEN_REDIRECT || type == TOKEN_APPEND ||
                 type == TOKEN_INPUT || type == TOKEN_ERROR ||
                 type == TOKEN_REDIRECT_AND_ERROR)
        {
            i++; 

            if (i >= (int)tokens.size() ||
                (tokens[i].type != TOKEN_COMMAND && tokens[i].type != TOKEN_ARGUMENT))
            {
                fprintf(stderr, "after redirection , filename is expected \n");
                cmd.insertSimpleCommand(currentsc);
                return false;
            }

            char *file = strdup(tokens[i].value.c_str());

            if (type==TOKEN_INPUT)
            {
                cmd._inputFile = file;
            }
            else if (type==TOKEN_ERROR)
            {
                cmd._errFile = file;
            }
            else   // >, >>, >>&
            {
                cmd._outFile = file;
                if (type == TOKEN_APPEND || type == TOKEN_REDIRECT_AND_ERROR)
                    cmd._append = 1;
                if (type == TOKEN_REDIRECT_AND_ERROR)
                    cmd._out_error = 1;
            }
        }

        else if (type == TOKEN_BACKGROUND)
        {
            if (tokens[i + 1].type != TOKEN_EOF)
            {
                fprintf(stderr, "'&' must be at the end of command\n");
                cmd.insertSimpleCommand(currentsc);
                return false;
            }
            cmd._background = 1;
        }

    }

    //save the last command if it has anything
    if (currentsc->_numberOfArguments == 0)
    {
        if (tokens.size() > 1)   
            fprintf(stderr, "Syntax error: missing command\n");
        cmd.insertSimpleCommand(currentsc); 
        return false;
    }

    cmd.insertSimpleCommand(currentsc);
    return true;
}



SimpleCommand::SimpleCommand() //constructor of simple command
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1) // lw el array malyana i double the capacity
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument; 
    _arguments[_numberOfArguments + 1] = NULL; // store null 3shan execvp bta5od nullterminated array 

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *)); 

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;      
    _out_error = 0;     
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i=0; i< _numberOfSimpleCommands; i++)
    {
        for (int j=0; j< _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        delete _simpleCommands[i];          // was free , cant use free with sth created w new
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
     _append = 0;      
    _out_error = 0;   
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _outFile : "default" // changed
           ,_background ? "YES" : "NO");
    printf("\n\n");
}
// Called automatically whenever a child process terminates
void logChild(pid_t pid)
{
    char msg[64];
    int len = snprintf(msg, sizeof(msg), "Child %d terminated\n", (int)pid);
    int fd = open("shell.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0)
    {
        write(fd, msg, len);
        close(fd);
    }
}
void childHandler(int sig)
{
    int saved_errno = errno; // the global error code
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        logChild(pid);
    errno = saved_errno;
}
void Command::execute()
{
   //  print();   //uncomment 

    int n= _numberOfSimpleCommands;
    char **first= _simpleCommands[0]->_arguments;

    if (n==1 && strcmp(first[0], "exit")==0)
    {
        printf("Good bye!!\n");
        clear();
        exit(0);
    }

    if (n==1 && strcmp(first[0], "cd")==0)
{
    const char *dir;

    if (first[1] != NULL)
        dir = first[1];
    else
        dir = getenv("HOME");

    if (dir == NULL || chdir(dir) != 0) // 0 when success
        perror("cd"); 

    clear();
    return;
}
    //creation of pipes
    std::vector<int> pipes(2 * (n-1));
    for (int i=0; i< n - 1; i++)
    {
        if (pipe(&pipes[2*i]) < 0) 
        {
            perror("pipe");
            clear();
            return;
        }
    }

    std::vector<pid_t> pids;
    for (int i = 0; i < n; i++)

    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            break;
        }
        if (pid==0) 
        {
            signal(SIGINT, SIG_DFL);
            if (i > 0)
            {
                dup2(pipes[2*(i - 1)], 0);
            }
            else if (_inputFile)
            {
                int fd = open(_inputFile, O_RDONLY);
                if (fd < 0) { perror(_inputFile); _exit(1); }
                dup2(fd, 0);
                close(fd);
            }
            if (i< n - 1)
            {
                dup2(pipes[2*i+1], 1);
            }
            else if (_outFile)
            {
                int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
                int fd = open(_outFile, flags, 0644);
                if (fd < 0) { perror(_outFile); _exit(1); }
                dup2(fd, 1);
                if (_out_error)
                    dup2(fd, 2);     
                close(fd);
            }
            if (i== n - 1 && _errFile)
            {
                int fd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(_errFile); _exit(1); }
                dup2(fd, 2);
                close(fd);
            }
            for (size_t k=0; k< pipes.size(); k++)
                close(pipes[k]);
            char **args = _simpleCommands[i]->_arguments;
            execvp(args[0], args);
            perror(args[0]);       
            _exit(1);
        }
        pids.push_back(pid);       
    }

    for (size_t k=0; k< pipes.size(); k++)
        close(pipes[k]);
    if (!_background)
    {
        for (size_t k=0; k< pids.size(); k++)
        {
            int st;
            pid_t r;
            while ((r= waitpid(pids[k], &st, 0)) < 0 && errno==EINTR)
                ;
            if (r > 0)
                logChild(r);
        }
    }
    clear();
}

void Command::prompt()
{
    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);

        if (!std::getline(std::cin, input))   //control d
        {
            printf("\n");
            break;
        }
        std::vector<Token> tokens = tokenize(input);
        if (parse(tokens))
            execute();   
        else
            clear(); 
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
        signal(SIGINT, SIG_IGN); //ctrl c ignoringg
    struct sigaction sa;
    sa.sa_handler = childHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  
    sigaction(SIGCHLD, &sa, NULL);
    Command::_currentCommand.prompt();
    return 0;
}
