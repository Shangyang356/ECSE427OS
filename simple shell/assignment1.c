#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <fcntl.h>

int counter;//the number of cmd user has typed
int commandposition;//stored position(index) of current cmd in history array
int availableposition = 0;//the first available position that can store bg process information. initialize it to 0
int errorcode; //if a cmd produces an error then this one will be 1. pass it between parent and child is
                   //done by building a pipe.
int fd[2];//array used for pipe


struct history//define a struct which store the typed cmd and its number,background or not, erroreous or not.
{
    char *args[20];
    int errorflag ;
    int numofcmd;
    int ifbg;
}history[10];


struct bgjobinfo{ // define a struct which store a background process's cmd nama,
    char *name;   // its status, its processID, the cmd number and a boolean of available to store or not
    int status;
    pid_t processID;
    int numofcmd;
    int available;
}bgjobinfo[30];



void initbgjobinfo(){//initialize all bgjobinfos
    int i;
    for(i=0;i<30;i++){
        bgjobinfo[i].available = 1;
        bgjobinfo[i].status = -1;
        bgjobinfo[i].numofcmd = 0;

    }
}

void inithistory(){//initialize all history
    int i;
    for(i = 0;i<10;i++){
        history[i].errorflag =0;
        history[i].numofcmd =0;
        history[i].ifbg =0;
    }
}

void freecmd(char* args[]){//initialize the args
    int i;
    for(i = 0;i<20;i++)
    {
        if(args[i] != NULL)
        {
            args[i] = NULL;
        }
    }
}

void freehistory(int position){ //initialize one specific history's data
    history[position].errorflag =0;
    history[position].numofcmd =0;
    history[position].ifbg =0;
    freecmd(history[position].args);
}

int freejobs(){//clean the background jobs that has finished.
    int i;
    for(i=0;i<30;i++){
        bgjobinfo[i].status = waitpid(bgjobinfo[i].processID,NULL,WNOHANG);
        if(bgjobinfo[i].status !=0 && bgjobinfo[i].available ==0){
            bgjobinfo[i].available = 1;
            bgjobinfo[i].status = -1; 
        }
    }
    for(i=0;i<30;i++){
        if(bgjobinfo[i].available ==1){
            return i;//and return the first position in this array that can store information.
        }
    }
}

void displayhistorycmd(){// display the latest 10 cmds' information in screen
    int i,j;
    int numberofhistorycmd = 10;
    if(counter<10){
        numberofhistorycmd = counter;//if user has typed less than 10 cmds then will display the cmds (s)he has typed.
    }
    for(i = 0;i<numberofhistorycmd;i++){
        printf("No.%d cmd:",history[i].numofcmd);
        for(j =0;history[i].args[j] != NULL;j++){
            printf("%s ",history[i].args[j]);
        }
        if(history[i].ifbg ==1){
            printf(". worked in background,");
        }
        else if(history[i].ifbg == 0){
            printf(". worked in frontground");
        }

        if(history[i].errorflag == 1){
            printf(", and has produced one error!\n");
        }
        else{
            printf(", and no error produced!\n");
        }
    }
}

