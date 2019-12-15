#include  "IceServer.h"

#include <iostream>

static constexpr size_t StunSerializeBufferSize{ 65536 };
static uint8_t StunSerializeBuffer[StunSerializeBufferSize];
IceServer::IceServer()
{
}
IceServer::~IceServer()
{
}
 IceServer::IceServer(const std::string& usernameFragment, const std::string& password)
	:usernameFragment(usernameFragment), password(password) {

}

void IceServer::ProcessStunPacket(RTC::StunPacket * packet, sockaddr_in * remoteAddr)
{
	// Must be a Binding method.
	if (packet->GetMethod() != RTC::StunPacket::Method::BINDING)
	{
		if (packet->GetClass() == RTC::StunPacket::Class::REQUEST)
		{
			std::cout << "unknown method %#.3x in STUN Request => 400" <<
				static_cast<unsigned int>(packet->GetMethod())<<std::endl;

			// Reply 400.
			RTC::StunPacket* response = packet->CreateErrorResponse(400);

			response->Serialize(StunSerializeBuffer);
			if (m_send_cb) {
				m_send_cb((char*)StunSerializeBuffer, response->GetSize(), remoteAddr);
			}
			delete response;
		}
		else
		{
			std::cout << "ignoring STUN Indication or Response with unknown method %#.3x"<<
				static_cast<unsigned int>(packet->GetMethod())<<std::endl;
		}

		return;
	}

	// Must use FINGERPRINT (optional for ICE STUN indications).
	if (!packet->HasFingerprint() && packet->GetClass() != RTC::StunPacket::Class::INDICATION)
	{
		if (packet->GetClass() == RTC::StunPacket::Class::REQUEST)
		{
			std::cout << "STUN Binding Request without FINGERPRINT => 400";

			// Reply 400.
			RTC::StunPacket* response = packet->CreateErrorResponse(400);

			response->Serialize(StunSerializeBuffer);
			if (m_send_cb) {
				m_send_cb((char*)StunSerializeBuffer, response->GetSize(), remoteAddr);
			}
			delete response;
		}
		else
		{
			std::cout << "ignoring STUN Binding Response without FINGERPRINT"<<std::endl;
		}

		return;
	}

	switch (packet->GetClass())
	{
	case RTC::StunPacket::Class::REQUEST:
	{
		// USERNAME, MESSAGE-INTEGRITY and PRIORITY are required.
		if (!packet->HasMessageIntegrity() || (packet->GetPriority() == 0u) || packet->GetUsername().empty())
		{
			std::cout << "mising required attributes in STUN Binding Request => 400" << std::endl;

			// Reply 400.
			RTC::StunPacket* response = packet->CreateErrorResponse(400);

			response->Serialize(StunSerializeBuffer);
			if (m_send_cb) {
				m_send_cb((char*)StunSerializeBuffer, response->GetSize(), remoteAddr);
			}
			delete response;

			return;
		}

		// Check authentication.
		switch (packet->CheckAuthentication(this->usernameFragment, this->password))
		{
		case RTC::StunPacket::Authentication::OK:
		{
			if (!this->oldPassword.empty())
			{
				std::cout << "new ICE credentials applied" << std::endl;

				this->oldUsernameFragment.clear();
				this->oldPassword.clear();
			}

			break;
		}

		case RTC::StunPacket::Authentication::UNAUTHORIZED:
		{
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
				std::cout << "using old ICE credentials" << std::endl;

				break;
			}
			std::cout << "wrong authentication in STUN Binding Request => 401" << std::endl;

			// Reply 401.
			RTC::StunPacket* response = packet->CreateErrorResponse(401);

			response->Serialize(StunSerializeBuffer);
			if (m_send_cb) {
				m_send_cb((char*)StunSerializeBuffer, response->GetSize(), remoteAddr);
			}
			delete response;

			return;
		}

		case RTC::StunPacket::Authentication::BAD_REQUEST:
		{
			std::cout << "cannot check authentication in STUN Binding Request => 400" << std::endl;

			// Reply 400.
			RTC::StunPacket* response = packet->CreateErrorResponse(400);

			response->Serialize(StunSerializeBuffer);
			if (m_send_cb) {
				m_send_cb((char*)StunSerializeBuffer, response->GetSize(), remoteAddr);
			}
			delete response;

			return;
		}
		}

		// NOTE: Should be rejected with 487, but this makes Chrome happy:
		//   https://bugs.chromium.org/p/webrtc/issues/detail?id=7478
		// The remote peer must be ICE controlling.
		// if (packet->GetIceControlled())
		// {
		// 	MS_WARN_TAG(ice, "peer indicates ICE-CONTROLLED in STUN Binding Request => 487");
		//
		// 	// Reply 487 (Role Conflict).
		// 	RTC::StunPacket* response = packet->CreateErrorResponse(487);
		//
		// 	response->Serialize(StunSerializeBuffer);
		// 	this->listener->OnIceServerSendStunPacket(this, response, tuple);
		//
		// 	delete response;
		//
		// 	return;
		// }

		std::cout << "processing STUN Binding Request [Priority:, UseCandidate:%s]"<<
			static_cast<uint32_t>(packet->GetPriority())<<
			(packet->HasUseCandidate() ? "true" : "false") << std::endl;

		// Create a success response.
		RTC::StunPacket* response = packet->CreateSuccessResponse();

		// Add XOR-MAPPED-ADDRESS.
		//response->SetXorMappedAddress(tuple->GetRemoteAddress());
		response->SetXorMappedAddress((struct sockaddr *)remoteAddr);

		// Authenticate the response.
		if (this->oldPassword.empty())
			response->Authenticate(this->password);
		else
			response->Authenticate(this->oldPassword);

		// Send back.
		response->Serialize(StunSerializeBuffer);
		if (m_send_cb) {
			m_send_cb((char*)StunSerializeBuffer, response->GetSize(), remoteAddr);
		}
		delete response;

		// Handle the tuple.
		HandleTuple(remoteAddr, packet->HasUseCandidate());

		break;
	}

	case RTC::StunPacket::Class::INDICATION:
	{
		std::cout << "STUN Binding Indication processed" << std::endl;

		break;
	}

	case RTC::StunPacket::Class::SUCCESS_RESPONSE:
	{
		std::cout << "STUN Binding Success Response processed" << std::endl;

		break;
	}

	case RTC::StunPacket::Class::ERROR_RESPONSE:
	{
		std::cout << "STUN Binding Error Response processed"<<std::endl;

		break;
	}
	}
}

void IceServer::HandleTuple(sockaddr_in * remoteAddr, bool hasUseCandidate)
{
	m_remoteAddr = *remoteAddr;
	if (hasUseCandidate) {
		this->state = IceState::COMPLETED;
	}
	if (m_IceServerCompletedCB) {
		m_IceServerCompletedCB();
	}
}



const std::string& IceServer::GetUsernameFragment() const
{
	return this->usernameFragment;
}

const std::string& IceServer::GetPassword() const
{
	return this->password;
}

inline void IceServer::SetUsernameFragment(const std::string& usernameFragment)
{
	this->oldUsernameFragment = this->usernameFragment;
	this->usernameFragment = usernameFragment;
}

inline void IceServer::SetPassword(const std::string& password)
{
	this->oldPassword = this->password;
	this->password = password;
}

inline IceServer::IceState IceServer::GetState() const
{
	return this->state;
}