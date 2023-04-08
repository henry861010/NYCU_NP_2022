#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <string>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem> 
#include <fstream>
#include <istream>
#include <ostream>



#define max_length 1048576

using boost::asio::ip::tcp;
using namespace std;
using namespace boost::asio::ip;
using namespace boost::asio;

boost::asio::io_service io_service_;

struct socks_request_struct{
    string version;
    string command;
    string dstport;
    string dstip;
    string userip;
    string domainname;
};


class server
 : public std::enable_shared_from_this<server>
{
public:
  server(unsigned short port)
      :signal_(io_service_, SIGCHLD),
      acceptor_(io_service_, tcp::endpoint(tcp::v4(), port))
  {
    //start_signal_wait();
    start_accept();
  }

private:
  void start_signal_wait()
  {
    signal_.async_wait(boost::bind(&server::handle_signal_wait, this));
  }

  void handle_signal_wait()
  {
    // Only the parent process should check for this signal. We can determine
    // whether we are in the parent by checking if the acceptor is still open.
    if (acceptor_.is_open())
    {
      // Reap completed child processes so that we don't end up with zombies.
      int status = 0;
      while (waitpid(-1, &status, WNOHANG) > 0) {}
      start_signal_wait();
    }
  }

  void start_accept()
  {
    acceptor_.async_accept(socket_client,
        boost::bind(&server::handle_accept, this, _1));
  }

  void handle_accept(const boost::system::error_code& ec)
  {
    if (!ec)
    {
      io_service_.notify_fork(boost::asio::io_service::fork_prepare);

      if (fork() == 0)
      {
        io_service_.notify_fork(boost::asio::io_service::fork_child); 
        acceptor_.close(); 
        start_parse();
      }
      else
      {
        io_service_.notify_fork(boost::asio::io_service::fork_parent);

        socket_server.close();
        socket_client.close();
        start_accept();
      }
    }
    else
    {
      std::cerr << "Accept error: " << ec.message() << std::endl;
      start_accept();
    }
  }

  void start_parse()
  {
    socket_client.async_read_some(boost::asio::buffer(request),
        boost::bind(&server::handle_parse, this, _1, _2));
  }

  void handle_parse(const boost::system::error_code& ec, std::size_t length)
  {
    if (!ec){
      //parse request
      socks_request.version=request[0];
      if(request[1]==1) socks_request.command="CONNECT";
      else if(request[1]==2) socks_request.command="BIND";
      unsigned short aa=(unsigned char)request[2];
      unsigned short bb=(unsigned char)request[3];
      socks_request.dstport=to_string((aa<< 8)&0xff00 | bb);

      for(int i{4};i<8;i++){
        socks_request.dstip=socks_request.dstip+to_string((unsigned char)request[i]);
        if(i<7) socks_request.dstip=socks_request.dstip+".";
      }

      int check_userip{8};
      while(request[check_userip]!=0){
        socks_request.userip[check_userip-8]=request[check_userip];
        check_userip++;
      }
      check_userip++;
      int check_dstip=check_userip;
      while(request[check_dstip]!=0){
        socks_request.domainname+=request[check_dstip];
        check_dstip++;
      }


      if(socks_request.command=="CONNECT"){

        string host;
        if(request[4]==0 and request[5]==0 and request[6]==0){
          host=socks_request.domainname;
        }
        else{
          host=socks_request.dstip;
        }

        tcp::resolver resolver(io_service_);
        tcp::resolver::query query(host,socks_request.dstport);
        tcp::resolver::iterator iter = resolver.resolve(query);
        endpoints = iter->endpoint();

        if (firewall_check() and ((unsigned int)(socks_request.version[0]))==4){ 
            do_connect();
          }
        else{
          print_connect_information(false);
          do_reply(false,(unsigned short)0);
          socket_server.close();
          socket_client.close();
          exit(0); 
        }
      }


      else if(socks_request.command=="BIND"){ 
        tcp::endpoint ep(ip::tcp::v4(),(unsigned short)0);
        acceptor_bind.open(ep.protocol());
        acceptor_bind.set_option(tcp::acceptor::reuse_address(true));
        acceptor_bind.bind(ep);
        acceptor_bind.listen();
        unsigned short port_=acceptor_bind.local_endpoint().port();

        if(firewall_check() and ((unsigned int)(socks_request.version[0]))==4){
          print_connect_information(true);
          do_reply(true,port_);
          acceptor_bind.accept(socket_server);
          do_reply(true,port_);
          //wait ftp server connect in 
          do_read_from_client();
          do_read_from_server();
        }
        else{
          print_connect_information(false);
          do_reply(false,(unsigned short)0);
          socket_server.close();
          socket_client.close();
          exit(0);
        }
      }

    }
      
  }

//////////////////////////////////////////////////////////////////////

  void do_connect()
  {
    socket_server.async_connect(endpoints,
        [this](const boost::system::error_code& ec){
          if (!ec)
          { 
            //reply to client (success)
            print_connect_information(true);
            do_reply(true,(unsigned short)0);
            //async_read<->async_write 
            do_read_from_client();
            do_read_from_server();
          }
          else{
            cerr << "------------------"<<ec.message() << endl;
            print_connect_information(false);
            do_reply(false,(unsigned short)0);
            socket_server.close();
            socket_client.close();
            exit(0); 
          }
        });
  }



  void do_read_from_client(void){
    memset(data_client,'\0',max_length);
    socket_client.async_read_some(boost::asio::buffer(data_client, max_length),
      [this](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
              do_write_to_server(length);
            }
            
        });
  }

  void do_write_to_server(size_t length){
      boost::asio::async_write(socket_server, boost::asio::buffer(data_client,length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        { 
          if(!ec){
            do_read_from_client();
          }
        });
  }

  void do_read_from_server(void){
    memset(data_server,'\0',max_length);
    socket_server.async_read_some(boost::asio::buffer(data_server, max_length),
      [this](boost::system::error_code ec, std::size_t length)
        {
          if(!ec){
            do_write_to_client(length);
          }
            
        });
  }

  void do_write_to_client(size_t length){
      boost::asio::async_write(socket_client, boost::asio::buffer(data_server,length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        { 
          if(!ec){
            do_read_from_server();
          }
        });
  }
 
///////////////////////////////////////////////////////////////////////
  void do_reply(bool if_success,unsigned short dstport){
    
    unsigned char reply[8]={0,90,(unsigned char)(dstport/256),(unsigned char)(dstport%256),0,0,0,0};
    if(!if_success) reply[1]=91;
    write(socket_client, boost::asio::buffer(reply,8));
  }

  void print_connect_information(bool if_success){
    //print on server
    cout<<endl;
    cout<<"<S_IP>: "<<socket_client.remote_endpoint().address().to_string()<<endl;//<S_IP>: source ip
    cout<<"<S_PORT>: "<<socket_client.remote_endpoint().port()<<endl;//<S_PORT>: source port
    //if(request[4]==0 and request[5]==0 and request[6]==0)  cout<<"<D_IP>: :"<<socks_request.domainname<<endl;//<D_IP>: destination ip
    //else cout<<"<D_IP>: "<<socks_request.dstip<<endl;//<D_IP>: destination ip
    cout<<"<D_IP>: "<<endpoints.address().to_string()<<endl;
    cout<<"<D_PORT>: "<<socks_request.dstport<<endl;//<D_PORT>: destination port
    cout<<"<Command>: "<<socks_request.command<<endl;//<Command>: CONNECT or BIND
    if(if_success) cout<<"<Reply>: Accept"<<endl;
    else cout<<"<Reply>: Reject"<<endl;
  }

  bool firewall_check(void){
    bool ans{0};
    std::ifstream myfile;
    myfile.open("./socks.conf");
    if(myfile.is_open());

    std::vector<std::string> permit;
    std::vector<std::string> ip_permit;
    std::vector<std::string> ip_dst;
    boost::split(ip_dst,socks_request.dstip, boost::is_any_of( "." ), boost::token_compress_on );
    string com;
    while(1){
      getline (myfile, com);
      if(com!=""){
        boost::split(permit,com, boost::is_any_of( " " ), boost::token_compress_on );
        boost::split(ip_permit,permit[2], boost::is_any_of( "." ), boost::token_compress_on );
        if(permit[1]=="c" and socks_request.command=="CONNECT" ){
            for(int i{0};i<4;i++){
                if(ip_permit[i]=="*") ans=1; 
                else if(ip_permit[i]==ip_dst[i] and i==3) ans=1;
                else if(ip_permit[i]!=ip_dst[i]) break;
            }
        }
        else if(permit[1]=="b" and socks_request.command=="BIND" ){
            for(int i{0};i<4;i++){
                if(ip_permit[i]=="*") ans=1; 
                else if(ip_permit[i]==ip_dst[i] and i==3) ans=1;
                else if(ip_permit[i]!=ip_dst[i]) break;
            }
        }
      }
      else break;
    }
    return ans;
  }
///////////////////////////////////////////////////////////////////////
  tcp::endpoint endpoints;
  boost::asio::signal_set signal_;
  tcp::acceptor acceptor_;
  tcp::acceptor acceptor_bind{io_service_};
  tcp::socket socket_client{io_service_};
  tcp::socket socket_server{io_service_};
  char data_client[max_length];
  char data_server[max_length];
  char request[max_length];
  socks_request_struct socks_request;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: process_per_connection <port>\n";
      return 1;
    }

    server s(atoi(argv[1]));

    io_service_.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}