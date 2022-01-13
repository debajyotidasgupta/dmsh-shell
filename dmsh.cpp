/**************************************************
 *                                                *
 *                  {  DMSH  }                    *
 *                                                *
 *      A minimal shell built using the api       *
 *      provided by the standard linux kernel     *
 *                                                *
 *  AUTHORS:      GROUP - 23                      *
 *                                                *
 *            1. DEBAJYOTI DASGUPTA (18CS30051)   *
 *               debajyotidasgupta6@gmail.com     *
 *                                                *
 *            2. SHUBHAM MISHRA (18CS10066)       *
 *               smishra99.iitkgp@gmail.com       *
 *                                                *
 * LANGUAGE:  C++17                               *
 * COURSE:    OPERATING SYSTEMS LAB               *
 * YEAR:      2020-2021 SPRING                    *
 *                                                *
 *                 DISCLAIMER                     *
 *                -----------                     *
 *   Please note the following shell  has  been   *
 *   built solely for the  purpose of education.  *
 *   Any  misuse  of  any   part of the code is   *
 *   strictly prohibited. The following code is   *
 *   Open  Sourced  and  can  be  modified  and   *
 *   redistributed  solely  for  the purpose of   *
 *   education.                                   *
 *                                                *
 **************************************************/

#include <map>
#include <regex>
#include <cstdio>
#include <vector>
#include <string>
#include <csignal>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

/**
 * Structure of commands:
 * 
 * 1. COMMAND consists  of  BLOCKs separated by &&
 * 
 * 2. BLOCK consists of ATOMs separated by '|' and 
 *    may terminate with '&'
 * 
 * 3. ATOM consists of ([RUNTIME_ENV_VARS], PROGRAM, 
 *    [ARGs],  < INPUT_STREAM,  >/>>  OUTPUT_STREAM)
 * 
 *  >>> Example:
 *  >>>  cmd  abc   &&  A=1 ./a.out < b.txt | VARI='abc' execute cde > out.txt &
 *  >>> |--ATOM--|     |-------ATOM-------|   |-------------ATOM---------------|
 *  >>> |-BLOCK--|     |------------------BLOCK--------------------------------|
 *  >>> |---------------------------COMMAND------------------------------------|
 * 
 * 4. The COMMANDS be taken as a std::string  and
 *    will be utilizing the parsing utilities for
 *    the further processing of the string
 * 
 * 5. The final splitted block will be stored in
 *    Command structure
 */

/************************************
 *                                  *
 *       CONTAINER STRUCTURES       *
 *                                  *
 ************************************/

/**
 * Following is the most basic unit of the 
 * shell  program.  Atoms  consist  of the 
 * following components:
 * 
 * * [RUNTIME_ENV_VARS]
 * * PROGRAM
 * * [ARGs]
 * * < INPUT_STREAM
 * * >/>> OUTPUT_STREAM
 * 
 * Atoms dont contain | or &&
 * 
 * Output modes:
     * -------------
     * 0 -> Append
     * 1 -> Write
 */
struct Atom
{
    std::map<std::string, std::string> RuntimeVars;
    std::string Program;
    std::vector<std::string> Args;
    std::string InputStream;
    std::string OutputStream;
    bool OutputMode;
};

/**
 * Following  structure  contains block 
 * of  code  that  are  executed in one 
 * pass. That is the block will contain 
 * no && 
 * 
 * IsBackgroundProcess  ->  boolean
 *      tells whether the following 
 *      block  code  is running the 
 *      background or not
 */
struct Block
{
    std::vector<Atom *> Atoms;
    bool IsBackgroundProcess;
};

/**
 * Structure for containing the fully parsed 
 * command. The structure command is storing 
 * a collection of the block  statement  and 
 * is the top level structure in the  parsed 
 * hierarchy of commands.
 */
struct Command
{
    std::vector<Block *> Blocks;
};

/**********************************************
 *             GLOBAL TABLES                  *
 *            ---------------                 *
 * global_envp  = global environment varables *
 * running_jobs = all jobs that are currently *
 *                running                     *
 **********************************************/

