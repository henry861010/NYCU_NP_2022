
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
#include <algorithm>

using namespace std;

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

class user_info{
private:
    struct future_record{
        int pipe_write;
        int pipe_read;
        int number;  //the number of process which read pipe(only one)
    };

    struct user_pipe_record{
        int pipe_write;
        int pipe_read;
        int sender;
        int receiver;
    };

    struct user_info_d{
        int id;
        char* nickname;
        char* env_path;
        sockaddr_in address;
        int socketfd;
    };

    user_info_d user[31];
    bool id_record[31];
    vector<user_pipe_record> user_pipe;      //to record the user pipe in the system
    vector<vector<future_record>> future_pipe;    //th record the number pipe in the system (2d array)
    int n;  // the number of clients in server

    int find_id(void){
        int i{1};   //0 is for server client 
        while(i<31){
            if(id_record[i]==0) break;
            i++;
        }
        return i;
    }
    int socketfd_to_id(int socketfd){
        for(int i{1};i<31;i++){
            if(socketfd==user[i].socketfd) return user[i].id;
        }
        return 0;
    }
public:
    int total_member(void){
        return n;
    }
    user_info() : future_pipe(31),n{0},id_record{0}{}
    //multiple user
    void broadcast(int socketfd,char* message){
        int id{socketfd_to_id(socketfd)};
        for(int i{1};i<31;i++){
            if(id_record[i]) {
                int origin{dup(STDOUT_FILENO)};
                dup2(user[i].socketfd, STDOUT_FILENO);
                cout<<"*** "<<user[id].nickname<<" yelled ***:"<<message<<endl;
                dup2(origin, STDOUT_FILENO);
                close(origin);
            }
        }
    }
    void tell(int socketfd_sender,int id_receiver,char* message){
        int id_sender{socketfd_to_id(socketfd_sender)};
        if(id_record[id_receiver]){
            int origin{dup(STDOUT_FILENO)};
            dup2(user[id_receiver].socketfd, STDOUT_FILENO);
            cout<<"*** "<<user[id_sender].nickname<<" told you ***: "<<message<<endl;
            dup2(origin, STDOUT_FILENO);
            close(origin);
        }
        else{
            int origin{dup(STDOUT_FILENO)};
            dup2(user[id_sender].socketfd, STDOUT_FILENO);
            cout<<"*** Error: user #"<< id_receiver<<" does not exist yet. ***"<<endl;
            dup2(origin, STDOUT_FILENO);
            close(origin);
        }
    }
    int login(sockaddr_in address,int socketfd){  //return id of user in system 
        int id=find_id();
        char* name = new char[500];
        strcpy(name,"(no name)");
        char* path = new char[500];
        strcpy(path,"bin:.");
        user[id]=user_info_d{id,name,path,address,socketfd};
        n++;
        id_record[id]=true;


        int origin{dup(STDOUT_FILENO)};
        dup2(socketfd, STDOUT_FILENO);
        cout<<"****************************************"<<endl;
        cout<<"** Welcome to the information server. **"<<endl;
        cout<<"****************************************"<<endl;
        dup2(origin, STDOUT_FILENO);
        close(origin);

        for(int i{1};i<31;i++){
            if(id_record[i]) {
                int origin{dup(STDOUT_FILENO)};
                dup2(user[i].socketfd, STDOUT_FILENO);
                cout<<"*** User '(no name)' entered from "<<inet_ntoa( user[id].address.sin_addr )<<":"<<htons (user[id].address.sin_port)<<". ***"<<endl; 
                dup2(origin, STDOUT_FILENO);
                close(origin);
            }
        }

        return id;
    }
    void logout(int socketfd){
        int id{socketfd_to_id(socketfd)};
        for(int i{1};i<31;i++){
            if(id_record[i] and i!=id) {
                int origin{dup(STDOUT_FILENO)};
                dup2(user[i].socketfd, STDOUT_FILENO);
                cout<<"*** User '"<<user[id].nickname<<"' left. ***"<<endl; 
                dup2(origin, STDOUT_FILENO);
                close(origin);
            }
        }
        delete user[id].env_path;
        delete user[id].nickname;

        n--;
        id_record[id]=0;
    }
    void who(int socketfd){
        int now{socketfd_to_id(socketfd)};
        int origin{dup(STDOUT_FILENO)};
        dup2(socketfd, STDOUT_FILENO);

        cout<<"<ID> <nickname>  <IP:port>   <indicate me>"<<endl;
        for(int i{1};i<31;i++){
            if(id_record[i]){
                cout<<user[i].id<<"  "<<user[i].nickname<<"    "<<inet_ntoa( user[i].address.sin_addr )<<":"<<htons (user[i].address.sin_port);
                if(now==i) cout<<"  <-me"<<endl;
                else cout<<endl;
            }
        }

        dup2(origin, STDOUT_FILENO);
        close(origin);
    }
    void name(int socketfd,char* newname){
        int id{socketfd_to_id(socketfd)};
        bool if_same{false};
        for(int i{1};i<31;i++){
            if(id_record[i]){
                if(0==strcmp(user[i].nickname,newname)){
                    if_same=true;
                }
            }
        }
        if(!if_same){
            strcpy(user[id].nickname,newname);
            for(int i{1};i<31;i++){
                if(id_record[i]) {
                    int origin{dup(STDOUT_FILENO)};
                    dup2(user[i].socketfd, STDOUT_FILENO);
                    cout<<"*** User from "<<inet_ntoa( user[id].address.sin_addr )<<":"<<htons (user[id].address.sin_port)<<" is named '"<<newname<<"'. ***"<<endl; 
                    dup2(origin, STDOUT_FILENO);
                    close(origin);
                }
            }
        }
        else{
            int origin{dup(STDOUT_FILENO)};
            dup2(user[id].socketfd, STDOUT_FILENO);
            cout<<"*** User '"<<newname<<"' already exists. ***"<<endl;
            dup2(origin, STDOUT_FILENO);
            close(origin);
        }
        
    }
    //env path
    void get_path(int socketfd){
        int id{socketfd_to_id(socketfd)};
        setenv("PATH",user[id].env_path,1);
    }
    void change_env(int socketfd,char* val,char* newpath){
        int id=socketfd_to_id(socketfd);
        strcpy(user[id].env_path,newpath);
        setenv(val,newpath,1);
    }
    //user pipe operator
    bool add_userpipe(int sender_sockfd,int receiver_id,char* line,int (&B)[2]){
        int sender_id=socketfd_to_id(sender_sockfd);
        if(!id_record[receiver_id]){   //user doesnt exit
            int origin{dup(STDOUT_FILENO)};
            dup2(user[sender_id].socketfd, STDOUT_FILENO);
            cout<<"*** Error: user #"<<receiver_id<<" does not exist yet. ***"<<endl;
            dup2(origin, STDOUT_FILENO);
            close(origin);
            int devNull = open("/dev/null",O_WRONLY);
            B[0]=devNull;
            B[1]=devNull;
            return false;
        }
        for(int i{0};i<user_pipe.size();i++){   //pipe already exit
            if(user_pipe[i].sender==sender_id and user_pipe[i].receiver==receiver_id){
                int origin{dup(STDOUT_FILENO)};
                dup2(user[sender_id].socketfd, STDOUT_FILENO);
                cout<<"*** Error: the pipe #"<<sender_id<<"->#"<<receiver_id<<" already exists. ***"<<endl;
                dup2(origin, STDOUT_FILENO);
                close(origin);
                int devNull = open("/dev/null",O_WRONLY);
                B[0]=devNull;
                B[1]=devNull;
                return false;
            }
        }
        //add user_pipe
        for(int i{1};i<31;i++){
            if(id_record[i]) {
                int origin{dup(STDOUT_FILENO)};
                    dup2(user[i].socketfd, STDOUT_FILENO);
                    cout<<"*** "<<user[sender_id].nickname<<" (#"<<sender_id<<") just piped '"<<line<<"' to "<<user[receiver_id].nickname<<" (#"<<receiver_id<<") ***"<<endl;
                    dup2(origin, STDOUT_FILENO);
                    close(origin);
            }
        }
        pipe(B);
        user_pipe.push_back(user_pipe_record{B[1],B[0],sender_id,receiver_id,});
        return true;
    }
    bool get_userpipe_read(int receiver_sockfd,int sender_id,int (&A)[2],char* line){
        int receiver_id=socketfd_to_id(receiver_sockfd);
        if(!id_record[sender_id]){
            int origin{dup(STDOUT_FILENO)};
            dup2(user[receiver_id].socketfd, STDOUT_FILENO);
            cout<<"*** Error: user #"<<sender_id<<" does not exist yet. ***"<<endl;
            dup2(origin, STDOUT_FILENO);
            close(origin);
            int devNull = open("/dev/null",O_RDONLY);
            A[0]=devNull;
            A[1]=devNull;
            return false;   //sender doesnt exit
        }
        for(int i{0};i<user_pipe.size();i++){
            if(user_pipe[i].sender==sender_id and user_pipe[i].receiver==receiver_id){
                for(int j{1};j<31;j++){  //broadcast about successful receiver
                    if(id_record[j]) {
                        int origin{dup(STDOUT_FILENO)};
                        dup2(user[j].socketfd, STDOUT_FILENO);
                        cout<<"*** "<<user[receiver_id].nickname<<" (#"<<receiver_id<<") just received from "<<user[sender_id].nickname<<" (#"<<sender_id<<") by '"<<line<<"' ***"<<endl;
                        dup2(origin, STDOUT_FILENO);
                        close(origin);
                    }
                }
                A[0]=user_pipe[i].pipe_read;
                A[1]=user_pipe[i].pipe_write;
                user_pipe.erase(user_pipe.begin()+i);
                return true;
            }
        }
        int origin{dup(STDOUT_FILENO)};
        dup2(user[receiver_id].socketfd, STDOUT_FILENO);
        cout<<"*** Error: the pipe #"<<sender_id<<"->#"<<receiver_id<<" does not exist yet. ***"<<endl;
        dup2(origin, STDOUT_FILENO);  
        close(origin); 
        int devNull = open("/dev/null",O_RDONLY);
        A[0]=devNull;
        A[1]=devNull;
        return false;
    } 
    //future pipe operator
    void future_pipe_number_subone(int socketfd){
        int id=socketfd_to_id(socketfd);
        int l=future_pipe[id].size();
        for(int i{0};i<l;i++){
            future_pipe[id][i].number=future_pipe[id][i].number-1;
        }
    }
    bool get_futurepipe_read(int socketfd,int (&A)[2]){
        int id=socketfd_to_id(socketfd);
        for(int k{0};k<future_pipe[id].size();k++){ 
            if(0==future_pipe[id][k].number){   //have pipe already
                A[0]=future_pipe[id][k].pipe_read;   
                A[1]=future_pipe[id][k].pipe_write;
                future_pipe[id].erase(future_pipe[id].begin()+k);               
                return true;                                
            }  
        }   
        return false;            
    }
    void add_futurepipe(int socketfd,int skip,int (&B)[2]){   //future pipe is stored in B
        int id=socketfd_to_id(socketfd);
        bool same_pipe_in_future{false};
        for(int k{0};k<future_pipe[id].size();k++){ 
            if(future_pipe[id][k].number==skip){
                B[0]=future_pipe[id][k].pipe_read;
                B[1]=future_pipe[id][k].pipe_write;
                same_pipe_in_future=true;
            }
        }
        if(!same_pipe_in_future){
            pipe(B);
            future_pipe[id].push_back(future_record{B[1],B[0],skip});
        }
    }
};

