#include <arpa/inet.h>
#include <sys/time.h> 
#include <sys/ioctl.h> 

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <fstream>

#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <algorithm>
  
#define FILLED 0
#define Ready 1
#define NotReady -1

#define MAXNAME 1024

#define MAX_USER 31
#define MAX_MESSAGE_NUMBER 9
#define MAX_MESSAGE_SIZE 1024
#define MAX_LINE_LENGTH 16000
#define MAX_NAME_SIZE 20

using namespace std;

int server_sockfd;   /* file description into transport */
int client_sockfd; /* file descriptor to accept        */

enum BroadcastSign
{
    YELL,
    ENTER,
    LOGOUT,
    NAME,
    SEND,
    RECV,
};
struct user_information{
    int id;
    char nickname[MAX_NAME_SIZE];
    sockaddr_in address;
    int socketfd;
    pid_t pid;
    char message[MAX_USER][MAX_MESSAGE_NUMBER][MAX_MESSAGE_SIZE];
};
struct system_information{
    user_information users[MAX_USER];
    short fifo_check[MAX_USER][MAX_USER];
    int n;

    system_information(void):n(0){}
};
struct record_future{
    int pipe_write;
    int pipe_read;
    int number;  //the number of process which read pipe(only one)
};
struct record_userpipe{
    int pipe_write;
    int pipe_read;
    int sender;
    int receiver;
};

system_information* user;
int id;
//vector<record_future> future; // record pipe which has reader in the maybe next line
vector<record_userpipe> userpipe;

int char_to_int(char* arr){   //x=-1 normal pipe , x=0 no pipe , x>0 number pipe(pipe x line)
    if(strlen(arr)==0) return -1;
    int d{1};
    size_t s{strlen(arr)};
    int ans{0};
    for(size_t i{0};i<s;i++){
        char tempp=arr[s-i-1];
        int temp=(int)tempp-48;
        ans=ans+temp*d;
        d=d*10;
    }
    return ans;
}
int find_id(void){
    int i{1};   //0 is for server client 
    while(i<MAX_USER){
        if(user->users[i].id==0) break;
        i++;
    }
    user->users[i].id=i;
    user->n++;  
    return i;
}

void fifoid(int sender_id,int receiver_id,char* temp){
    int i{18};
    strcpy(temp,"./user_pipe/myfifo");
    while(sender_id!=0 or receiver_id!=0){
        if(i%2==0){
            temp[i]=(char)((short)(sender_id%10+97));
            sender_id=sender_id/10;
        }
        else{
            temp[i]=(char)((short)(receiver_id%10+97));
            receiver_id=receiver_id/10;
        }
        i++;
    }
    temp[i]='\0';
    
//cout<<"fifoid in function:"<<temp<<endl;
}


