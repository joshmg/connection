// File: connection.cpp
// Written by Joshua Green

#include "connection.h"
#include "transmission.h"
#include <winsock2.h>
#include <iostream>
#include <string>
#include <windows.h>
#include <process.h>
#include <ctime>
using namespace std;

void do_nothing(void* ptr=0) {delete (string*)ptr;}
void do_nothing_int(void* ptr=0) {delete (int*)ptr;}

void p2p::_initialize() {
  _is_linked = false;
  _is_receiving = false;
  _halt_receiving = false;
  _abort_listening = false;
  _listen = INVALID_SOCKET;
  _transmit = INVALID_SOCKET;
  _recv_function = &do_nothing;
  _disc_function = &do_nothing_int;
  PORT = DEFAULT_PORT;
  _id = 0;
  LISTEN_TIMEOUT = DEFAULT_LISTEN_TIMEOUT;
  
  const int REQ_WINSOCK_VER = 2;
  WSADATA wsaData;
  WSAStartup(MAKEWORD(REQ_WINSOCK_VER,0), &wsaData);
  
  // ------------ GET USER IP ---------------
  const int temp_size = 20;
  char temp[temp_size];
  HOSTENT* host;
  gethostname(temp, temp_size);
  if (!(host = gethostbyname(temp))) {
    cout << "Unable to resolve user ip." << endl;
    _machine_ip = "0.0.0.0";
  }
  else _machine_ip = inet_ntoa(*(in_addr*)*host->h_addr_list);
}

void p2p::_recv_branch(void* pThis) {
  ((p2p*)pThis)->_recv_loop();
}

void p2p::_close_branch(void* pThis) {
  ((p2p*)pThis)->close();
}

void p2p::_recv_loop() {
  _is_receiving = false;
  if (!_is_linked) return;

  char buffer[BUFF_SIZE+1];
  int recv_count;

  while(!_halt_receiving && _is_linked) {
    _is_receiving = true;
    recv_count = recv(_transmit, buffer, (BUFF_SIZE), 0);
    if (recv_count==SOCKET_ERROR || recv_count == 0) {
      if (WSAGetLastError() != WSAEWOULDBLOCK || recv_count == 0) {
        _beginthread(&_close_branch, 0, (void*)this);
        int *connection_id = new int;
        (*connection_id) = _id;
        _beginthread(_disc_function, 0, (void*)(connection_id));
        _halt_receiving = true;
        break;
      }
      else Sleep(250);
    }
    else {
      buffer[recv_count] = 0;
      string* data = new string;
      (*data) = buffer;
      _beginthread(_recv_function, 0, data);
    }
  }
  _is_receiving = false;
}

p2p::p2p() {
  _initialize();
}

p2p::p2p(int port) {
  _initialize();
  PORT = port;
}

/*p2p::p2p(void (*func)(void*)) {
  _initialize();
  _recv_function = func;
}

p2p::p2p(int port, void (*func)(void*)) {
  _initialize();
  PORT = port;
  _recv_function = func;
}*/

p2p::~p2p() {
  close();
  WSACleanup();
}

void p2p::set_port(int port) {
  PORT = port;
}

void p2p::close(bool now) {
  if (!now) Sleep(1000); // enables pending transmissios to finalize
  disable_receive();
  _is_linked = false;
  if (_transmit != INVALID_SOCKET) closesocket(_transmit);
}

void p2p::set_recv_func(void (*func)(void*)) {
  _recv_function = func;
}

void p2p::set_disc_func(void (*func)(void*)) {
  _disc_function = func;
}

void p2p::enable_receive() {
  _halt_receiving = false;
  if (!_is_receiving) _beginthread(&p2p::_recv_branch, 0, (void*)(this)); // _recv_loop();
}

void p2p::disable_receive() {
  _halt_receiving = true;
}

void p2p::set_link_timeout(int timeout) {
  LISTEN_TIMEOUT = timeout;
}

bool p2p::link(const string &connect_ip) {
  if (_is_linked) return false;

  // ----------- REQUEST CONNECTION ---------
  _transmit_addr.sin_family = AF_INET;
  _transmit_addr.sin_port = htons(PORT);
  _transmit_addr.sin_addr.S_un.S_addr = inet_addr(connect_ip.c_str());
  
  _transmit = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (connect(_transmit, (sockaddr*)(&_transmit_addr), sizeof(_transmit_addr)) != 0) {
    // Connect Failed
    _transmit = INVALID_SOCKET;
    
    // ----------- AWAIT CONNECTION -----------
    _listen_addr.sin_family = AF_INET;
    _listen_addr.sin_port = htons(PORT);
    _listen_addr.sin_addr.S_un.S_addr = inet_addr(_machine_ip.c_str());

    _listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    bind(_listen, (sockaddr*)(&_listen_addr), sizeof(_listen_addr));
    
    // enable non-blocking for listen-sock:
    u_long l_sock_mode = 1;
    ioctlsocket(_listen, FIONBIO, &l_sock_mode);
    time_t start_time = time(0);
    
    
    int transmit_addr_size = sizeof(_transmit_addr);
    listen(_listen, SOMAXCONN);
    _transmit = accept(_listen, (sockaddr*)&_transmit_addr, &transmit_addr_size);
    while ((_transmit==SOCKET_ERROR) && ((LISTEN_TIMEOUT < 0) || ((time(0)-start_time) < LISTEN_TIMEOUT)) && (!_abort_listening)) {
      if (WSAGetLastError() != WSAEWOULDBLOCK) break;
      Sleep(250);
      _transmit = accept(_listen, (sockaddr*)&_transmit_addr, &transmit_addr_size);
    }
    _abort_listening = false;

    closesocket(_listen);
    _listen = INVALID_SOCKET;
    if (_transmit == INVALID_SOCKET) {
      _is_linked = false;
      return false;
    }
    else _is_linked = true;
    // ----------------------------------------

  }
  else {
    _listen = INVALID_SOCKET;
    _is_linked = true;
  }
  // ----------------------------------------
  
  // enable non-blocking:
  if (_is_linked) {
    u_long t_sock_mode = 1;
    ioctlsocket(_transmit, FIONBIO, &t_sock_mode);
  }
  
  return _is_linked;
}

void p2p::abort_link() {
  if (_listen != INVALID_SOCKET) _abort_listening = true;
  else _abort_listening = false;
}

bool p2p::transmit(const string& data) {
  if (_is_linked) return (send(_transmit, data.c_str(), data.length(), 0) > 0);
  else return false;
}
bool p2p::transmit(const transmission& data) {return transmit(data.value());}

string p2p::get_local_ip() {
  return _machine_ip;
}

string p2p::get_remote_ip() {
  if (_is_linked) return inet_ntoa((in_addr)_transmit_addr.sin_addr);
  else return "0.0.0.0";
}

bool p2p::is_linked() {
  return _is_linked;
}

void p2p::id(int x) {
  _id = x;
}

int p2p::id() {
  return _id;
}