std::vector<char *> global_envp;
std::vector<pid_t> running_jobs;
std::vector<Command *> queue;

/************************************
 *                                  *
 *            NAMESPACES            *
 *               AND                *
 *       FUNCTION DECLARATIONS      *
 *                                  *
 ************************************/

/**
 * The following namespace contains the definations
 * of the utility fuinctions that are utilized  for
 * helping in the execution of the commands and for 
 * signal handling.
 */
namespace utility
{
    std::string getPrompt();
    void signal_callback_handler(int);
    char **strToChrArr(std::string, const std::vector<std::string> &);
    char **constructEnvArr(std::map<std::string, std::string>);
} // namespace utility

/**
 * The  following  namespace contains the 
 * commands to execute the kernel bulitin 
 * commands
 */
namespace builtin
{
    int cd(std::vector<std::string> &);
    int info(std::vector<std::string> &);
    int help(std::vector<std::string> &);
    int exit(std::vector<std::string> &);
    int history(std::vector<std::string> &);
    int exportEnv(std::vector<std::string> &);
    std::map<std::string, int (*)(std::vector<std::string> &)> builtin_commands = {
        {"cd", &cd},
        {"exit", &exit},
        {"info", &info},
        {"help", &help},
        {"export", &exportEnv},
        {"history", &history}};

} // namespace builtin

/**
 * The  following  namespace contains the 
 * functions  that  are required for  the 
 * execution of the commands. Each of the 
 * functions  have  different tasks w.r.t 
 * the hierarchy of commands  where  they 
 * appear
 */
namespace executors
{
    int execSingleCmd(Atom *, bool);
    int execute_atom(Atom *, bool);
    int execute_block(Block *);
    int execute(Command *);
} // namespace executors

/**
 * The following namespace contains the function 
 * that  are  utilized  for parsing the commands.
 * The namespace also contains commands that are
 * utilized for the splitting of the strings and
 * trimming space related characters.
 * 
 * A TOP DOWN parser has been implemented for the 
 * parsing of the program. That is the parse tree
 * will be contstructed from the root down to the
 * leaves.
 */
namespace parser
{
    std::string trim(const std::string &trimStr);
    std::vector<std::string> splitString(const std::string &, const std::string &);
    Atom *getAtom(std::string &);
    Block *getBlock(std::string &);
    Command *Parse(const std::string &);
} // namespace parser

/**
 *               ----------------
 *               MAIN EVENT LOOP
 *              ----------------
 * 
 * Following function has the main event loop 
 * that  the terminals follows. the following 
 * states are processed in the event loop.
 * 
 *                PROCESS LIFE CYCLE
 *                    <-------
 *    IDLE ----> READY         RUN -------> TERMINATED
 *                  \  ------> /
 *                   \        /
 *                    \      /
 *                      WAIT
 * 
 * 1. We start by setting up the global  environment 
 *    variables. Followed by we initialize the event
 *    handler for SIGINT interrupt.
 * 
 * 2. Set up the prompt that willl appear showin the 
 *    current directory we are present
 * 
 * 3. within an infinite loop we take in command from
 *    the user in the form of std::string and send it
 *    to the parser for further processing
 * 
 * 4. The   parser  will  return  a  command  object. This 
 *    command is then send for execution to  the  function
 *    provided in the namespace executors for the handling 
 *    of the execution of this command object.
 * 
 * 5. Once the process if finished execution the loop repeats
 */

int main(int argc, char *argv[], char *envp[])
{
    std::string cmd;

    int i = 0;
    while (envp[i] != NULL)
        global_envp.push_back(envp[i++]);

    signal(SIGINT, utility::signal_callback_handler);
    setenv("PS1", "$ ", 0);
    // 0 --> Don't replace already existing value

    while (true)
    {
        std::cout << utility::getPrompt() << std::flush;
        std::getline(std::cin, cmd);

        Command *command = parser::Parse(cmd);
        if (cmd.find("history") == std::string::npos)
            queue.push_back(command);
        executors::execute(command);
    }
}