//new
void broadcast(int id_,BroadcastSign sign,const char* msg){
    string temp = "";
    string input=msg;
    switch(sign) {
        case YELL:
            temp = "*** "+string(user->users[id].nickname)+" yelled ***: " + input ;
            break;
        case ENTER:
            temp = "*** User '(no name)' entered from "+string(inet_ntoa( user->users[id].address.sin_addr ))+":"+to_string(htons (user->users[id].address.sin_port))+". ***";
            break;
        case LOGOUT:
            temp = "*** User '"+string(user->users[id].nickname)+"' left. ***";
            break;
        case NAME:
            temp = "*** User from " +string(inet_ntoa( user->users[id].address.sin_addr ))+":"+to_string(htons (user->users[id].address.sin_port))+" is named '"+string(user->users[id].nickname)+"'. ***";
            break;
        case SEND:
            temp = "*** "+string(user->users[id].nickname)+" (#"+to_string(user->users[id].id)+") just piped '"+input+"' to "+string(user->users[id_].nickname)+" (#"+to_string(user->users[id_].id)+") ***";
            break;
        case RECV:
            temp = "*** "+string(user->users[id].nickname)+" (#"+to_string(user->users[id].id)+") just received from "+string(user->users[id_].nickname)+" (#"+to_string(user->users[id_].id)+") by '"+input+"' ***";
            break;
        default:
            break;
    }
    for(int i{1};i<MAX_USER;i++){
        if(user->users[i].id>0){
            for(int j{0};j<MAX_MESSAGE_NUMBER;j++){
                if(user->users[i].message[id][j][0]==0){
                    strncpy(user->users[i].message[id][j],temp.c_str(), MAX_MESSAGE_SIZE + 1);
                    kill(user->users[i].pid,SIGUSR1);
                    break;
                }
            }
        }
    }
}
int login(sockaddr_in address,int socketfd,pid_t pid){  //return id of user in system 
    id=find_id();
    user->users[id].id=id;
    strcpy(user->users[id].nickname,"(no name)");
    user->users[id].address=address;
    user->users[id].socketfd=socketfd;
    user->users[id].pid=pid;
    cout<<"****************************************"<<endl;
    cout<<"** Welcome to the information server. **"<<endl;
    cout<<"****************************************"<<endl;
    
    broadcast(0,ENTER,"");

    return id;
}
void logout(void){
    //broadcast
    user->users[id].id=0;
    user->n--;
    broadcast(0,LOGOUT,"");
}
void who(void){
    cout<<"<ID> <nickname>  <IP:port>   <indicate me>"<<endl;
    for(int i{1};i<31;i++){
        if(user->users[i].id>0){
            cout<<user->users[i].id<<"  "<<user->users[i].nickname<<"    "<<inet_ntoa( user->users[i].address.sin_addr )<<":"<<htons (user->users[i].address.sin_port);
            if(id==i) cout<<"  <-me"<<endl;
            else cout<<endl;
        }
    }
}
void tell(int receiver_id,char* msg){
    if(user->users[receiver_id].id>0){
        string message=msg;
        string temp="*** "+string(user->users[id].nickname)+" told you ***: "+message;
        for(int j{0};j<MAX_MESSAGE_NUMBER;j++){
            if(user->users[receiver_id].message[id][j][0]==0){
                strcpy(user->users[receiver_id].message[id][j],temp.c_str());
                kill(user->users[receiver_id].pid,SIGUSR1);
                j=MAX_MESSAGE_NUMBER;
            }
        }

    }
    else{
        cout<<"*** Error: user #"<< receiver_id<<" does not exist yet. ***"<<endl;
    }
}
void name(char* newname){
    bool if_same{false};
    for(int i{1};i<MAX_USER;i++){
        if(user->users[i].id>0){
            if(0==strncmp(user->users[i].nickname,newname,MAX_NAME_SIZE+1)){
                if_same=true;
            }
        }
    }
    if(!if_same){
        strcpy(user->users[id].nickname,newname);
        broadcast(0,NAME,"");
    }
    else{
        cout<<"*** User '"<<newname<<"' already exists. ***"<<endl;
    }       
}
//future pipe
bool get_futurepipe_read(int (&A)[2],vector<record_future> &future){
    for(int k{0};k<future.size();k++){ 
        if(0==future[k].number){   //have pipe already
            A[0]=future[k].pipe_read;   
            A[1]=future[k].pipe_write;
            future.erase(future.begin()+k);             
            return true;  
        }  
    }   
    return false;            
}
void add_futurepipe(int skip,int (&B)[2],vector<record_future> &future){   //future pipe is stored in B
    bool same_pipe_in_future{false};
    for(int k{0};k<future.size();k++){ 
        if(future[k].number==skip){
            B[0]=future[k].pipe_read;
            B[1]=future[k].pipe_write;
            same_pipe_in_future=true;
        }
    }
    if(!same_pipe_in_future){
        pipe(B);
        future.push_back(record_future{B[1],B[0],skip});
    }
}
//user pipe
bool add_userpipe(int receiver_id,int (&B)[2],char* line){
    int sender_id=id;
    if(user->users[receiver_id].id==0){   //user doesnt exit
        cout<<"*** Error: user #"<<receiver_id<<" does not exist yet. ***"<<endl;
        int devNull = open("/dev/null",O_WRONLY);
        B[0]=devNull;
        B[1]=devNull;
        return false;
    }
    if(user->fifo_check[sender_id][receiver_id]!=0){//pipe already exit
        cout<<"*** Error: the pipe #"<<sender_id<<"->#"<<receiver_id<<" already exists. ***"<<endl;
        int devNull = open("/dev/null",O_WRONLY);
        B[0]=devNull;
        B[1]=devNull;
        return false;
    }
    //add user_pipe
    user->fifo_check[sender_id][receiver_id]=2;
    broadcast(receiver_id,SEND,line);

    char temp[30];
    char* myfifo_=temp;
    fifoid(sender_id,receiver_id,myfifo_);
    mkfifo(myfifo_, 0666);
    kill(user->users[receiver_id].pid,SIGUSR2);
//cout<<"test1"<<"-"<<sender_id<<"-"<<receiver_id<<endl;
    int fd = open(myfifo_, O_WRONLY);
//cout<<"test2"<<endl;
    B[0]=fd;
    B[1]=fd;
    return true;
}
bool get_userpipe(int sender_id,int (&A)[2],char* line){
        if(user->users[sender_id].id==0){
            cout<<"*** Error: user #"<<sender_id<<" does not exist yet. ***"<<endl;
            int devNull = open("/dev/null",O_RDONLY);
            A[0]=devNull;
            A[1]=devNull;
            return false;   //sender doesnt exit
        }

        for(int i{0};i<userpipe.size();i++){
            if(userpipe[i].sender==sender_id and userpipe[i].receiver==id){
                broadcast(sender_id,RECV,line);
                A[0]=userpipe[i].pipe_read;
                A[1]=userpipe[i].pipe_write;
                int s=userpipe[i].sender;
                int r=userpipe[i].receiver;
                userpipe.erase(userpipe.begin()+i);
                user->fifo_check[s][r]=0;
                return true;
            }
        }

        
        cout<<"*** Error: the pipe #"<<sender_id<<"->#"<<id<<" does not exist yet. ***"<<endl;
        int devNull = open("/dev/null",O_RDONLY);
        A[0]=devNull;
        A[1]=devNull;
        return false;
    } 

