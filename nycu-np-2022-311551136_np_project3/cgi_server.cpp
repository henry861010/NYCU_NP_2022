#include <fstream>
#include <istream>
#include <ostream>


#include <cstdlib>
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


#include <boost/system/error_code.hpp>
#include <iostream>
using namespace boost::system;


#include <filesystem>  

boost::asio::io_context io_context_;  
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class console
 : public std::enable_shared_from_this<console>
{
public:
  console(string id,tcp::resolver::query q,string filename,shared_ptr<tcp::socket> socket_browser_)
    :id_(id),q_(move(q)),filename_(filename),socket_browser(socket_browser_){
      string filepath="./test_case/"+filename_;
      myfile.open(filepath);
      if(myfile.is_open()){
cerr<<id_<<" "<<"open file success:"<<filepath<<endl;
      }
      else{
cerr<<id_<<" "<<"open file fail:"<<filepath<<endl;
      }
    }

  void start(){
cerr<<id_<<" "<<"start"<<endl;
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
cerr<<id_<<" "<<"resolver handler"<<endl;
    auto self(shared_from_this());
    socket_.async_connect(*it, 
        [this, self](boost::system::error_code ec)
        {
            if(!ec){
cerr<<id_<<" "<<"connect to server success"<<endl;
              do_read();
            }
            else{
cerr<<id_<<" "<<"connect to server fail"<<endl;
            }
        });
  }

  void do_read(void){

cerr<<id_<<" "<<"do read"<<endl;
    auto self(shared_from_this());
    memset(result,'\0',strlen(result));
    socket_.async_read_some(boost::asio::buffer(result, max_length),
      [this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
              result[length]='\0';
cerr<<id_<<" "<<result<<endl;
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
cerr<<"do write"<<endl;
    auto self(shared_from_this());

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

      //replace( com.begin(), com.end(), '\r', '\0');
      //replace( com.begin(), com.end(), '\n', '\0');

cerr<<"com:["<<com<<"]"<<endl;
      if(com.find("exit")!= string::npos){
        boost::asio::async_write(socket_, boost::asio::buffer(com.c_str(),com.length()),
          [this, self](boost::system::error_code ec, std::size_t /*length*/)
          { 
            if(!ec){
              socket_.close();
              myfile.close();
              output_shell(id_,"~~~finish !!!");
              if(socket_browser.use_count() == 2){
cerr<<"close socket_browser"<<endl;
                socket_browser->close();
              }
            }
            else{
              cerr <<id_<<" exit write---"<<ec.message() << endl;
            }
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
     boost::asio::async_read(socket_,boost::asio::buffer(result, max_length),
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
    auto self(shared_from_this());
    escape(content);
    string r="<script>document.getElementById('s"+session+"').innerHTML += '"+content+"';</script>";
    boost::asio::write(*socket_browser, boost::asio::buffer(r) );
    usleep(5000);
  }

  void output_command(string session,string  content){
    auto self(shared_from_this());
    content=content+"\n";
    escape(content);
    string r="<script>document.getElementById('s"+session+"').innerHTML += '<b>"+content+"</b>';</script>";
    boost::asio::write(*socket_browser, boost::asio::buffer(r) );
    usleep(5000);
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
shared_ptr<tcp::socket> socket_browser;
string id_;
string filename_;
enum { max_length = 40000 };
char result[max_length];
char output[max_length];
char err[max_length];
string tt;  ////////////
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_browser(std::move(socket))
  {
  }

  void start()
  {
//cout<<"********************accept http connect request*************************"<<endl;
    wait_http_request();
  }

private:
  void write_to_browser(string msg){
    auto self(shared_from_this());
    boost::asio::async_write(
            socket_browser, boost::asio::buffer(msg.c_str(), strlen(msg.c_str())),
            [this, self](boost::system::error_code ec, size_t /*length*/) {
                if (ec) {
                    cerr << "------------------"<<ec.message() << endl;
                }
            }
        );
  }

  void printenv(map<string,string> env){
    cerr<<"connect information:"<<endl;
    cerr<<"REQUEST_METHOD:"<<"["<<env["REQUEST_METHOD"]<<"]"<<endl;
    cerr<<"REQUEST_URI:"<<"["<<env["REQUEST_URI"]<<"]"<<endl;
    cerr<<"QUERY_STRING:"<<"["<<env["QUERY_STRING"]<<"]"<<endl;
    cerr<<"SERVER_PROTOCOL:"<<"["<<env["SERVER_PROTOCOL"]<<"]"<<endl;
    cerr<<"HTTP_HOST:"<<"["<<env["HTTP_HOST"]<<"]"<<endl;
    cerr<<"SERVER_ADDR:"<<"["<<env["SERVER_ADDR"]<<"]"<<endl;
    cerr<<"SERVER_PORT:"<<"["<<env["SERVER_PORT"]<<"]"<<endl;
    cerr<<"REMOTE_ADDR:"<<"["<<env["REMOTE_ADDR"]<<"]"<<endl;
    cerr<<"REMOTE_PORT:"<<"["<<env["REMOTE_PORT"]<<"]"<<endl;


    //setenv("REQUEST_METHOD", env["REQUEST_METHOD"].c_str(), 1);
    //setenv("REQUEST_URI", env["REQUEST_URI"].c_str(), 1);
    //setenv("QUERY_STRING", env["QUERY_STRING"].c_str(), 1);
    //setenv("SERVER_PROTOCOL", env["SERVER_PROTOCOL"].c_str(), 1);
    //setenv("HTTP_HOST", env["HTTP_HOST"].c_str(), 1);
    //setenv("SERVER_ADDR", env["SERVER_ADDR"].c_str(), 1);
    //setenv("SERVER_PORT", env["SERVER_PORT"].c_str(), 1);
    //setenv("REMOTE_ADDR", env["REMOTE_ADDR"].c_str(), 1);
    //setenv("REMOTE_PORT", env["REMOTE_PORT"].c_str(), 1);

  }

  void console_printbegin(void){


    string b="Content-type: text/html\r\n\r\n";
    write_to_browser(b);
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
      //write_to_browser(html);
      write_to_browser(html);
  }

  void wait_http_request()
  {
    auto self(shared_from_this());
    socket_browser.async_read_some(boost::asio::buffer(request_message, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            accept_http_request(length);
          }
        });
  }

  void accept_http_request(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_browser, boost::asio::buffer(response_str,strlen(response_str)),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            ////////// parsing request_message
            std::string s(request_message);
            std::vector<std::string> sentences;
            std::vector<std::string> word;
            boost::split( sentences, s, boost::is_any_of( "\r\n" ), boost::token_compress_on );
            boost::split( word, sentences[0], boost::is_any_of( " " ), boost::token_compress_on );

            env["REQUEST_METHOD"]=word[0];
            env["REQUEST_URI"]=word[1].substr(1,word[1].find("?")-1);    
            env["QUERY_STRING"]="";
            if(word[1].find("?") != string::npos) env["QUERY_STRING"]=word[1].substr(word[1].find("?")+1);
            env["SERVER_PROTOCOL"]=word[2];
            env["HTTP_HOST"]=sentences[1].substr(sentences[1].find(" ")+1);         
            env["SERVER_ADDR"]=socket_browser.local_endpoint().address().to_string(); 
            env["SERVER_PORT"]=to_string(socket_browser.local_endpoint().port());  
            env["REMOTE_ADDR"]=socket_browser.remote_endpoint().address().to_string();
            env["REMOTE_PORT"]=to_string(socket_browser.remote_endpoint().port());

            if(0==strcmp( env["REQUEST_URI"].c_str(),"panel.cgi")) {     
cerr << "execute panel.cgi" << endl;
              int N_SERVERS = 5;
              int HOST_NUMBER = 12;

              string TEST_CASE_DIR = "test_case";
              string DOMAIN = "cs.nctu.edu.tw";

              string option_host_menu="<option></option>";
              for(int i{0};i<HOST_NUMBER;i++){
                //option_host_menu=option_host_menu+"<option value=nplinux"+to_string(i+1)+"."+DOMAIN+">"+"nplinux"+to_string(i+1)+"</option>";
                option_host_menu=option_host_menu+"<option>localhost</option>";
              }

              string path = "./test_case";
              string option_testcase_menu="<option></option>";
              for (const auto & entry : std::filesystem::directory_iterator(path)){
                std::vector<std::string> temp;
                boost::split( temp, (entry.path()).string(), boost::is_any_of( "/" ), boost::token_compress_on );

                option_testcase_menu=option_testcase_menu+"<option value="+temp[temp.size()-1]+">"+temp[temp.size()-1]+"</option>";
              }

              string html0="Content-type: text/html\r\n\r\n";
              write_to_browser(html0);

              string html1 =
                  R""""(
                    <!DOCTYPE html>
                    <html lang="en">
                    <head>
                    <title>NP Project 3 Panel</title>
                    <link
                    rel="stylesheet"
                    href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
                    integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
                    crossorigin="anonymous"
                    />
                    <link
                    href="https://fonts.googleapis.com/css?family=Source+Code+Pro"ptrr
                    rel="stylesheet"
                    />
                    <link
                    rel="icon"
                    type="image/png"
                    href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
                    />
                    <style>
                    * {
                    font-family: 'Source Code Pro', monospace;
                    }
                    </style>
                    </head>
                    <body class="bg-secondary pt-5">
                    )"""";
              write_to_browser(html1);

              string html2 =
                  R""""(
                    <form action="console.cgi" method="GET">
                    <table class="table mx-auto bg-light" style="width: inherit">
                    <thead class="thead-dark">
                    <tr>
                    <th scope="col">#</th>
                    <th scope="col">Host</th>
                    <th scope="col">Port</th>
                    <th scope="col">Input File</th>
                    </tr>
                    </thead>
                    <tbody>
                    )"""";
              write_to_browser(html2);

              for(int i{0};i<N_SERVERS;i++){
                string html3 =
                  R""""(
                    <tr>
                    <th scope="row" class="align-middle">Session )""""+to_string(i + 1)+R""""(</th>
                    <td>
                    <div class="input-group">
                    <select name="h)""""+to_string(i)+R""""(" class="custom-select">
                    )""""+option_host_menu+R""""(
                    </select>
                    <div class="input-group-append">
                    <span class="input-group-text">.cs.nctu.edu.tw</span>
                    </div>
                    </div>
                    </td>
                    <td>
                    <input name="p)""""+to_string(i)+R""""(" type="text" class="form-control" size="5" />
                    </td>
                    <td>
                    <select name="f)""""+to_string(i)+R""""(" class="custom-select">
                    )""""+option_testcase_menu+R""""(
                    </select>
                    </td>
                    </tr>
                    )"""";
                write_to_browser(html3);
              }

              string html4 =
                  R""""(
                    <tr>
                    <td colspan="3"></td>
                    <td>
                    <button type="submit" class="btn btn-info btn-block">Run</button>
                    </td>
                    </tr>
                    </tbody>
                    </table>
                    </form>
                    </body>
                    </html>
                    )"""";
              write_to_browser(html4);
              socket_browser.close();
            }
            if(0==strcmp( env["REQUEST_URI"].c_str(),"console.cgi")){
cerr << "execute console.cgi" << endl;
                console_printbegin();
                printenv(env);

                try{ 
                    shared_ptr<tcp::socket> ptrr(&socket_browser);
                    string s{env["QUERY_STRING"]};
                    vector<string> parameter;
                    boost::replace_all(s, "%2F", "/");
                    boost::split( parameter, s, boost::is_any_of( "&" ), boost::token_compress_on );
                    for(int i{0};i<parameter.size();i=i+3){
                        string host=parameter[i].substr(parameter[i].find("=")+1);
                        string port=parameter[i+1].substr(parameter[i+1].find("=")+1);
                        string filename=parameter[i+2].substr(parameter[i+2].find("=")+1);

                        if(host!="" and port!="" and filename!=""){
cerr<<"begin client-"<<to_string(i/3)<<"  (int)"<<i/3<<"  port:"<<port<<"  filename:"<<filename<<endl;
                            tcp::resolver::query q(host, port);
                            std::make_shared<console>(to_string(i/3),move(q),filename,ptrr)->start();
                        }
                    }
                    io_context_.run();
                }
                catch (std::exception& e){
                    std::cout << "Exception: " << e.what() << "\n";
                }
            }


            //socket_browser.close(); // when finish ,need to close socket(every new page request is new "http accept",so every request will create new session object include socket)
          }
        });
  }

  map<string,string> env;
  tcp::socket socket_browser;
  enum { max_length = 1024 };
  char request_message[max_length];
  char response_str[100]="HTTP/1.1 200 OK\r\n";
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class server
{
public:
  server(boost::asio::io_context& io_context_, short port)
    : acceptor_(io_context_, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
cerr<<"*********************new http request come in*********************"<<endl;
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
  cout<<"http server begin"<<endl;
  try{
    if (argc != 2){
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }
    server s(io_context_, std::atoi(argv[1]));
    io_context_.run();
  }
  catch (std::exception& e){
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}