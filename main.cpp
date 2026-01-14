#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// WebRTC related headers
#include "api/data_channel_interface.h"
#include <api/create_peerconnection_factory.h>
#include <api/peer_connection_interface.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <system_wrappers/include/field_trial.h>

#include "TinyJson.hpp"

// --- Data Structures ---
struct Ice {
  std::string candidate;
  std::string sdp_mid;
  int sdp_mline_index;
};

// --- Observer Classes ---
class Wrapper;

class DataChannelObserverProxy : public webrtc::DataChannelObserver {
  Wrapper &parent;

public:
  DataChannelObserverProxy(Wrapper &p) : parent(p) {}

  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer &buffer) override;
  void OnBufferedAmountChange(uint64_t previous_amount) override {}
};
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
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;

  void OnRenegotiationNeeded() override {
    std::cout << "[PCO] Renegotiation needed!" << std::endl;
  }

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
    const char *state_str = "Unknown";
    switch (new_state) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      state_str = "New";
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      state_str = "Checking";
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      state_str = "Connected";
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      state_str = "Completed";
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      state_str = "Failed";
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      state_str = "Disconnected";
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      state_str = "Closed";
      break;
    default:
      break;
    }
    std::cout << "[PCO] IceConnectionState Change: " << state_str << " ("
              << new_state << ")" << std::endl;
  }
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    const char *state_str = "Unknown";
    switch (new_state) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      state_str = "New";
      break;
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      state_str = "Gathering";
      break;
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      state_str = "Complete";
      break;
    default:
      break;
    }
    std::cout << "[PCO] IceGatheringState Change: " << state_str << " ("
              << new_state << ")" << std::endl;
  }
  void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
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
  rtc::scoped_refptr<webrtc::DataChannelInterface> dc;

  // Observers
  PeerConnectionObserverProcy pco{*this};
  DataChannelObserverProxy dco{*this};
  rtc::scoped_refptr<CreateSessionDescriptionObserverProxy> csdo;
  rtc::scoped_refptr<SetSessionDescriptionObserverProxy> ssdo;

  std::function<void(const std::string &)> on_sdp_callback;
  std::function<void(const Ice &)> on_ice_callback;

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
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    config.servers.push_back(ice_server);

    pc = pcf->CreatePeerConnection(config, nullptr, nullptr, &pco);
    if (!pc) {
      std::cerr << "Failed to create PeerConnection!" << std::endl;
      return;
    }

    // Create DataChannel
    webrtc::DataChannelInit dc_config;
    dc = pc->CreateDataChannel("sample_channel", &dc_config);
    dc->RegisterObserver(&dco);

    pc->CreateOffer(csdo,
                    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
  }

  void create_answer(const std::string &remote_sdp) {
    std::cout << "Creating Answer..." << std::endl;
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    config.servers.push_back(ice_server);

    pc = pcf->CreatePeerConnection(config, nullptr, nullptr, &pco);
    if (!pc) {
      std::cerr << "Failed to create PeerConnection!" << std::endl;
      return;
    }

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

  void add_ice_candidate(const Ice &ice_it) {
    webrtc::SdpParseError error;
    webrtc::IceCandidateInterface *ice = webrtc::CreateIceCandidate(
        ice_it.sdp_mid, ice_it.sdp_mline_index, ice_it.candidate, &error);

    if (!ice) {
      std::cerr << "[Wrapper] Failed to create ICE candidate: "
                << error.description << std::endl;
      return;
    }
    if (!pc->AddIceCandidate(ice)) {
      std::cerr << "[Wrapper] Failed to add ICE candidate" << std::endl;
    } else {
      std::cout << "[Wrapper] Successfully added remote ICE candidate."
                << std::endl;
    }
  }

  void send_message(const std::string &msg) {
    if (!dc || dc->state() != webrtc::DataChannelInterface::kOpen) {
      std::cerr << "DataChannel is not open!" << std::endl;
      return;
    }
    webrtc::DataBuffer buffer(msg);
    dc->Send(buffer);
  }

  void on_local_sdp_ready(webrtc::SessionDescriptionInterface *desc) {
    std::string sdp;
    desc->ToString(&sdp);
    pc->SetLocalDescription(ssdo, desc);
    if (on_sdp_callback) {
      on_sdp_callback(sdp);
    }
  }

  void on_local_ice_ready(const webrtc::IceCandidateInterface *candidate) {
    Ice ice;
    candidate->ToString(&ice.candidate);
    ice.sdp_mid = candidate->sdp_mid();
    ice.sdp_mline_index = candidate->sdp_mline_index();
    if (on_ice_callback) {
      on_ice_callback(ice);
    }
  }

  void cleanup() {
    std::cout << "Cleaning up Wrapper..." << std::endl;
    if (dc) {
      dc->UnregisterObserver();
      dc = nullptr;
    }
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

void PeerConnectionObserverProcy::OnIceCandidate(
    const webrtc::IceCandidateInterface *candidate) {
  std::cout << "[PCO] IceCandidate found!" << std::endl;
  parent.on_local_ice_ready(candidate);
}

void PeerConnectionObserverProcy::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  std::cout << "[PCO] DataChannel received!" << std::endl;
  parent.dc = data_channel;
  parent.dc->RegisterObserver(&parent.dco);
}

void CreateSessionDescriptionObserverProxy::OnSuccess(
    webrtc::SessionDescriptionInterface *desc) {
  std::cout << "[CSDO] Success (Description Created)" << std::endl;
  parent.on_local_sdp_ready(desc);
}

void DataChannelObserverProxy::OnStateChange() {
  if (parent.dc) {
    const char *state_str = "Unknown";
    switch (parent.dc->state()) {
    case webrtc::DataChannelInterface::kConnecting:
      state_str = "Connecting";
      break;
    case webrtc::DataChannelInterface::kOpen:
      state_str = "Open";
      break;
    case webrtc::DataChannelInterface::kClosing:
      state_str = "Closing";
      break;
    case webrtc::DataChannelInterface::kClosed:
      state_str = "Closed";
      break;
    }
    std::cout << "[DCO] DataChannel State Change: " << state_str << std::endl;
    if (parent.dc->state() == webrtc::DataChannelInterface::kOpen) {
      std::cout
          << "\n>>> CONNECTION READY! You can now use the 'send' command <<<\n"
          << std::endl;
    }
  }
}

void DataChannelObserverProxy::OnMessage(const webrtc::DataBuffer &buffer) {
  std::string msg(reinterpret_cast<const char *>(buffer.data.data()),
                  buffer.data.size());
  std::cout << "\n[DCO] Received Message: " << msg << std::endl;
}

// --- Main Program ---

int main(int argc, char *argv[]) {

  webrtc::field_trial::InitFieldTrialsFromString("");
  rtc::InitializeSSL();

  std::cout << "--- WebRTC C++ Reconstruction ---" << std::endl;

  Wrapper rtc_wrapper;
  std::list<Ice> local_ice_list;

  rtc_wrapper.on_ice_callback = [&](const Ice &ice) {
    local_ice_list.push_back(ice);
  };

  rtc_wrapper.on_sdp_callback = [](const std::string &sdp) {
    std::cout << "\n--- SDP START ---" << std::endl;
    std::cout << sdp;
    std::cout << "--- SDP END ---\n" << std::endl;
  };

  rtc_wrapper.init();

  std::cout
      << "Commands: 'sdp1' (Offer), 'sdp2' (Answer), 'sdp3' (SetAnswer),\n"
      << "          'ice1' (Show Local ICE), 'ice2' (Add Remote ICE),\n"
      << "          'send' (Send Message), 'quit'" << std::endl;

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
      } else if (line == "ice1") {
        auto ice_arr = tinyjson::Value::array();
        for (const auto &ice : local_ice_list) {
          auto ice_obj = tinyjson::Value::object();
          ice_obj.set("candidate", ice.candidate);
          ice_obj.set("sdp_mid", ice.sdp_mid);
          ice_obj.set("sdp_mline_index", ice.sdp_mline_index);
          ice_arr.push(ice_obj);
        }
        std::cout << "\n--- ICE CANDIDATES START ---\n"
                  << ice_arr.serialize() << "\n--- ICE CANDIDATES END ---\n"
                  << std::endl;
        local_ice_list.clear();
      } else if (line == "sdp2" || line == "sdp3" || line == "ice2" ||
                 line == "send") {
        command = line;
        collecting_param = true;
        parameter = "";
        if (line == "send")
          std::cout << "Enter message: ";
        else
          std::cout << "Paste parameter then type ';' on a new line:"
                    << std::endl;
      }
    } else {
      if (command == "send") {
        rtc_wrapper.send_message(line);
        collecting_param = false;
      } else if (line == ";") {
        collecting_param = false;
        if (command == "sdp2")
          rtc_wrapper.create_answer(parameter);
        else if (command == "sdp3")
          rtc_wrapper.set_remote_answer(parameter);
        else if (command == "ice2") {
          tinyjson::Parser parser;
          tinyjson::Value v = parser.parse(parameter);
          if (v.type() == tinyjson::Type::Array) {
            const auto &arr = std::get<tinyjson::Array>(v.data);
            for (const auto &item : arr) {
              if (item.type() == tinyjson::Type::Object) {
                const auto &obj = std::get<tinyjson::Object>(item.data);
                Ice ice;
                ice.candidate = std::get<std::string>(obj.at("candidate").data);
                ice.sdp_mid = std::get<std::string>(obj.at("sdp_mid").data);
                ice.sdp_mline_index = static_cast<int>(
                    std::get<double>(obj.at("sdp_mline_index").data));
                rtc_wrapper.add_ice_candidate(ice);
              }
            }
          }
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