int getcmd(char *prompt, char *args[], int *background,int *flag,int *ifredirection,int *counter)
{
    int length, i = 0;
    char *token, *loc;
    char *line;
    size_t linecap = 0;
    int j;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {
        exit(-1);
    }
    *counter = *counter +1; //after get a valid cmd,add counter by 1
    //historycmd[*cmdnum] = line;
    // Check if background is specified..
    if ((loc =  index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else
        *background = 0;

    if ((loc =  index(line, '>')) != NULL) {//if cmd typed by user has ">" then ifredirection equals 1
        *ifredirection = 1;
        *loc = ' ';
    } else
        *ifredirection = 0;

    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (strlen(token) > 0)
            args[i++] = token;
    }
        args[i++] = '\0';
    flag[0] = strcmp(args[0],"exit");//if user type exit as first word then flag[0] becomes 0
    flag[1] = strcmp(args[0],"cd");  //if user type cd as first word then flag[1] becomes 0
    flag[2] = strcmp(args[0],"pwd"); //if user type pwd as first word then flag[2] becomes 0
    flag[3] = strcmp(args[0],"jobs");//if user type jobs as first word then flag[3] becomes 0
    flag[4] = strcmp(args[0],"fg");  //if user type fg as first word then flag[4] becomes 0
    flag[5] = strcmp(args[0],"history");//if user type history as first word then flag[5] becomes 0
    
    return i;
}

void strargs(int ifbg,int counter,char* args[]){//store cmd,coutner,background or not to current history 
   history[commandposition].numofcmd = counter;
   history[commandposition].ifbg= ifbg;
   int a;
   for(a=0;a<20;a++){
        history[commandposition].args[a]=args[a];
   }
}

int runhistory(int ifhistory,int *counter){ // if user type a number then run this function before creating a child process
    int runexec;//represent whether a child process to create.
    if(ifhistory>=*counter || ifhistory <(*counter -10)||ifhistory<1){ //if user type a number which is not the lastest 10 histories' numbers 
        printf("cannot find the command with this number in history\n");//then give user a notification
        freehistory(commandposition);
        *counter =*counter -1; //this operation wont be stored in history so next cmd will use the current counter's value
        runexec =0;    // and neednt fork a child process
    }
    else{
        int originindex = (ifhistory-1)%10; // transfer the number user typed to the index of the history stored
        history[commandposition].errorflag = history[originindex].errorflag;
        if(history[commandposition].errorflag == 1){//if that history produced an error
            printf("this command produced one error before!\n");
            freehistory(commandposition);//then this operation wont be stored
            *counter =*counter -1;// and next cmd will use the current counter
            runexec = 0;// and neednt fork a child process.
        }
        else {
            int ifbg = history[originindex].ifbg; // copy the information of chosen history to current one.
            freehistory(commandposition);
            strargs(history[originindex].ifbg,*counter,history[originindex].args);

            runexec = 1;//and need to fork a child process to excute the cmd
        }
    }
    return runexec;
    
}


void runbuildincmd(int *flagbuiltincmd){//run the builtin cmds
    if(flagbuiltincmd[0] == 0){ //run exit
        printf("exit the program !\n");
        exit(0);//kill the parent process and end the entire c program
    }
    if(flagbuiltincmd[1] == 0){
        chdir(history[commandposition].args[1]);//change directory to the path after cd
    }
    if(flagbuiltincmd[2] == 0){
        char* cwd;
        char buff[PATH_MAX + 1];

        cwd = getcwd( buff, PATH_MAX + 1 );//get the current work directory
        if( cwd != NULL ) {
            printf( "My working directory is %s.\n", cwd );//print it in screen.
        }
    }
    if(flagbuiltincmd[3] == 0){
    // show the list of background child processes.
        int i;
        int j =0;
        availableposition =freejobs(bgjobinfo);//remove all the background processes that has finished.
        for(i=0;i<30;i++){
            if(bgjobinfo[i].available == 0 &&bgjobinfo[i].status == 0){
                j++;
                printf("Command: %s:processID is %d and the status is %d\n",bgjobinfo[i].name,bgjobinfo[i].processID,bgjobinfo[i].status);
            }//if the information has stored in this position and status is zero(means running)
        }//then display the information of this background process.
        if(j==0){
            printf("there is no process running in background\n");
        }
    }
    if(flagbuiltincmd[4] == 0){
        int ID = atoi(history[commandposition].args[1]);//get the process ID user typed after fg
        if(ID == 0){//if user type not a number 
            printf("the process ID should be typied in a correct format!\n");
        }
        else{
            int fg = waitpid(ID,NULL,0);
            if (fg == -1){//if the number user typed is not a valid.
                printf("fail to make the process to frontground! make sure the process is still running!\n");
            }
        }
    }
    if(flagbuiltincmd[5] == 0){
        displayhistorycmd();//display all the latest history cmds' information
    }

}

void redirectioncmd(){ //if a ">" detected then run this function
    FILE *fp;
    char *newargs[2];
    newargs[0] = history[commandposition].args[0];
    newargs[1] = NULL;
    fp = freopen(history[commandposition].args[1], "a", stdout);//a means writable and readable
    //int ff = open(history[commandposition].args[1],O_RDWR|O_CREAT|O_APPEND|O_TRUNC,0666);
    write(fd[1], &errorcode, sizeof(errorcode));
    int e = execvp(newargs[0],newargs);
    //dup2(ff,1);
    //close(ff);
    if(e<0){
        read(fd[0], &errorcode, sizeof(errorcode));//remove the 0 
        errorcode = 1;     
        write(fd[1], &errorcode, sizeof(errorcode));//and write the 1 to parent process.
        exit(1); 
    }
    
    fclose(fp);  
    exit(1); 
}





int main()
{   
    char *args[20];
    int flagbuiltincmd[6];//flag of builtin cmd typed or not
    int bg;
    pid_t pid;
    int ifredirection;//type ">" or not
    int ifhistory;    // type a number or not
    int nohistoryerror;// if number typed is not valid or that history cmd produced error, it will become 0.

    counter = 0;
    initbgjobinfo();
    inithistory();//initialize everything before the shell starts.
    while (1) {
        nohistoryerror = 1;
        bg = 0;
        commandposition = counter%10;

        int cnt = getcmd("\n>>  ", args, &bg,flagbuiltincmd,&ifredirection,&counter);
        freehistory(commandposition);//before store args,clean the data used to store the current args.
        strargs(bg,counter,args);
        freecmd(args);//free the args 
        ifhistory = atoi(history[commandposition].args[0]);//if user type a number then it will be nonzero number.

        if (pipe(fd) == -1) {//build a pipe.
        exit(EXIT_FAILURE);
        }

        if(ifhistory != 0){
            nohistoryerror = runhistory(ifhistory,&counter);
        }


        int runexec = nohistoryerror && flagbuiltincmd[0]!=0 && flagbuiltincmd[1] != 0 && flagbuiltincmd[2] != 0 && flagbuiltincmd[3] != 0 && flagbuiltincmd[4] !=0 && flagbuiltincmd[5] !=0;
        //create a child process or not
        if (runexec){
            pid = fork();
            if(pid == 0){
            //child process will perform the command
                if(ifredirection ==0){// user didnt type a ">"
                    errorcode = 0;
                    write(fd[1], &errorcode, sizeof(errorcode));//trasfer 0 at errorcode to parent process.
                    int e = execvp(history[commandposition].args[0],history[commandposition].args);
                    if(e < 0){//if e <0 then one error occurs
                        printf("\nerror:exec failed!");
                        read(fd[0], &errorcode, sizeof(errorcode));//remove the 0 
                        errorcode = 1;     
                        write(fd[1], &errorcode, sizeof(errorcode));//and write the 1 to parent process.
                        exit(1);                     
                    }
                }
                else { //user type a ">"
                    redirectioncmd();
                }
            }            
            else{
                if(history[commandposition].ifbg == 1){//dont wait the child process and store its information to bgjobinfo array.
                    bgjobinfo[availableposition].processID = pid;
                    bgjobinfo[availableposition].numofcmd = counter;
                    bgjobinfo[availableposition].status = waitpid(pid,NULL,WNOHANG);
                    bgjobinfo[availableposition].available = 0;//this position of array has been used.
                    bgjobinfo[availableposition].name =strdup(history[commandposition].args[0]);
                }
                else if(history[commandposition].ifbg ==0){
                    close(fd[1]);//close the write end
                    waitpid(pid, NULL,0);//wait child process to finish
                    read(fd[0], &errorcode, sizeof(errorcode));//read the errorcode passed from child process.
                    history[commandposition].errorflag = errorcode;
                }
                

            }
    
        }
        else{
            runbuildincmd(flagbuiltincmd);//run builtin cmd.
        }
    }
}