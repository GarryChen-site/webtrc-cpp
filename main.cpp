#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// WebRTC related headers
#include <api/create_peerconnection_factory.h>
#include <api/peer_connection_interface.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <system_wrappers/include/field_trial.h>

// --- Observer Classes ---
class Wrapper;
class PeerConnectionObserverProcy : public webrtc::PeerConnectionObserver {
  Wrapper &parent;

public:
  PeerConnectionObserverProcy(Wrapper &p) : parent(p) {}
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    std::cout << "[PCO] SignalingState Change: " << new_state
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
    std::cout << "[PCO] DataChannel received!" << std::endl;
  }

  void OnRenegotiationNeeded() override {
    std::cout << "[PCO] Renegotiation needed!" << std::endl;
  }

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    std::cout << "[PCO] IceConnectionState Change: " << new_state << std::endl;
  }
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    std::cout << "[PCO] IceGatheringState Change: " << new_state << std::endl;
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override {
    std::cout << "[PCO] IceCandidate found!" << std::endl;
  }
};

class CreateSessionDescriptionObserverProxy
    : public webrtc::CreateSessionDescriptionObserver {
  Wrapper &parent;

public:
  CreateSessionDescriptionObserverProxy(Wrapper &p) : parent(p) {}
  void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
  void OnFailure(webrtc::RTCError error) override {
    std::cerr << "[CSDO] Failure: " << error.message() << std::endl;
  }
};

class SetSessionDescriptionObserverProxy
    : public webrtc::SetSessionDescriptionObserver {
public:
  SetSessionDescriptionObserverProxy() {}
  void OnSuccess() override {
    std::cout << "[SSDO] Success (Description Set)" << std::endl;
  }
  void OnFailure(webrtc::RTCError error) override {
    std::cerr << "[SSDO] Failure: " << error.message() << std::endl;
  }
};

// --- Wrapper Implementation ---

class Wrapper {
public:
  std::unique_ptr<rtc::Thread> network_thread;
  std::unique_ptr<rtc::Thread> worker_thread;
  std::unique_ptr<rtc::Thread> signaling_thread;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcf;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc;

  // Observers
  PeerConnectionObserverProcy pco{*this};
  rtc::scoped_refptr<CreateSessionDescriptionObserverProxy> csdo;
  rtc::scoped_refptr<SetSessionDescriptionObserverProxy> ssdo;

  std::function<void(const std::string &)> on_sdp_callback;

  Wrapper() {
    csdo =
        new rtc::RefCountedObject<CreateSessionDescriptionObserverProxy>(*this);
    ssdo = new rtc::RefCountedObject<SetSessionDescriptionObserverProxy>();
  }

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

  void create_offer() {
    std::cout << "Creating Offer..." << std::endl;
    webrtc::PeerConnectionInterface::RTCConfiguration config;

    pc = pcf->CreatePeerConnection(config, nullptr, nullptr, &pco);
    if (!pc) {
      std::cerr << "Failed to create PeerConnection!" << std::endl;
      return;
    }

    pc->CreateOffer(csdo,
                    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }

  void create_answer(const std::string &remote_sdp) {
    std::cout << "Creating Answer..." << std::endl;
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    pc = pcf->CreatePeerConnection(config, nullptr, nullptr, &pco);

    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface *session_desc =
        webrtc::CreateSessionDescription("offer", remote_sdp, &error);

    if (!session_desc) {
      std::cerr << "Failed to parse remote SDP: " << error.description
                << std::endl;
      return;
    }

    pc->SetRemoteDescription(ssdo, session_desc);
    pc->CreateAnswer(csdo,
                     webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }

  void set_remote_answer(const std::string &remote_rdp) {
    std::cout << "Setting Remote Answer..." << std::endl;
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface *session_desc =
        webrtc::CreateSessionDescription("answer", remote_rdp, &error);

    if (!session_desc) {
      std::cerr << "Failed to parse remote Answer: " << error.description
                << std::endl;
      return;
    }
    pc->SetRemoteDescription(ssdo, session_desc);
  }

  void on_local_sdp_ready(webrtc::SessionDescriptionInterface *desc) {
    std::string sdp;
    desc->ToString(&sdp);
    pc->SetLocalDescription(ssdo, desc);
    if (on_sdp_callback) {
      on_sdp_callback(sdp);
    }
  }

  void cleanup() {
    std::cout << "Cleaning up Wrapper..." << std::endl;
    if (pc) {
      pc->Close();
    }
    pc = nullptr;
    pcf = nullptr;
    network_thread->Stop();
    worker_thread->Stop();
    signaling_thread->Stop();
  }
};

void CreateSessionDescriptionObserverProxy::OnSuccess(
    webrtc::SessionDescriptionInterface *desc) {
  std::cout << "[CSDO] Success (Description Created)" << std::endl;
  parent.on_local_sdp_ready(desc);
}

// --- Main Program ---

int main(int argc, char *argv[]) {

  webrtc::field_trial::InitFieldTrialsFromString("");
  rtc::InitializeSSL();

  std::cout << "--- WebRTC C++ Reconstruction ---" << std::endl;

  Wrapper rtc_wrapper;
  rtc_wrapper.init();

  rtc_wrapper.on_sdp_callback = [](const std::string &sdp) {
    std::cout << "\n--- SDP START ---" << std::endl;
    std::cout << sdp;
    std::cout << "--- SDP END ---\n" << std::endl;
  };

  std::cout
      << "Commands: 'sdp1' (Offer), 'sdp2' (Answer), 'sdp3' (SetAnswer), 'quit'"
      << std::endl;

  std::string line;
  std::string command;
  std::string parameter;
  bool collecting_param = false;

  while (std::getline(std::cin, line)) {
    if (!collecting_param) {
      if (line == "quit")
        break;
      if (line == "sdp1") {
        rtc_wrapper.create_offer();
      } else if (line == "sdp2" || line == "sdp3") {
        command = line;
        collecting_param = true;
        parameter = "";
        std::cout << "Pated SDP then type ';' on a new line:" << std::endl;
      }
    } else {
      if (line == ";") {
        collecting_param = false;
        if (command == "sdp2") {
          rtc_wrapper.create_answer(parameter);
        } else if (command == "sdp3") {
          rtc_wrapper.set_remote_answer(parameter);
        }
      } else {
        parameter += line + "\n";
      }
    }
  }

  rtc_wrapper.cleanup();
  rtc::CleanupSSL();

  std::cout << "Exit successful." << std::endl;

  return 0;
}