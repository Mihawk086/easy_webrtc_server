#define MS_CLASS "RTC::IceServer"
// #define MS_LOG_DEV_LEVEL 3

#include "rtc/ice_server.h"

#include <utility>

#include "common/logger.h"

namespace RTC {
/* Static. */

static constexpr size_t StunSerializeBufferSize{65536};
static uint8_t StunSerializeBuffer[StunSerializeBufferSize];

/* Instance methods. */

IceServer::IceServer(Listener* listener, const std::string& usernameFragment,
                     const std::string& password)
    : listener(listener), usernameFragment(usernameFragment), password(password) {
  MS_TRACE();
}

void IceServer::ProcessStunPacket(RTC::StunPacket* packet, RTC::TransportTuple* tuple) {
  MS_TRACE();

  // Must be a Binding method.
  if (packet->GetMethod() != RTC::StunPacket::Method::BINDING) {
    if (packet->GetClass() == RTC::StunPacket::Class::REQUEST) {
      MS_WARN_TAG(ice, "unknown method %#.3x in STUN Request => 400",
                  static_cast<unsigned int>(packet->GetMethod()));

      // Reply 400.
      RTC::StunPacket* response = packet->CreateErrorResponse(400);

      response->Serialize(StunSerializeBuffer);
      this->listener->OnIceServerSendStunPacket(this, response, tuple);

      delete response;
    } else {
      MS_WARN_TAG(ice, "ignoring STUN Indication or Response with unknown method %#.3x",
                  static_cast<unsigned int>(packet->GetMethod()));
    }

    return;
  }

  // Must use FINGERPRINT (optional for ICE STUN indications).
  if (!packet->HasFingerprint() && packet->GetClass() != RTC::StunPacket::Class::INDICATION) {
    if (packet->GetClass() == RTC::StunPacket::Class::REQUEST) {
      MS_WARN_TAG(ice, "STUN Binding Request without FINGERPRINT => 400");

      // Reply 400.
      RTC::StunPacket* response = packet->CreateErrorResponse(400);

      response->Serialize(StunSerializeBuffer);
      this->listener->OnIceServerSendStunPacket(this, response, tuple);

      delete response;
    } else {
      MS_WARN_TAG(ice, "ignoring STUN Binding Response without FINGERPRINT");
    }

    return;
  }

  switch (packet->GetClass()) {
    case RTC::StunPacket::Class::REQUEST: {
      // USERNAME, MESSAGE-INTEGRITY and PRIORITY are required.
      if (!packet->HasMessageIntegrity() || (packet->GetPriority() == 0u) ||
          packet->GetUsername().empty()) {
        MS_WARN_TAG(ice, "mising required attributes in STUN Binding Request => 400");

        // Reply 400.
        RTC::StunPacket* response = packet->CreateErrorResponse(400);

        response->Serialize(StunSerializeBuffer);
        this->listener->OnIceServerSendStunPacket(this, response, tuple);

        delete response;

        return;
      }

      // Check authentication.
      switch (packet->CheckAuthentication(this->usernameFragment, this->password)) {
        case RTC::StunPacket::Authentication::OK: {
          if (!this->oldPassword.empty()) {
            MS_DEBUG_TAG(ice, "new ICE credentials applied");

            this->oldUsernameFragment.clear();
            this->oldPassword.clear();
          }

          break;
        }

        case RTC::StunPacket::Authentication::UNAUTHORIZED: {
          // We may have changed our usernameFragment and password, so check
          // the old ones.
          // clang-format off
						if (
							!this->oldUsernameFragment.empty() &&
							!this->oldPassword.empty() &&
							packet->CheckAuthentication(this->oldUsernameFragment, this->oldPassword) == RTC::StunPacket::Authentication::OK
						)
          // clang-format on
          {
            MS_DEBUG_TAG(ice, "using old ICE credentials");

            break;
          }

          MS_WARN_TAG(ice, "wrong authentication in STUN Binding Request => 401");

          // Reply 401.
          RTC::StunPacket* response = packet->CreateErrorResponse(401);

          response->Serialize(StunSerializeBuffer);
          this->listener->OnIceServerSendStunPacket(this, response, tuple);

          delete response;

          return;
        }

        case RTC::StunPacket::Authentication::BAD_REQUEST: {
          MS_WARN_TAG(ice, "cannot check authentication in STUN Binding Request => 400");

          // Reply 400.
          RTC::StunPacket* response = packet->CreateErrorResponse(400);

          response->Serialize(StunSerializeBuffer);
          this->listener->OnIceServerSendStunPacket(this, response, tuple);

          delete response;

          return;
        }
      }

      // // The remote peer must be ICE controlling.
      // if (packet->GetIceControlled()) {
      //   MS_WARN_TAG(ice, "peer indicates ICE-CONTROLLED in STUN Binding Request => 487");

      //   // Reply 487 (Role Conflict).
      //   RTC::StunPacket* response = packet->CreateErrorResponse(487);

      //   response->Serialize(StunSerializeBuffer);
      //   this->listener->OnIceServerSendStunPacket(this, response, tuple);

      //   delete response;

      //   return;
      // }

      MS_DEBUG_DEV("processing STUN Binding Request [Priority:%" PRIu32 ", UseCandidate:%s]",
                   static_cast<uint32_t>(packet->GetPriority()),
                   packet->HasUseCandidate() ? "true" : "false");

      // Create a success response.
      RTC::StunPacket* response = packet->CreateSuccessResponse();

      // Add XOR-MAPPED-ADDRESS.
      response->SetXorMappedAddress((struct sockaddr*)tuple);

      // Authenticate the response.
      if (this->oldPassword.empty())
        response->Authenticate(this->password);
      else
        response->Authenticate(this->oldPassword);

      // Send back.
      response->Serialize(StunSerializeBuffer);
      this->listener->OnIceServerSendStunPacket(this, response, tuple);

      delete response;

      // Handle the tuple.
      // HandleTuple(tuple, packet->HasUseCandidate());
      HandleTuple(tuple);
      break;
    }

    case RTC::StunPacket::Class::INDICATION: {
      MS_DEBUG_TAG(ice, "STUN Binding Indication processed");

      break;
    }

    case RTC::StunPacket::Class::SUCCESS_RESPONSE: {
      MS_DEBUG_TAG(ice, "STUN Binding Success Response processed");

      break;
    }

    case RTC::StunPacket::Class::ERROR_RESPONSE: {
      MS_DEBUG_TAG(ice, "STUN Binding Error Response processed");

      break;
    }
  }
}

void IceServer::HandleTuple(RTC::TransportTuple* tuple) {
  if (this->state != IceState::COMPLETED) {
    this->state = IceState::COMPLETED;
    if (this->listener) {
      this->listener->OnIceServerSelectedTuple(this, tuple);
    }
  }
}

}  // namespace RTC