/******************************
 *                            *
 *   FUNCTION  DEFINATIONS    *
 *                            *
 ******************************/

/**
 * Following function is a signal handler for CTRL+C 
 * signal which raises a SIGINT interrupt. When this 
 * interrupt is raised all the running process  must 
 * be stopped and the normal execution of the  shell 
 * must be resumed.
 * 
 * For  the  we send a SIGTERM signal that is terminate 
 * the running program signal along with the pid of the 
 * job that needs to be killed.
 * 
 * Once all the running jobs are killed return to the 
 * normal execution of the shell.
 */

void utility::signal_callback_handler(int signum)
{
    int status;
    pid_t cpid;
    for (auto &proc : running_jobs)
    {
        kill(proc, SIGTERM);
        cpid = waitpid(proc, &status, 0);
    }
    std::cout << std::endl
              << "Stopped all processes!!" << std::endl;
    return;
}

/**
 * The following function is  used  to bring
 * up the display prompt  for  the  user, so
 * that the user is aware of the space where 
 * the command is to be entered. 
 * 
 * The prompt string will be concatenation of 
 * the value of  evironment  variable PS1 and 
 * the current working directory
 */

std::string utility::getPrompt()
{

    std::string ps1(getenv("PS1"));
    std::string user(getenv("USER"));
    std::string dir(getcwd(nullptr, 0));
    return "\033[1;32m" + user + ":" + "\033[1;31m" + dir + " " + ps1 + "\033[0m";
}

/**
 * The following function converts a vector
 * of  string  into  a  array  of  character 
 * pointers. this is particularly useful for
 * the functions provided by the kernel like 
 * execvpe that requre the arguments in  the
 * for of traditional char** 
 */
char **utility::strToChrArr(std::string prog, const std::vector<std::string> &args)
{
    char **Args = (char **)malloc((args.size() + 2) * sizeof(char *));
    Args[0] = (char *)prog.c_str();
    for (int i = 0; i < args.size(); ++i)
    {
        Args[i + 1] = (char *)args[i].c_str();
    }
    Args[args.size() + 1] = NULL;
    return Args;
}

/**
 * The  following funtion converts the
 * map of string, string that contains 
 * the  mapping  so   the  environment 
 * variables into  the  array  of  char
 * pointers so that it can be passed as
 * an argument for the kernel functions
 * that  take  environment variables in 
 * the traditional char**  format  like
 * >>> execvpe(_ , _ , char** envs)
 */
char **utility::constructEnvArr(std::map<std::string, std::string> env)
{
    char **envarr = new char *[global_envp.size() + env.size() + 1];
    int i = 0;
    for (auto s : global_envp)
    {
        envarr[i] = new char[strlen(s) + 1];
        sprintf(envarr[i], "%s", s);
        i++;
    }
    for (auto it : env)
    {
        if (it.first.length() < 1)
            continue;
        envarr[i] = new char[it.first.length() + it.second.length() + 2];
        sprintf(envarr[i++], "%s=%s", it.first.c_str(), it.second.c_str());
    }
    envarr[i] = NULL;
    return envarr;
}

/**
 * The following function handles  the 
 * execution of the builtin change dir
 * function   using  the  `chdir`  api
 * provided  by  the  kernel.
 *   
 * If  no  argument is provided it is 
 * assumed that the directry is to be 
 * changed  to  the `HOME`  directory 
 * of the user denoted by '~'.
 * 
 * Otherwise change to the directory
 * provided in the argument.
 */

int builtin::cd(std::vector<std::string> &args)
{
    int exec_status;
    if (args.empty())
    {
        std::string new_dir = "/home/" + std::string(getenv("USER"));
        exec_status = chdir(new_dir.c_str());
    }
    else if (args[0][0] == '~')
    {
        std::string new_dir = "/home/" + std::string(getenv("USER"));
        new_dir = new_dir + args[0].substr(1);
        exec_status = chdir(new_dir.c_str());
    }
    else
    {
        exec_status = chdir(args[0].c_str());
        if (exec_status)
            perror("cd");
    }
    return exec_status;
}

