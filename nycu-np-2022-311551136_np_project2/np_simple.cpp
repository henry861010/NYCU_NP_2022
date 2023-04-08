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

#define MAXNAME 1024

using namespace std;

struct record_future{
    int pipe_write;
    int pipe_read;
    int number;  //the number of process which read pipe(only one)
};


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

int shell(int client_sockfd){
      char* com[4000];   //record comment
      char* file[4000][20]{0};
      int filenumber[4000]{0};
      bool err[4000];    //record does neet to pipe stderr    //////////////////////////////err
	int skip[4000];   //number of pipe to next n th line
	int line_t[4000];   //int this line,who is i th line's head
      int line_h[4000];
      pid_t pid;
      char buf[BUFSIZ];
      bool if_userpipe{false};
      setenv("PATH", "bin:.", 1);
      signal(SIGCHLD, SIG_IGN);

      vector<record_future> future; // record pipe which has reader in the maybe next line
      while (1)
      {
            char line[20000];
            memset(line, '\0', sizeof(line));

            write(client_sockfd,"% ",2);
            int read_size{0};
            if((read_size = recv(client_sockfd, line, 20000,0) <0)){
                  if(errno == EINTR) continue;
                  cerr<<"Fail to read cmdLine, errno: "<<errno<<endl;
                  return -1;
            }
//cout<<"read_size:"<<read_size<<endl;
//cout<<"line_size:"<<strlen(line)<<endl;
            string s=line;
            replace( s.begin(), s.end(), '\r', '\0');
            replace( s.begin(), s.end(), '\n', '\0');
            strcpy(line,s.c_str());
           

            char *redirection_file;
            bool redirection_file_happen{false};
            int n{0}; // command number
            int p{0}; // line number
            com[n] = NULL;

            // transfer line to com[]
            char *temp = strtok(line, " ");

            while(temp!=NULL){
                  filenumber[n]=0;
                  com[n]=temp;
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
//cout<<"n:"<<n<<endl;

            if (com[0] != NULL)
            {
                  line_h[0] = 0;
                  for (int i{0}; i < n; i++){
                        if (skip[i] > 0){
                              line_t[p] = i;
                              if ((i + 1) < n)
                                    line_h[p + 1] = i + 1;
                              p++;
                        }
                  }
                  if (!(skip[n - 1] > 0)) p++;
                  line_t[p - 1] = n - 1;

                  // build child process(commend)
                  for (int j{0}; j < p; j++){
                        for (size_t k{0}; k < future.size(); k++){
                              future[k].number--;
                        }
                        for (int i{line_h[j]}; i < (line_t[j] + 1); i++){
//cout<<"["<<com[i]<<"]"<<"-"<<strlen(com[i])<<endl;
//if(file1[i])cout<<"["<<file1[i]<<"]"<<"-"<<strlen(file1[i])<<endl;
                              // head
                              int have_pipe_already{false};
                              if (i == line_h[j])
                              {

                                    for (int k{0}; k < future.size(); k++)
                                    {
                                          if (0 == future[k].number)
                                          { // have pipe already
                                                A[0] = future[k].pipe_read;
                                                A[1] = future[k].pipe_write;
                                                future.erase(future.begin() + k);
                                                have_pipe_already = true;
                                          }
                                    }
                                    if (line_t[j] != line_h[j])
                                    {
                                          pipe(B);
                                    }
                              }
                              // middle
                              if (i != line_h[j] and i != line_t[j])
                              {

                                    A[1] = B[1];
                                    A[0] = B[0];

                                    pipe(B);
                              }
                              // tail
                              if (i == line_t[j])
                              {
                                    if (line_t[j] != line_h[j])
                                    {
                                          A[0] = B[0];
                                          A[1] = B[1];
                                    }
                                    if (skip[i] > 0)
                                    { // stdout dont need to creat pipe
                                          bool same_pipe_in_future{false};
                                          for (int k{0}; k < future.size(); k++)
                                          {
                                                if (future[k].number == skip[i])
                                                {
                                                      B[0] = future[k].pipe_read;
                                                      B[1] = future[k].pipe_write;
                                                      same_pipe_in_future = true;
                                                }
                                          }
                                          if (!same_pipe_in_future)
                                          {
                                                pipe(B);
                                                future.push_back(record_future{B[1], B[0], skip[i]});
                                          }
                                    }
                              }

                              pid = fork();

                              if (pid > 0)
                              {
                                    // close pipe;
                                    if ((have_pipe_already and i == line_h[j]) or (i != line_h[j]))
                                    {
                                          close(A[1]);
                                          close(A[0]);
                                    }
                                    int status;
                                    if (i == (n - 1) and skip[i] == 0)
                                    {
                                          waitpid(pid, &status, 0); /////
                                    }
                                    usleep(100);
                              }

                              // child
                              else if (pid == 0){
                                    if(!strcmp(com[i],(char*)"printenv")) exit(1);
                                    if(!strcmp(com[i],(char*)"setenv")) exit(1);
                                    if(!strcmp(com[i],(char*)"exit")) exit(1);
                                    //single process server,so if use "exit()",entire server be closed

                                    if((have_pipe_already and i==line_h[j])or(i!=line_h[j])){
                                          dup2(A[0],STDIN_FILENO);
                                          close(A[1]);
                                          close(A[0]);
                                    }
                                    if((i!=line_t[j])or(skip[i]>0)){
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
      return 0;
}

int main(int argc, char* const argv[])
{
      
      if(argc > 2)
      {
            cerr<<"Argument set error"<<endl;
            exit(1);
      }

      int SERV_PORT = atoi(argv[1]);

       int socket_fd;   /* file description into transport */
       int recfd; /* file descriptor to accept        */
       int length;      /* length of address structure      */
       int nbytes;      /* the number of read **/
       char buf[BUFSIZ];

       struct sockaddr_in myaddr; /* address of this service */
       struct sockaddr_in client_addr; /* address of client    */


       if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
            perror ("socket failed");
            exit(1);
       }



       bzero ((char *)&myaddr, sizeof(myaddr)); /* 清除位址內容 */
       myaddr.sin_family = AF_INET;    /* 設定協定格式 */
       myaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* IP次序轉換 */
       myaddr.sin_port = htons(SERV_PORT);  /* 埠口位元次序轉換 */

      int option = 1;
      if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option , sizeof(int)) < 0){
            perror("setsockopt(SO_REUSEADDR) failed");
      }

       if (bind(socket_fd, (sockaddr *)&myaddr, sizeof(myaddr)) <0) {
             perror ("bind failed");
             exit(1);
       }

       if (listen(socket_fd, 100) <0) {
             perror ("listen failed");
             exit(1);
       }


       length = sizeof(client_addr);

       cout<<"Server is ready to receive !!"<<endl;
       cout<<"Can strike Cntrl-c to stop Server"<<endl;

       while (1) {

            if ((recfd = accept(socket_fd,(sockaddr*)&client_addr, (socklen_t*)&length)) <0) {
                  perror ("could not accept call");
                  exit(1);
               }
            if(fork()==0){
                  //childe
                  close(socket_fd);
                  dup2(recfd, STDIN_FILENO);  //server need to receive command from socket
                  dup2(recfd, STDOUT_FILENO);
                  dup2(recfd, STDERR_FILENO);
                  //close(recfd);
                  shell(recfd);
                  exit(0);
                  
            }
            
            //parent
            close(recfd);
            

       }
       return 0;

}