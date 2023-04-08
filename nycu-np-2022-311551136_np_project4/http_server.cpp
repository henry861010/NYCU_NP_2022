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

using boost::asio::ip::tcp;
using namespace std;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
cout<<"********************accept http connect request*************************"<<endl;
    wait_http_request();
  }

private:
  void wait_http_request()
  {
    auto self(shared_from_this());

    socket_.async_read_some(boost::asio::buffer(request_message, max_length),
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
    char response_str[100]="HTTP/1.1 200 OK\r\n";
    boost::asio::async_write(socket_, boost::asio::buffer(response_str,strlen(response_str)),
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
            env["SERVER_ADDR"]=socket_.local_endpoint().address().to_string(); 
            env["SERVER_PORT"]=to_string(socket_.local_endpoint().port());  
            env["REMOTE_ADDR"]=socket_.remote_endpoint().address().to_string();
            env["REMOTE_PORT"]=to_string(socket_.remote_endpoint().port());


            if(fork()==0){
              for (const auto& s : env ){
                setenv((s.first).c_str(), (s.second).c_str(), 1);
              }
              dup2(socket_.native_handle(), 0);
              dup2(socket_.native_handle(), 1);
              close(socket_.native_handle());
              string address="./";
              string exe=address+env["REQUEST_URI"];
              if(execlp(exe.c_str() , exe.c_str() ,NULL ) == -1) {
                cerr << "Error: " << env["REQUEST_URI"].c_str() << endl;
              }
            }
            else {
              socket_.close();
            }
            //////////
            wait_http_request();
          }
        });
  }

  map<string,string> env;
  tcp::socket socket_;
  enum { max_length = 10000 };
  char request_message[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
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
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  cout<<"http server begin"<<endl;
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
