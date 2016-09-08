This C program that implements a shell interface that accepts user commands and executes each command in a separate process.This shell program provides a command prompt, where the user inputs a line of command. 
The shell is responsible for executing the command. The shell program assumes that the first string of the line gives the name of
the executable file. The rest of the strings in the line are considered as arguments for the command.
Built-in Commands:
The history command is a built-in command because the functionality is completely built into
your shell. On the other hand, the process forking mechanism was used to execute outside commands.
In addition to the history command, implement the cd (change directory), pwd (present working
directory), and exit (leave shell) commands. The cd command could be implemented using the
chdir() system call, the pwd could be implemented using the getcwd() library routine, and the
exit command is necessary to quit the shell. Other built-in commands to be implemented include fg
and jobs. The command jobs should list all the jobs that are running in the background at any given
time. These are jobs that are put into the background by giving the command with & as the last one in
the command line. Each line in the list provided by the jobs should have a number identifier that can be
used by the fg command to bring the job to the foreground. 
Simple Output Redirection:
The last feature to implement is the output redirection. If you type
ls > out.txt
The output of the ls command should be sent to the out.txt file. See the class notes on how to
implement this feature.
