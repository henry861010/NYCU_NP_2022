#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

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

int main(void){
    char* com[4000];   //record comment
    char* file1[4000];  //record file
    char* file2[4000];
    int file_number[4000];
    bool err[4000];    //record does neet to pipe stderr    //////////////////////////////err
	int skip[4000];   //number of pipe to next n th line
	int line_t[4000];   //int this line,who is i th line's head
    int line_h[4000];
    pid_t pid;
    setenv("PATH","bin:.",1);
    signal(SIGCHLD,SIG_IGN);


    vector<record_future> future;  //record pipe which has reader in the maybe next line
    while(1){
        string line_s;

        cout<<"% ";
        getline(cin,line_s);
        char* line;
        line=&line_s[0];
        char* redirection_file;
        bool redirection_file_happen{false};
        int n{0};   //command number
		int p{0};   //line number
        com[n]=NULL;

    //transfer line to com[]
        char* temp=strtok(line," ");
      
        while(temp!=NULL){
            com[n]=temp;
            file1[n]=NULL;
            file2[n]=NULL;
            skip[n]=0;
            err[n]=false;      ///////////////////////////err
            redirection_file=(char*)"NULL";
            if((0==strcmp(temp,(char*)"printenv")) or (0==strcmp(temp,(char*)"setenv")) or (0==strcmp(temp,(char*)"exit"))){
                char* val=strtok(NULL," ");
                char* argument=strtok(NULL," "); 

                if(0==strcmp(temp,(char*)"printenv")){
                    char* iii=getenv(val);
                    if(iii) cout<<iii<<endl;
                }
                if(0==strcmp(temp,(char*)"setenv")){
                    setenv(val,argument,1);
                }
                if(0==strcmp(temp,(char*)"exit")){
                    exit(0);
                }
                break;
            }

            temp=strtok(NULL," ");
            if(temp==NULL) break; //command error
            file_number[n]=0;
            if((temp[0]!='|')and (temp[0]!='!')and (temp[0]!='>')){
                file1[n]=temp;
                temp=strtok(NULL," ");
                file_number[n]++;
                if(temp==NULL) break; //command error
            }
            if((temp[0]!='|')and (temp[0]!='!')and (temp[0]!='>')){
                file2[n]=temp;
                temp=strtok(NULL," ");
                file_number[n]++;
                if(temp==NULL) break; //command error
            }
            

            if(temp[0]=='>'){
                redirection_file_happen=true;
                redirection_file=strtok(NULL," ");
                break;
            }
            else{
                if(temp[0]=='!'){err[n]=true; } 
                char* k=temp+1;
                skip[n]=char_to_int(k);
                temp=strtok(NULL," ");
                if(temp==NULL) break;  //command error
            }
            
            n++;
        }
        n++;
        int A[2];
        int B[2];

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
                 for(size_t k{0};k<future.size();k++){
                    future[k].number--;
                }
                for(int i{line_h[j]};i<(line_t[j]+1);i++){

                    //head
                    int have_pipe_already{false};
                    if(i==line_h[j]){

                        for(int k{0};k<future.size();k++){ 
                            if(0==future[k].number){   //have pipe already
                                A[0]=future[k].pipe_read;   
                                A[1]=future[k].pipe_write;
                                future.erase(future.begin()+k);
                                have_pipe_already=true;
                            }  
                        }
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
                        if(skip[i]>0){   //stdout dont need to creat pipe
                            bool same_pipe_in_future{false};
                            for(int k{0};k<future.size();k++){ 
                                if(future[k].number==skip[i]){
                                    B[0]=future[k].pipe_read;
                                    B[1]=future[k].pipe_write;
                                    same_pipe_in_future=true;
                                }
                            }
                            if(!same_pipe_in_future){
                                pipe(B);
                                future.push_back(record_future{B[1],B[0],skip[i]});
                            }
                        }
                    }

                    pid=fork();

                    if(pid>0){
                        //close pipe; 
                        if((have_pipe_already and i==line_h[j])or(i!=line_h[j])){
                            close(A[1]);
                            close(A[0]);
                        }
                        int status;
                        if(i==(n-1) and skip[i]==0){
                            waitpid(pid,&status,0);                  /////     
                        }
                        usleep(100000);
                    }

                    //child
                    else if(pid==0){
                        if(!strcmp(com[i],(char*)"printenv") or !strcmp(com[i],(char*)"setenv") or !strcmp(com[i],(char*)"exit")){
                            exit(0);
                        }

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
                        
                        if(file_number[i]==0){
                            if(execlp(com[i],com[i],NULL)) cerr<<"Unknown command: ["<<com[i]<<"]."<<endl;
                        }
                        else if(file_number[i]==1){ 
                            if(execlp(com[i],com[i],file1[i],NULL)) cerr<<"Unknown command: ["<<com[i]<<"]."<<endl;
                        }
                        else if(file_number[i]==2){ 
                            if(execlp(com[i],com[i],file1[i],file2[i],NULL)) cerr<<"Unknown command: ["<<com[i]<<"]."<<endl;
                        }
                        else{
                            cerr<<"Unknown command: ["<<com[i]<<"]."<<endl;
                        }
                        exit(0);
                    }
                }
            }
        }   
    }
    return 0;
}