int shell(int fd,user_info &user){    //return 0:close shell of user fd return 1:not thing
    char* com[4000];   //record comment
    char* file[4000][20]{0};
    int filenumber[4000]{0};
    bool err[4000];    //record does neet to pipe stderr    //////////////////////////////err
	int skip[4000];   //number of pipe to next n th line
	int line_t[4000];   //int this line,who is i th line's head
    int line_h[4000];
    int userpipe_in[4000];
    int userpipe_out[4000];
    pid_t pid;
    char buf[BUFSIZ];
    bool if_userpipe{false};

        char line[20000];
        memset(line, '\0', sizeof(line));

        int read_size{0};
        if((read_size = recv(fd, line, 20000,0) <0)){
                cerr<<"Fail to read cmdLine, errno: "<<errno<<endl;
                return -1;
        }
        string s=line;
        replace( s.begin(), s.end(), '\r', '\0');
        replace( s.begin(), s.end(), '\n', '\0');
        strcpy(line,s.c_str());

//cout<<"["<<line<<"]"<<"-"<<strlen(line)<<endl;

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
                user.change_env(fd,val,argument);
                break;
            }
            if(0==strcmp(temp,(char*)"exit")){
                user.logout(fd);
                return 0;
            }
            if(0==strcmp(temp,(char*)"who")){
                user.who(fd);
                break;
            }
            if(0==strcmp(temp,(char*)"tell")){
                int receiver_id=char_to_int(strtok(NULL," "));
                char* message=strtok(NULL,"\0"); 
                user.tell(fd,receiver_id,message);
                break;
            }
            if(0==strcmp(temp,(char*)"yell")){
                char* message=strtok(NULL,"\0");   ///////////////////////
                user.broadcast(fd,message);
                break;
            }
            if(0==strcmp(temp,(char*)"name")){
                char* name=strtok(NULL," "); 
                user.name(fd,name);
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
        int A[2]; //front pipe
        int B[2]; //rear pipe

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
                user.future_pipe_number_subone(fd);  //all future pipe-number --
                for(int i{line_h[j]};i<(line_t[j]+1);i++){

//cout<<"["<<com[i]<<"]"<<"-"<<strlen(com[i])<<endl;
                    bool if_suc_userpipe_a{true};
                    bool if_suc_userpipe_b{true};
                    //head
                    int have_pipe_already{false};
                    if(i==line_h[j]){

                        have_pipe_already=user.get_futurepipe_read(fd,A);

                        if(userpipe_in[i]>0) if_suc_userpipe_a=user.get_userpipe_read(fd,userpipe_in[i],A,LINE);

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
                        if(skip[i]>0) user.add_futurepipe(fd,skip[i],B);
                        else if(userpipe_out[i]>0) if_suc_userpipe_b=user.add_userpipe(fd,userpipe_out[i],LINE,B);
               
                    }
//cout<<"A[0]"<<A[0]<<endl;
//cout<<"A[1]"<<A[1]<<endl;
//cout<<"B[0]"<<B[0]<<endl;
//cout<<"B[1]"<<B[1]<<endl;
                    pid=fork();

                    if(pid>0){
                        //close pipe; 
                        if((have_pipe_already and i==line_h[j])or(i!=line_h[j])or(userpipe_in[i]>0)){
                            close(A[1]);
                            close(A[0]);
                        }
                        int status;
                        if(i==(n-1) and skip[i]==0 and userpipe_out[i]==0){
                            waitpid(pid,&status,0);                     
                        }
                        usleep(100);
                    }

                    //child
                    else if(pid==0){
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
    //close LINE store in heap
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

    signal(SIGCHLD,SIG_IGN);

    int server_sockfd, client_sockfd; 
    int server_len, client_len; 
    struct sockaddr_in server_address; 
    struct sockaddr_in client_address; 
    int result; 
    user_info user; //record all client information
    fd_set socket_all, socket_writen; //readfds->record all socket in system   testfds->the sockets which haves been writen
    
    //create "server socket" and is recorded in socket_all[0]
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);//建立服务器端socket 
    server_address.sin_family = AF_INET; 
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_address.sin_port = htons(SERV_PORT); 
    server_len = sizeof(server_address); 

    int option = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &option , sizeof(int)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    if (bind(server_sockfd, (sockaddr *)&server_address, server_len) <0) {
             perror ("bind failed");
             exit(1);
       }

    if (listen(server_sockfd, 100) <0) {
         perror ("listen failed");
         exit(1);
    }

    FD_ZERO(&socket_all); 
    FD_SET(server_sockfd, &socket_all);//将服务器端socket加入到集合中  readfd[0]=the socket used to accept connection

    
    cout<<"server on"<<endl;
    
    while(1) 
    {
        char ch; 
        int nread;  //???????????
        socket_writen = socket_all;//将需要监视的描述符集copy到select查询队列中，select会对其修改，所以一定要分开使用变量  each loop we need to re-store to socket_writen

        /*无限期阻塞，并测试文件描述符变动 */
        result = select(FD_SETSIZE, &socket_writen, (fd_set *)0,(fd_set *)0, (struct timeval *) 0); //FD_SETSIZE：系统默认的最大文件描述符
        if(result < 1) 
        { 
            cerr<<"Fail to select, errno: "<<errno<<endl;
            if(errno == EINTR)
                continue;
            exit(1);
        } 
        else{
            for(int fd {0}; fd < FD_SETSIZE; fd++) //check each client socket  //fd:the ID of user serviced right now
            {
                /*找到相关文件描述符*/
                if(FD_ISSET(fd,&socket_writen)) //if client socket was writen(==1 in set_fd)
                {   

                /*判断是否为服务器套接字，是则表示为客户请求连接。*/
                    if(fd == server_sockfd) 
                    { 
                        if(user.total_member()<30){
                            client_len = sizeof(client_address); 
                            client_sockfd = accept(server_sockfd, (sockaddr *)&client_address,(socklen_t*)&client_len);
                            FD_SET(client_sockfd, &socket_all);//将客户端socket加入到集合中
                            user.login(client_address,client_sockfd);

                            write(client_sockfd,"% ",2);
                        }
                    } 
                    /*客户端socket中有数据请求时*/
                    else 
                    {   
                        user.get_path(fd);
                        int origin{dup(STDOUT_FILENO)};
                        dup2(fd, STDOUT_FILENO);
                        int origin_{dup(STDERR_FILENO)};
                        dup2(fd, STDERR_FILENO);
                        if(!shell(fd,user)){
                            FD_CLR(fd,&socket_all);
                            close(fd);
                        }
                        write(fd,"% ",2);
                        dup2(origin, STDOUT_FILENO);
                        close(origin);
                        dup2(origin_, STDERR_FILENO);
                        close(origin_);
                    } 
                } 
            }
        } 
        usleep(1000);
    } 

    return 0;
}




//user no"exit()" or entire system be clased,when user command=="exit"->need to remove socket of client from set_fd and boardcast