int builtin::help(std::vector<std::string> &args)
{
    std::cout << "\
    BUILTIN COMMANDS\n\
    ----------------\n\
    cd [OPTIONAL Path]             : change directory to the given path\n\
    exit                           : Exit from the shell. Stops all running processes\n\
    info                           : Info about the authors\n\
    export [CLAUSE] [OPTIONAL]     : Export environment variables\n\
    history [NUMBER]               : Execute N th from the last command"
              << std::endl;
    return 0;
}

/**
 * The following  function  is 
 * used to execute  the  [N]th 
 * command from the  last.  It 
 * pulls out the command  from
 * the command queue and  then
 * executes the program  using 
 * the command helper function
 */
int builtin::history(std::vector<std::string> &args)
{
    int n = atoi(args[0].c_str());
    if (n > queue.size())
        return 1;
    executors::execute(queue[queue.size() - n]);
    return 0;
}

/**
 * The  following  funtion  handles  the 
 * execution of exit command  using  the 
 * exit api provided by processes module
 * 
 * Before exiting from the dshell all  the
 * currently    running   process,    both
 * backgropund and the  foreground   needs 
 * to be terminated.  Following that  exit 
 * from the terminal and the  exit  status 
 * will be the error status (if any) while
 * terminating the running tasks
 */
int builtin::exit(std::vector<std::string> &args)
{
    int status;
    pid_t cpid;
    for (auto &proc : running_jobs)
    {
        kill(proc, SIGTERM);
        cpid = waitpid(proc, &status, 0);
    }
    std::exit(status);
    return status;
}
/**
 * The following  function  handles 
 * the export environment variables 
 * functionality of the shell. This 
 * function adds the variable  that
 * have  been  exported and adds it 
 * to the global variable list
 */
int builtin::exportEnv(std::vector<std::string> &args)
{
    try
    {
        std::string buff = args[0];
        for (int i = 1; i < args.size(); i++)
        {
            buff += args[i];
        }

        global_envp.push_back((char *)buff.c_str());
        std::cout << buff << std::endl;
    }
    catch (...)
    {
        perror("Invalid Command");
    }
    return 0;
}

/**
 * The following function is built just to 
 * provide information about the  projects 
 * and its authors
 */
int builtin::info(std::vector<std::string> &)
{
    std::cout << " **************************************************\n\
 *                                                *\n\
 *                  \033[0;33m{  DMSH  }\033[0m                    *\n\
 *                                                *\n\
 *      A minimal shell built using the api       *\n\
 *      provided by the standard linux kernel     *\n\
 *                                                *\n\
 *  \033[1;35mAUTHORS:\033[1;36m      GROUP - 23\033[0m                      *\n\
 *                                                *\n\
 *            \033[1;32m1. DEBAJYOTI DASGUPTA (18CS30051)   *\n\
 *               debajyotidasgupta6@gmail.com\033[0m     *\n\
 *                                                *\n\
 *            \033[1;32m2. SHUBHAM MISHRA (18CS10066)       *\n\
 *               smishra99.iitkgp@gmail.com\033[0m       *\n\
 *                                                *\n\
 * \033[1;35mLANGUAGE:  \033[0;33mC++17                \033[0m               *\n\
 * \033[1;35mCOURSE:    \033[0;33mOPERATING SYSTEMS LAB\033[0m               *\n\
 * \033[1;35mYEAR:      \033[0;33m2020-2021 SPRING     \033[0m               *\n\
 *                                                *\n\
 *                 \033[1;31mDISCLAIMER\033[0m                     *\n\
 *                -----------                     *\n\
 *   Please note the following shell  has  been   *\n\
 *   built solely for the  purpose of education.  *\n\
 *   Any  misuse  of  any   part of the code is   *\n\
 *   strictly prohibited. The following code is   *\n\
 *   Open  Sourced  and  can  be  modified  and   *\n\
 *   redistributed  solely  for  the purpose of   *\n\
 *   education.                                   *\n\
 *                                                *\n\
 **************************************************"
              << std::endl;
    return 0;
}

