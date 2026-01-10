#include "TinyJson.hpp"
#include <iostream>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/field_trial.h>

int main(int argc, char *argv[]) {

  webrtc::field_trial::InitFieldTrialsFromString("");
  rtc::InitializeSSL();

  std::cout << "--- WebRTC C++ Reconstruction ---" << std::endl;

  auto test_object = tinyjson::Value::object();
  test_object.set("name", "John");
  test_object.set("age", 30);
  std::cout << "Serialized: " << test_object.serialize() << std::endl;

  rtc::CleanupSSL();

  return 0;
}