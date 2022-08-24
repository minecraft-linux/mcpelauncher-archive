#pragma once

#include <memory>
#include <msa/client/service_launcher.h>
#include <msa/client/service_client.h>
#if 0
#include <minecraft/Xbox.h>
#endif
#include <cll/event_manager.h>
#include "cll_upload_auth_step.h"

class XboxLiveHelper {

private:
    static std::string const MSA_CLIENT_ID;
    static std::string const MSA_COBRAND_ID;

    static XboxLiveHelper instance;

    msa::client::ServiceLauncher launcher;
    std::unique_ptr<msa::client::ServiceClient> client;
    std::mutex clientMutex;
    std::mutex cllMutex;
    std::unique_ptr<cll::EventManager> cll;
    CllUploadAuthStep cllAuthStep;

    msa::client::ServiceClient* getMsaClientOrNull();

    msa::client::ServiceClient& getMsaClient();

public:
    static XboxLiveHelper& getInstance() {
        return instance;
    }

    static std::string findMsa();

    XboxLiveHelper();

    void shutdown() {
        cll.reset();
        client.reset();
    }


    void invokeMsaAuthFlow(std::function<void (std::string const& cid, std::string const& binaryToken)> success_cb,
                           std::function<void (simpleipc::rpc_error_code, std::string const&)> error_cb);


    simpleipc::client::rpc_call<std::shared_ptr<msa::client::Token>> requestXblToken(std::string const& cid,
                                                                                     bool silent);

    void requestXblToken(std::string const& cid, bool silent,
                         std::function<void (std::string const& cid, std::string const& binaryToken)> success_cb,
                         std::function<void (simpleipc::rpc_error_code, std::string const&)> error_cb);


    std::string getCllMsaToken(std::string const& cid);

    static std::string getCllXToken(bool refresh);

    static std::string getCllXTicket(std::string const& xuid);

    void initCll(std::string const& cid = std::string());

    void logCll(cll::Event const& event);
};