/**
 * The following function handles 
 * the execution of a single line
 * function that is stored a Atom 
 * object.
 * 
 * 1. Convert  the  args  to  char**
 * 2. Convert the env Vars to char**
 * 3. Fork to create a child process
 * 
 *    On succesful creation of the child
 *    process we get either 0 or the pid 
 *    of  the  child  process so created. 
 *    Wait teh child process  to  finish 
 *    execution.
 */
int executors::execSingleCmd(Atom *a, bool bg)
{
    char **Args = utility::strToChrArr(a->Program, a->Args);
    char **Envs = utility::constructEnvArr(a->RuntimeVars);

    if (strlen(Args[0]) == 0)
        return 0;

    int pid = fork(), status;
    if (pid == 0)
    {
        running_jobs.push_back(getpid());
        if (bg)
            setpgid(0, 0);
        status = execvpe(Args[0], Args, Envs);
        if (status == -1)
        {
            perror(strerror(errno));
        }
        return errno;
    }
    else
    {
        waitpid(pid, &status, 0);
    }
    return status;
}

/**
 * Following function is the main handler 
 * of any execution  related to statement 
 * stored  as  an  atom, that is the most 
 * basic command  in the shell.
 * 
 * For this we  find  whether  the  program 
 * is part of any of the predefined builtin
 * functions. If so then send the program to 
 * the builtin function handler for further 
 * execution.
 * 
 * Otherwise make use of the single command 
 * handler utility provided in the executor
 * namespace defined. 
 * 
 * Boolean type bg menstions whether the 
 * program  should   be  sent  into  the 
 * background for running
 */
int executors::execute_atom(Atom *a, bool bg)
{
    int exec_val;
    std::string cmd = a->Program;

    transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    if (builtin::builtin_commands.find(cmd) != builtin::builtin_commands.end())
    {
        return builtin::builtin_commands[cmd](a->Args);
    }
    else
    {
        exec_val = execSingleCmd(a, bg);
    }
    return exec_val;
}

/**
 * Following function is the main  function 
 * for handling the execution of the  block 
 * level statements. Block level statements
 * may contain pipes, so pipes are  handled
 * as a part of this code.
 * 
 * all the atom  commands  that  are  contained
 * the current block are executed  sequentially
 * in a loop. In the loop the pipes are handled
 * where output from prvious iteration is  sent
 * to the next iteration atom command execution.
 * 
 * For the last element of the loop no further
 * piping is required hence the input and out-
 * -put streams are set manually.
 * 
 * The execute_atom function handles the execution
 * of the atom commands. Finally restore the stdin 
 * file descriptor and the stdout file descriptor.  
 */
int executors::execute_block(Block *b)
{
    int stdin_copy = dup(STDIN_FILENO);
    int stdout_copy = dup(STDOUT_FILENO);

    int fdin;
    // Redirections are only taken from the last command of the block
    if (!b->Atoms[b->Atoms.size() - 1]->InputStream.empty())
    {
        fdin = open(b->Atoms[b->Atoms.size() - 1]->InputStream.c_str(), O_RDONLY);
    }
    else
    {
        fdin = dup(stdin_copy);
    }

    int fdout;

    for (int i = 0; i < b->Atoms.size(); i++)
    {
        dup2(fdin, STDIN_FILENO);
        close(fdin);

        if (i == b->Atoms.size() - 1)
        {
            if (b->Atoms[i]->OutputStream.length() > 0)
            {
                FILE *out;
                if (b->Atoms[i]->OutputMode == 1)
                {
                    out = fopen(b->Atoms[i]->OutputStream.c_str(), "w");
                }
                else
                {
                    out = fopen(b->Atoms[i]->OutputStream.c_str(), "a");
                }
                fdout = fileno(out);
            }
            else
            {
                fdout = dup(stdout_copy);
            }
        }
        else
        {
            int fdes[2];
            int res = pipe(fdes);
            if (res == -1)
            {
                perror("pipe");
                exit(-1);
            }
            fdout = fdes[1];
            fdin = fdes[0];
        }

        dup2(fdout, STDOUT_FILENO);
        close(fdout);

        execute_atom(b->Atoms[i], b->IsBackgroundProcess);
    }

    int status = 0;
    if (!b->IsBackgroundProcess)
        wait(&status);
    else
        std::cerr << "Command sent to background" << std::endl;

    dup2(stdin_copy, STDIN_FILENO);
    dup2(stdout_copy, STDOUT_FILENO);
    close(stdin_copy);
    close(stdout_copy);

    return status;
}

