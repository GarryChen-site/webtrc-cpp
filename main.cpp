#include "TinyJson.hpp"
#include <api/peer_connection_interface.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <rtc_base/ssl_adapter.h>
#include <system_wrappers/include/field_trial.h>

class PeerConnectionObserverProcy : public webrtc::PeerConnectionObserver {
public:
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    std::cout << "SignalingState Change: " << new_state
              << " on thread: " << std::this_thread::get_id() << std::endl;
  }
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {

  };

  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {

  };

  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
    std::cout << "DataChannel received!" << std::endl;
  }

  void OnRenegotiationNeeded() override {
    std::cout << "Renegotiation needed!" << std::endl;
  }

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    std::cout << "IceConnectionState Change: " << new_state << std::endl;
  }
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    std::cout << "IceGatheringState Change: " << new_state << std::endl;
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override {
    std::cout << "IceCandidate found!" << std::endl;
  }
};

class Wrapper {
public:
  std::unique_ptr<rtc::Thread> network_thread;
  std::unique_ptr<rtc::Thread> worker_thread;
  std::unique_ptr<rtc::Thread> signaling_thread;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf;

  void init() {
    std::cout << "Initializing WebRTC..." << std::endl;

    // 1. Create and start threads
    network_thread = rtc::Thread::CreateWithSocketServer();
    network_thread->Start();

    worker_thread = rtc::Thread::Create();
    worker_thread->Start();

    signaling_thread = rtc::Thread::Create();
    signaling_thread->Start();

    // 2. Create the Factory
    webrtc::PeerConnectionFactoryDependencies deps;
    deps.network_thread = network_thread.get();
    deps.worker_thread = worker_thread.get();
    deps.signaling_thread = signaling_thread.get();

    pcf = webrtc::CreateModularPeerConnectionFactory(std::move(deps));

    if (!pcf) {
      std::cerr << "Failed to initialize PeerConnectionFactory!" << std::endl;
      exit(EXIT_FAILURE);
    }

    std::cout << "PeerConnectionFactory initialized successfully." << std::endl;
  }

  void cleanup() {
    std::cout << "Cleaning up Wrapper..." << std::endl;
    pcf = nullptr;
    network_thread->Stop();
    worker_thread->Stop();
    signaling_thread->Stop();
  }
};

int main(int argc, char *argv[]) {

  webrtc::field_trial::InitFieldTrialsFromString("");
  rtc::InitializeSSL();

  std::cout << "--- WebRTC C++ Reconstruction ---" << std::endl;

  Wrapper rtc_wrapper;
  rtc_wrapper.init();

  std::cout << "WebRTC Factory and Threads are live. Type 'quit' to exit!"
            << std::endl;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "quit")
      break;
  }

  rtc_wrapper.cleanup();
  rtc::CleanupSSL();

  std::cout << "Exit successful." << std::endl;

  return 0;
}