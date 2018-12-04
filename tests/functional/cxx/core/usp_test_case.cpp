//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#include <chrono>
#include <thread>
#include <random>
#include <string>

#include "exception.h"
#define __SPX_THROW_HR_IMPL(hr) Microsoft::CognitiveServices::Speech::Impl::ThrowWithCallstack(hr)

#include "thread_service.h"
#include "test_utils.h"
#include "usp.h"
#include "guid_utils.h"

using namespace std;
using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;

class UspClient : public USP::Callbacks, public std::enable_shared_from_this<UspClient>
{
    USP::EndpointType m_endpoint;
    USP::RecognitionMode m_mode;

public:
    UspClient(USP::EndpointType endpoint = USP::EndpointType::Speech,
        USP::RecognitionMode mode = USP::RecognitionMode::Interactive)
        : m_endpoint(endpoint), m_mode(mode)
    {
    }

    void Init()
    {
        auto region = Config::Region.empty() ? "westus" : Config::Region;
        m_threadService = std::make_shared<CSpxThreadService>();
        m_threadService->Init();
        auto client = USP::Client(shared_from_this(), m_endpoint, PAL::CreateGuidWithoutDashes(), m_threadService)
            .SetRecognitionMode(m_mode)
            .SetRegion(region)
            .SetAuthentication(USP::AuthenticationType::SubscriptionKey, Keys::Speech);
        if (!Config::Endpoint.empty())
        {
            client.SetEndpointType(USP::EndpointType::Speech).SetEndpointUrl(Config::Endpoint);
        }

        m_connection = client.Connect();
    }

    void Term()
    {
        m_threadService->Term();
    }

    virtual void OnError(bool /*transport*/, USP::ErrorCode errorCode, const std::string& errorMessage) override
    {
        (void)errorCode;
        FAIL(errorMessage);
    }

    template <class T>
    void WriteAudio(T* buffer, size_t size)
    {
        m_connection->WriteAudio(reinterpret_cast<const uint8_t*>(buffer), size);
    }

private:
    USP::ConnectionPtr m_connection;
    std::shared_ptr<CSpxThreadService> m_threadService;
};

using UspClientPtr = std::shared_ptr<UspClient>;

TEST_CASE("USP is properly functioning", "[usp]")
{
    SECTION("usp can be initialized, connected and closed")
    {
        auto client = std::make_shared<UspClient>();
        REQUIRE_NOTHROW(client->Init());
        REQUIRE_NOTHROW(client->Term());
    }

    string input_file{ Config::InputDir + "/audio/whatstheweatherlike.wav" };
    REQUIRE(exists(input_file));

    SECTION("usp can be used to upload binary data")
    {
        string dummy = "RIFF1234567890";
        auto client = std::make_shared<UspClient>();
        REQUIRE_NOTHROW(client->Init());
        client->WriteAudio(dummy.data(), dummy.length());
        REQUIRE_NOTHROW(client->Term());
    }

    random_engine rnd(12345);
    size_t buffer_size_8k = 1 << 13;
    vector<char> buffer(buffer_size_8k);

    SECTION("usp can be used to upload audio from file")
    {
        auto client = std::make_shared<UspClient>();
        client->Init();
        auto is = get_stream(input_file);

        while (is) {
            auto size_to_read = max(size_t(1 << 10), rnd() % buffer_size_8k);
            is.read(buffer.data(), size_to_read);
            auto bytesRead = (size_t)is.gcount();
            client->WriteAudio(buffer.data(), bytesRead);
            std::this_thread::sleep_for(std::chrono::milliseconds(rnd() % 100));
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
        REQUIRE_NOTHROW(client->Term());
    }

    SECTION("usp can toggled on/off multiple times in a row")
    {
        for (unsigned int i = 10; i > 0; i--) 
        {
            auto client = std::make_shared<UspClient>();
            client->Init();
            auto is = get_stream(input_file);
            while (is && (rnd()%i < i>>1)) {
                is.read(buffer.data(), buffer_size_8k);
                auto bytesRead = (size_t)is.gcount();
                client->WriteAudio(buffer.data(), bytesRead);
                std::this_thread::sleep_for(std::chrono::milliseconds(rnd() % 100));
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
            REQUIRE_NOTHROW(client->Term());
        }
    }

    SECTION("several usp clients can coexist peacefully")
    {
        int num_handles = 10;
        vector<UspClientPtr> clients(num_handles);
        for (int i = 0; i < num_handles; ++i)
        {
            clients[i] = std::make_shared<UspClient>();
            clients[i]->Init();
        }

        auto is = get_stream(input_file);
        is.read(buffer.data(), buffer_size_8k);
        REQUIRE(is.good());

        for (int i = 0; i < num_handles; i++)
        {
            auto bytesRead = (size_t)is.gcount();
            clients[i]->WriteAudio(buffer.data(), bytesRead);
        }

        while (is)
        {
            auto size_to_read = max(size_t(1 << 10), rnd() % buffer_size_8k);
            is.read(buffer.data(), size_to_read);
            auto bytesRead = (size_t)is.gcount();
            clients[rnd() % num_handles]->WriteAudio(buffer.data(), bytesRead);
            std::this_thread::sleep_for(std::chrono::milliseconds(rnd() % 100));
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
        for (int i = 0; i < num_handles; i++)
        {
            REQUIRE_NOTHROW(clients[i]->Term());
        }
    }
}

class TlsCheck : public USP::Callbacks
{
    void OnError(bool /*transport*/, USP::ErrorCode /*errorCode*/, const std::string& errorMessage) override
    {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6237)
        // Disable: (<zero> && <expression>) is always zero.  <expression> is never evaluated and might have side effects.
#endif
        REQUIRE(errorMessage == "WebSocket Upgrade failed with HTTP status code: 301");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    }
};

TEST_CASE("USP uses TLS12", "[usp]")
{
    // GitHub doesn't allow TLSv1 and TLSv1.1 since February 2018 (https://githubengineering.com/crypto-removal-notice/).
    auto service = std::make_shared<CSpxThreadService>();
    service->Init();
    auto callbacks = std::make_shared<TlsCheck>();
    auto client = USP::Client(callbacks, USP::EndpointType::Speech, PAL::CreateGuidWithoutDashes(), service)
        .SetRegion("westus")
        .SetEndpointUrl("wss://www.github.com/")
        .SetAuthentication(USP::AuthenticationType::SubscriptionKey, "test");

    auto connection = client.Connect();
    std::vector<uint8_t> buffer = { 1, 2, 3, 4, 5, 6, 7 };
    connection->WriteAudio(buffer.data(), buffer.size());
    this_thread::sleep_for(5s);
}