/**
 * Following function performs the  task  of 
 * handling  the  execution of each  of  the 
 * command block, which the largest unit  in
 * the parse tree. The function makes use of
 * the execute_block function  for  handling 
 * the execution of each block stored in it.
 */
int executors::execute(Command *c)
{
    int exec_val;
    for (const auto &blk : c->Blocks)
    {
        exec_val = execute_block(blk);
    }
    return exec_val;
}

/**
 * The following function helps in pre  processing
 * the string. This function is a utility function 
 * specifically for the purpose of  parsing . This 
 * function removes any white/space character from
 * the beginning and the end of  the  string  thus 
 * cleaning up the string  for  further processing
 */
std::string parser::trim(const std::string &trimStr)
{
    std::string str(trimStr);

    // trim spaces from the front
    std::reverse(str.begin(), str.end());
    while (isspace(str.back()))
        str.pop_back();

    // trim spaces from the end
    std::reverse(str.begin(), str.end());
    while (isspace(str.back()))
        str.pop_back();
    return str;
}

/**
 * Following funcion is a utility  function
 * specifically for the purpose of  parsing 
 * The function is used to split the string 
 * as per the delimeter passed as  the  arg
 * regexPattern. This function makes use of 
 * the in-built regex library.
 */
std::vector<std::string> parser::splitString(const std::string &stringToSplit, const std::string &regexPattern)
{
    std::vector<std::string> result;

    const std::regex rgx(regexPattern);

    // -1 refers to capture all the substrings generated after splitting
    std::sregex_token_iterator iter(stringToSplit.begin(), stringToSplit.end(), rgx, -1);

    for (std::sregex_token_iterator end; iter != end; ++iter)
    {
        result.push_back(trim(iter->str()));
    }

    return result;
}

/**
 * The following function is the  main function
 * responsible  for  parsing  the  atom command 
 * statement  that  is  sent  as a  std::string 
 * parameter  to the  function.  This  function 
 * makes use of regex expressions for capturing 
 * the different parts of the Atom object.
 * 
 * the regex experession captures the
 * following groups :-
 *  0  =>  Full string
 *  1  =>  Runtime vars
 *  2  =>  Program
 *  3  =>  Args
 *  4  =>  Redirects
 * 
 * With the help of the redirects the 
 * function  sets  the values for the 
 * Input Stream and the Output Stream
 * 
 * If redirect string length is 2,
 * then as per the regex used  the 
 * Output Mode will be  set  to  1 
 * which is the append mode.
 * 
 * The  arguments   and  the  redirect 
 * strings  that   were  captured  are 
 * further passed  though  the  argRgx 
 * and the redirRgx for capturing  the 
 * values. Finally  using  the  values
 * that were extracted using the regex
 * expressions are  used  to  set  the 
 * valus of the fields of atom object.
 */
