#include <fstream>
#include <istream>
#include <ostream>
//#include <boost/bind.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <iostream>
#include <string>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost::asio::ip;
using namespace boost::asio;

boost::asio::io_context io_context_;

class client
 : public std::enable_shared_from_this<client>
{
public:
  client(string id,tcp::resolver::query q,string filename)
    : id_(id),q_(move(q)),filename_(filename){
      string filepath="./test_case/"+filename_;
      myfile.open(filepath);
      if(myfile.is_open()){
cerr<<"open file success:"<<filepath<<endl;
      }
      else{
cerr<<"open file fail:"<<filepath<<endl;
      }
    }

  void start(){
cerr<<"start"<<endl;
    auto self(shared_from_this());
    resolver_.async_resolve(q_, 
        [this, self](boost::system::error_code ec, tcp::resolver::iterator it)
        {
            if(!ec){
              resolver_handler(it);
            }
        });
  }

private:
  void resolver_handler(tcp::resolver::iterator it){
cerr<<"resolver handler"<<endl;
    auto self(shared_from_this());
    socket_.async_connect(*it, 
        [this, self](boost::system::error_code ec)
        {
            if(!ec){
cerr<<"connect to np_single server success"<<endl;
              do_read();
            }
            else{
cerr<<"connect to np_single server fail"<<endl;
            }
        });
  }

  void do_read(void){

cerr<<"do read"<<endl;
    auto self(shared_from_this());
    memset(result,'\0',strlen(result));
    socket_.async_read_some(boost::asio::buffer(result, max_length),
      [this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
              result[length]='\0';
              string temp=result;
              memset(result,'\0',strlen(result));
              output_shell(id_,temp); //have command result
              if(temp.find("%")!= string::npos){
                do_write();
              }
              else{  //broadcast,tell....
                do_read();
              }
            }
            
        });
  }

  void do_write(void){
    auto self(shared_from_this());
cerr<<"do writ"<<endl;
    string com;
    getline (myfile, com);
    output_command(id_,com);
    com=com+"\n";
    /////////////////////////////////////////////
    vector<string> temp;
    boost::split( temp, com, boost::is_any_of( " " ), boost::token_compress_on );
    if(com.find("removetag0")!= string::npos and temp[temp.size()-1].find("|")!= string::npos){
      tt=com;
      boost::asio::async_write(socket_, boost::asio::buffer(com.c_str(),strlen(com.c_str())),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if(!ec){
              do_error();
            }
        });
      }
    else{
    /////////////////////////////////////////////
    if(com.find("exit")!= string::npos){
      boost::asio::async_write(socket_, boost::asio::buffer(com.c_str(),com.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          output_shell(id_,"~~~finish !!!");
        });
    }
    else{
      boost::asio::async_write(socket_, boost::asio::buffer(com.c_str(),strlen(com.c_str())),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if(!ec){
                do_read();
            }
        });
    }
    }/////////////////////////////
  }
void do_error(){
cerr<<"do_error"<<endl;
    auto self(shared_from_this());
    memset(result,'\0',strlen(result));
    socket_.async_read_some(boost::asio::buffer(result, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          { 
            output_shell(id_,result);
            string temp=result;
            memset(result,'\0',strlen(result));
            if(temp.find("Error")!= string::npos){
              tt="";
              do_write();
            }
            else{
              do_error();
            }
          }
        });
  }
  void output_shell(string session,string  content){
    escape(content);
    cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '"<<content<<"';</script>"<<endl;
    cout.flush();
    usleep(500);
  }

  void output_command(string session,string  content){
    content=content+"\n";
    escape(content);
    cout<<"<script>document.getElementById('s"<<session<<"').innerHTML += '<b>"<<content<<"</b>';</script>"<<endl;
    cout.flush();
    usleep(500);
  }

  void escape(string &content){
    boost::replace_all(content, "&", "&amp;");
    boost::replace_all(content, "\"", "&quot;");
    boost::replace_all(content, "\'", "&apos;");
    boost::replace_all(content, "<", "&lt;");
    boost::replace_all(content, ">", "&gt;");
    boost::replace_all(content, "\r", "");
    boost::replace_all(content, "\n", "&NewLine;");
  }

std::ifstream myfile; 
vector<string> com;
tcp::resolver::query q_;
tcp::resolver resolver_{io_context_};
tcp::socket socket_{io_context_};
string id_;
string filename_;
enum { max_length = 40000 };
char result[max_length];
string tt;  ////////////
};

void printbegin(void){


  cout<<"Content-type: text/html\r\n\r\n";
  string html =
      R""""(
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8" />
            <title>NP Project 3 Sample Console</title>
            <link
            rel="stylesheet"
            href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
            integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
            crossorigin="anonymous"
            />
            <link
            href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
            rel="stylesheet"
            />
            <link
            rel="icon"
            type="image/png"
            href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
            />
            <style>
            * {
                font-family: 'Source Code Pro', monospace;
                font-size: 1rem !important;
            }
            body {
                background-color: #212529;
            }
            pre {
                color: #cccccc;
            }
            b {
                color: #01b468;
            }
            </style>
            </head>
            <body>
                <table class="table table-dark table-bordered">
                <thead>
                    <tr>
                    <th scope="col">server 0</th>
                    <th scope="col">server 1</th>
                    <th scope="col">server 2</th>
                    <th scope="col">server 3</th>
                    <th scope="col">server 4</th>
                    </tr>
                </thead>
                <tbody>
                    <tr>
                    <td><pre id="s0" class="mb-0"></pre></td>
                    <td><pre id="s1" class="mb-0"></pre></td>
                    <td><pre id="s2" class="mb-0"></pre></td>
                    <td><pre id="s3" class="mb-0"></pre></td>
                    <td><pre id="s4" class="mb-0"></pre></td>
                    </tr>
                </tbody>
                </table>
            </body>
            </html>
      )"""";
      cout << html;
}

int main(int argc, char* argv[])
{
  printbegin();

  try
  { 
    tcp::resolver resolv{io_context_};

    string s{getenv("QUERY_STRING")};
    vector<string> parameter;
    boost::split( parameter, s, boost::is_any_of( "&" ), boost::token_compress_on );
    for(int i{0};i<parameter.size();i=i+3){
      string host=parameter[i].substr(parameter[i].find("=")+1);
      string port=parameter[i+1].substr(parameter[i+1].find("=")+1);
      string filename=parameter[i+2].substr(parameter[i+2].find("=")+1);

      if(host!="" and port!="" and filename!=""){
cerr<<"begin client-"<<to_string(i/3)<<"  (int)"<<i/3<<"  port:"<<port<<"  filename:"<<filename<<" host:"<<host<<"  length:"<<host.length()<<endl;
        tcp::resolver::query q(host, port);
        std::make_shared<client>(to_string(i/3),move(q),filename)->start();
      }
    }
    io_context_.run();
  }
  catch (std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }

  return 0;
}