void handler_sig(int sig){
    if (sig == SIGUSR1) {	/* receive messages from others */
//cout<<"receive signal"<<endl;
		for (int i = 1; i < MAX_USER;i++) {
			for (int j = 0; j < MAX_MESSAGE_NUMBER; j++) {
				if (user->users[id].message[i][j][0] != 0) {
//cout<<"test2:"<<user->users[i].message[id][j]<<endl;
					write (STDOUT_FILENO, user->users[id].message[i][j], strlen (user->users[id].message[i][j]));
					memset (user->users[id].message[i][j], 0, MAX_MESSAGE_SIZE);
                    cout<<endl;
				}
                
			}
		}
	} 
    else if (sig == SIGUSR2) {	/* open fifos to read from */
//cout<<"test3"<<endl;
		for(int i{1};i<MAX_USER;i++){
            if(user->fifo_check[i][id]==2){
                user->fifo_check[i][id]=1;
                char temp[30];
                char* myfifo_=temp;
                fifoid(i,id,myfifo_);
                mkfifo(myfifo_, 0666);
//cout<<"test3"<<"-"<<i<<"-"<<id<<endl;
                int fd = open(myfifo_, O_RDONLY);
//cout<<"test4"<<endl;
                userpipe.push_back(record_userpipe{fd,fd,i,id});
            }
        }
        char LINE[20000];
	} 
    else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
        shmdt(user);
        exit(0);
	}
}