Atom *parser::getAtom(std::string &cmd)
{
    Atom *atom = new Atom();
    atom->OutputStream = "";
    atom->InputStream = "";
    atom->OutputMode = 0;

    std::smatch matches;
    std::regex rgx("((?:[a-zA-Z0-9-_]+=(?:(?:\"[^\"]*\")|(?:\'[^\']*\')|(?:[^ \'\"]*)) )*)([a-zA-Z0-9-_./]+)( [^><]*)? *((?:<|>>|>).*)?");

    if (regex_search(cmd, matches, rgx))
    {
        atom->Program = trim(matches[2].str());
        atom->InputStream = "";
        atom->OutputStream = "";

        std::string vars = trim(matches[1].str());
        std::regex varRgx("(([a-zA-Z0-9-_]+)=((?:\"[^\"]*\")|(?:\'[^\']*\')|(?:[^ \'\"]*)))");
        std::smatch varMatch;
        while (std::regex_search(vars, varMatch, varRgx))
        {
            atom->RuntimeVars[varMatch[2].str()] = varMatch[3].str();
            vars = varMatch.suffix().str();
        }

        std::string args = matches[3].str();
        std::regex argRgx("((?:\"[^\"]*\")|(?:\'[^\']*\')|(?:[^ \'\"]+))");

        std::smatch argMatches;
        while (std::regex_search(args, argMatches, argRgx))
        {
            std::string s = argMatches[0].str();
            if (s.length() > 0 && (s[0] == '"' || s[0] == '\''))
            {
                s.erase(s.begin());
                s.erase(s.end() - 1);
            }
            atom->Args.push_back(s);
            args = argMatches.suffix().str();
        }

        std::string redirs = trim(matches[4].str());
        std::regex redirRgx("((?:<|>>|>) *[^ ><]*)");

        std::smatch redirMatches;
        while (std::regex_search(redirs, redirMatches, redirRgx))
        {
            std::string redirStr = redirMatches[0].str();
            if (redirStr[0] == '<')
            {
                redirStr.erase(redirStr.begin());
                redirStr = trim(redirStr);
                if (redirStr.length() > 0)
                    atom->InputStream = redirStr;
            }
            else
            {
                if (redirStr.length() >= 2)
                {
                    if (redirStr[1] == '>')
                    {
                        redirStr.erase(redirStr.begin());
                        atom->OutputMode = 0;
                    }
                    else
                    {
                        atom->OutputMode = 1;
                    }

                    redirStr.erase(redirStr.begin());
                    redirStr = trim(redirStr);
                    if (redirStr.length() > 0)
                        atom->OutputStream = redirStr;
                }
            }
            redirs = redirMatches.suffix().str();
            // Multiple matches will overwrite
        }
    }
    return atom;
}

/**
 * The following function is the main
 * function that handles the  parsing  
 * of the  block  statements that are 
 * passed as a std::string parameter.
 * 
 * getBlock make use of  the  getAtom 
 * utility  function  in  the  parser 
 * namespace that for further parsing
 * and creating of  the Atom  objects
 * 
 * The  IsBackgroundProcess  is a boolean
 * field which keeps track of whether the 
 * code is to be run in the background or 
 * not. 
 * 
 * The split string command is used
 * to split the  block  string into 
 * atom string. The  atom  string do 
 * not contain any  "|" hence we use
 * "|"  as  the  delimiter  for  the 
 * splitting of string. "|" -> PIPE
 */
Block *parser::getBlock(std::string &cmd)
{
    Block *blk = new Block();
    if (*(cmd.end() - 1) == '&')
    {
        // Background process
        blk->IsBackgroundProcess = true;
        cmd.erase(cmd.end() - 1);
    }
    else
    {
        blk->IsBackgroundProcess = false;
    }

    cmd = trim(cmd);

    auto singles = splitString(cmd, "\\|");
    for (auto s : singles)
    {
        blk->Atoms.push_back(getAtom(s));
    }

    return blk;
}

/**
 * The following function is the main
 * function that handles the  parsing  
 * of the command statements that are 
 * passed as a std::string parameter.
 * 
 * Parse  make use  of  the  getBlock 
 * utility  function  in  the  parser 
 * namespace that for further parsing
 * and creating of the block  objects
 * 
 * The split string command is used
 * to split the command string into 
 * block string. The block string do 
 * not contain any "&&" hence we use
 * "&&"  as  the  delimiter  for the 
 * splitting of string.
 */
Command *parser::Parse(const std::string &cmd)
{
    auto singles = splitString(cmd, "&&");
    Command *cmds = new Command();
    for (auto &s : singles)
    {
        cmds->Blocks.push_back(getBlock(s));
    }
    return cmds;
}
