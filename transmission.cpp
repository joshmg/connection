// File: transmission.cpp
// Written by Joshua Green

#include "transmission.h"
#include "connection.h"
#include <string>
using namespace std;

void transmission::_format() const {
  _formatted = _server_code;
  _formatted += _data;
  _formatted.resize(TRANSMISSION_SIZE, 0x04);
  _format_valid = true;
}

transmission::transmission() {
  clear();
}
transmission::transmission(const string& data) {
  clear();
  set_server_code(data);
  //set(data);
}

void transmission::clear() {
  _data.clear();
  _formatted.clear();
  _server_code.clear();
  _format_valid = false;
}

void transmission::set_server_code(const std::string& code) {
  _server_code = code;
}

void transmission::set(const string& data) {
  _formatted.clear();
  _data = data;
  _format_valid = false;
}

string transmission::value(bool unpadded) const {
  if (unpadded) return _data;
  else {
    if (!_format_valid) _format();
    return _formatted;
  }
}

transmission& transmission::add(const string& data) {
  string new_value = _data;
  new_value += data;
  set(new_value);
  return (*this);
}

transmission& transmission::add(const transmission& data) {
  string new_value = _data;
  new_value += data.value(true);
  set(new_value);
  return (*this);
}

transmission transmission::operator +(const string& data) const {
  string new_value = _data;
  new_value += data;
  return transmission(new_value);
}

transmission transmission::operator +(const transmission& data) const {
  string new_value = _data;
  new_value += data.value(true);
  return transmission(new_value);
}

transmission& transmission::operator +=(const string& data) {
  string new_value = _data;
  new_value += data;
  set(new_value);
  return (*this);
}

transmission& transmission::operator +=(const transmission& data) {
  string new_value = _data;
  new_value += data.value(true);
  set(new_value);
  return (*this);
}