int shell(void){    //return 0:close shell of user fd return 1:not thing
    char* com[4000];   //record comment
    char* file[4000][20]{0};
    int filenumber[4000]{0};
    bool err[4000];    //record does neet to pipe stderr    //////////////////////////////err
	int skip[4000];   //number of pipe to next n th line
	int line_t[4000];   //int this line,who is i th line's head
    int line_h[4000];
    int userpipe_in[4000];
    int userpipe_out[4000];
    pid_t pid_c;
    char buf[BUFSIZ];
    bool if_userpipe{false};
    setenv("PATH", "bin:.", 1);
    signal(SIGCHLD, SIG_IGN);

    vector<record_future> future;

    while(1){
        char line[20000];
        memset(line, '\0', sizeof(line));

        write(client_sockfd,"% ",2);
        int read_size{0};
        if((read_size = recv(client_sockfd, line, 20000,0) <0)){
                if(errno == EINTR) continue;
                cerr<<"Fail to read cmdLine, errno: "<<errno<<endl;
                return -1;
        }
        string s=line;
        replace( s.begin(), s.end(), '\r', '\0');
        replace( s.begin(), s.end(), '\n', '\0');
        strcpy(line,s.c_str());

        char LINE[20000];
        strcpy(LINE,line);

        char* redirection_file;
        bool redirection_file_happen{false};
        int n{0};   //command number
		int p{0};   //line number
        com[n]=NULL;

    //transfer line to com[]
        char* temp=strtok(line," ");

        while(temp!=NULL){
            filenumber[n]=0;
            com[n]=temp;
            userpipe_in[n]=0;
            userpipe_out[n]=0;
            skip[n]=0;
            file[n][0]=0;
            err[n]=false;      ///////////////////////////err
            redirection_file=(char*)"NULL";
          
            if(0==strcmp(temp,(char*)"printenv")){
                char* val=strtok(NULL," ");
                char* argument=strtok(NULL," "); 
                char* iii=getenv(val);
                if(iii) cout<<iii<<endl;
                break;
            }
            if(0==strcmp(temp,(char*)"setenv")){
                char* val=strtok(NULL," ");
                char* argument=strtok(NULL," "); 
                setenv(val, argument, 1);
                break;
            }
            if(0==strcmp(temp,(char*)"exit")){
                return 0;
            }
            if(0==strcmp(temp,(char*)"who")){
                who();
                break;
            }
            if(0==strcmp(temp,(char*)"tell")){
                int receiver_id=char_to_int(strtok(NULL," "));
                char* message=strtok(NULL,"\0"); 
                tell(receiver_id,message);
                break;
            }
            if(0==strcmp(temp,(char*)"yell")){
                char* message=strtok(NULL,"\0");   ///////////////////////
                broadcast(0,YELL,message);
                break;
            }
            if(0==strcmp(temp,(char*)"name")){
                char* name_=strtok(NULL," "); 
                name(name_);
                break;
            }
            temp=strtok(NULL," ");
            int others_information{1};
            while(1){
                if(temp==NULL){    //just has com no other information(after pipe)
                    break;
                }
                else if(temp[0]=='|'){
                    char* k=temp+1;
                    skip[n]=char_to_int(k);
                    break;
                }
                else if(temp[0]=='!'){
                    char* k=temp+1;
                    skip[n]=char_to_int(k);
                    err[n]=true;
                    break;
                }
                else if(temp[0]=='>' and temp[1]=='\0'){  //output to file
                    redirection_file_happen=true;
                    redirection_file=strtok(NULL," ");
                    break;
                }
                else if(temp[0]=='<'){  //input from user pipe !!!
                    char* k=temp+1;
                    userpipe_in[n]=char_to_int(k);
                }
                else if(temp[0]=='>' and temp[1]!='\0'){  //output to user pipe !!!
                    char* k=temp+1;
                    userpipe_out[n]=char_to_int(k);
                }
                else{
                    file[n][others_information]=temp;
                    filenumber[n]++;
                    others_information++;
                }
                temp=strtok(NULL," ");
            }
            temp=strtok(NULL," ");
            n++;
        }
        int A[2]{0}; //front pipe
        int B[2]{0}; //rear pipe

        if(com[0]!=NULL){
            line_h[0]=0;
            for(int i{0};i<n;i++){
                if(skip[i]>0){
                    line_t[p]=i;
                    if((i+1)<n) line_h[p+1]=i+1;
                    p++;
                }
            }
            if(!(skip[n-1]>0)) p++;
            line_t[p-1]=n-1;


            //build child process(commend)
            for(int j{0};j<p;j++){
                for (size_t k{0}; k < future.size(); k++){
                    future[k].number--;
                }
                for(int i{line_h[j]};i<(line_t[j]+1);i++){
                    bool if_suc_userpipe_a{true};
                    bool if_suc_userpipe_b{true};

                    //head
                    int have_pipe_already{false};
                    if(i==line_h[j]){

                        have_pipe_already=get_futurepipe_read(A,future);

                        if(userpipe_in[i]>0) if_suc_userpipe_a=get_userpipe(userpipe_in[i],A,LINE);

                        if(line_t[j]!=line_h[j]){
                            pipe(B);
                        }
                    }
                    //middle
                    if(i!=line_h[j] and i!=line_t[j]){

                        A[1]=B[1];
                        A[0]=B[0];

                        pipe(B);
                    }
                    //tail
                    if(i==line_t[j]){
                        if(line_t[j]!=line_h[j]){
                            A[0]=B[0];
                            A[1]=B[1];
                        }
                        if(skip[i]>0) add_futurepipe(skip[i],B,future);
                        else if(userpipe_out[i]>0) if_suc_userpipe_b=add_userpipe(userpipe_out[i],B,LINE);
                    }
//cout<<"A[0]"<<A[0]<<endl;
//cout<<"A[1]"<<A[1]<<endl;
//cout<<"B[0]"<<B[0]<<endl;
//cout<<"B[1]"<<B[1]<<endl;
                    pid_c=fork();

                    if(pid_c>0){
                        //close pipe; 
                        if((have_pipe_already and i==line_h[j])or(i!=line_h[j])or(userpipe_in[i]>0)){
                            close(A[1]);
                            close(A[0]);
                        }
                        int status;
                        if(i==(n-1) and skip[i]==0 and userpipe_out[i]==0){
                            waitpid(pid_c,&status,0);                     
                        }
                        if(i==(n-1) and userpipe_out[i]>0){
                            close(B[0]);   
                            close(B[1]);                 
                        }
                        usleep(100);
                    }

                    //child
                    else if(pid_c==0){
                        if(!strcmp(com[i],(char*)"printenv")) exit(1);
                        if(!strcmp(com[i],(char*)"setenv")) exit(1);
                        if(!strcmp(com[i],(char*)"exit")) exit(1);
                        if(!strcmp(com[i],(char*)"who")) exit(1);
                        if(!strcmp(com[i],(char*)"tell")) exit(1);
                        if(!strcmp(com[i],(char*)"yell")) exit(1);
                        if(!strcmp(com[i],(char*)"name")) exit(1);
                        //single process server,so if use "exit()",entire server be closed

                        if((have_pipe_already and i==line_h[j])or(i!=line_h[j])or(userpipe_in[i]>0)){
                            dup2(A[0],STDIN_FILENO);
                            close(A[1]);
                            close(A[0]);
                        }
                        if((i!=line_t[j])or(skip[i]>0)or(userpipe_out[i]>0)){
                            dup2(B[1],STDOUT_FILENO);
                            if( err[i] ) dup2(B[1],STDERR_FILENO);
                            close(B[0]);
                            close(B[1]);
                        }
                        //execpl() file 
                        if((i==(n-1)) and(redirection_file_happen)){  /////////////////////
                            std::ofstream outfile (redirection_file);
                            dup2(open(redirection_file,O_WRONLY),STDOUT_FILENO);
                        }
                        if(filenumber[i]>=0){
                            char* arg[filenumber[i]+2];
                            arg[0]=com[i];
                            arg[filenumber[i]+1]=NULL;
                            for(int k{1};k<(filenumber[i]+1);k++){
                                arg[k]=file[i][k];
                            }
                            if(execvp(com[i],arg)) cerr<<"Unknown command: ["<<com[i]<<"]."<<endl;
                        }
                        else{
                            cerr<<"Unknown command: ["<<com[i]<<"]."<<endl;
                        }
                        exit(1);
                    }
                }
            }
        }   
    }
    return 1;
}


int main(int argc, char* const argv[])
{   

    if(argc > 2)
    {
        cerr<<"Argument set error"<<endl;
        exit(1);
    }

    int SERV_PORT = atoi(argv[1]);


    signal (SIGUSR1, handler_sig);	/* receive messages from others */
	signal (SIGUSR2, handler_sig);	/* open fifos to read from */
	signal (SIGINT, handler_sig);
	signal (SIGQUIT, handler_sig);
	signal (SIGTERM, handler_sig);

       int client_len;      /* length of address structure      */
       int nbytes;      /* the number of read **/
       char buf[BUFSIZ];


       struct sockaddr_in myaddr; /* address of this service */
       struct sockaddr_in client_address; /* address of client    */


       if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
             perror ("socket failed");
             exit(1);
       }



       bzero ((char *)&myaddr, sizeof(myaddr)); /* 清除位址內容 */
       myaddr.sin_family = AF_INET;    /* 設定協定格式 */
       myaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* IP次序轉換 */
       myaddr.sin_port = htons(SERV_PORT);  /* 埠口位元次序轉換 */


    int option = 1;
      if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &option , sizeof(int)) < 0){
            perror("setsockopt(SO_REUSEADDR) failed");
      }

    if (bind(server_sockfd, (sockaddr *)&myaddr, sizeof(myaddr)) <0) {
        perror ("bind failed");
        exit(1);
    }

    if (listen(server_sockfd, 100) <0) {
        perror ("listen failed");
        exit(1);
    }


       client_len = sizeof(client_address); 

       cout<<"Server is ready to receive !!"<<endl;
       cout<<"Can strike Cntrl-c to stop Server"<<endl;

       /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        int key = 12345;
        int shmid = shmget(key, sizeof(struct system_information), IPC_CREAT | 0666);
        if(shmid<0) cout<<"add memory fail"<<endl;
        user = (system_information*)shmat(shmid, NULL, 0);


        memset(user->users,0,sizeof(user->users));
        memset(user->fifo_check,0,sizeof(user->fifo_check));
       /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
        while (1) {
            if(user->n < 30){
            if ((client_sockfd = accept(server_sockfd, (sockaddr *)&client_address,(socklen_t*)&client_len))<0) {
                  perror ("could not accept call");
                  exit(1);
            }
        
            pid_t pid=fork();
            if(pid==0){
                close(server_sockfd);
                dup2(client_sockfd, STDIN_FILENO);  //server need to receive command from socket
                dup2(client_sockfd, STDOUT_FILENO);
                dup2(client_sockfd, STDERR_FILENO);

                pid=getpid();
                login(client_address,client_sockfd,pid);
                shell();
                logout();
                
                exit(0);
            }
            
            else{
                close(client_sockfd);
                usleep(100);
            }
            }
       }
       return 0;
}
