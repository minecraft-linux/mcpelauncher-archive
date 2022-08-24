#pragma once

#include <memory>
#include <vector>
#include <stdexcept>
#include "std/string.h"

namespace web {
namespace json {

class value {

private:
    int filler;

public:
    value();
    value(mcpe::string);
    /// @symbol _ZN3web4json5valueC2ERKS1_
    value(web::json::value const&);
    /// @symbol _ZN3web4json5valueC2EOS1_
    value(web::json::value&&);

    /// @symbol _ZN3web4json5valueixERKSs
    web::json::value& operator[](mcpe::string const&);

    /// @symbol _ZN3web4json5valueaSEOS1_
    web::json::value& operator=(web::json::value&&);

    mcpe::string to_string() const;

    void erase(mcpe::string const&);

};

}
}

namespace xbox {
namespace services {

void* xbox_services_error_code_category();

template <typename T>
struct xbox_live_result {

    T data;
    int code;
    void* error_code_category;
    mcpe::string message;

    xbox_live_result() {}
    xbox_live_result(xbox_live_result<T> const& t) : data(t.data), code(t.code), error_code_category(t.error_code_category), message(t.message) {}

};

template <>
struct xbox_live_result<void> {

    int code;
    void* error_code_category;
    mcpe::string message;

};

struct java_interop {

    char filler[0xc];
    void* activity;

    static std::shared_ptr<xbox::services::java_interop> get_java_interop_singleton();

};

struct xbox_live_app_config {

    uint32_t title_id();

    static std::shared_ptr<xbox::services::xbox_live_app_config> get_app_config_singleton();

};

struct local_config {

    void** vtable;

    /// @vtable xbox::services::local_config _ZN4xbox8services12local_config28get_value_from_local_storageERKSsS3_S3_
    mcpe::string get_value_from_local_storage(mcpe::string const& value, mcpe::string const& u1 = mcpe::string(), mcpe::string const& u2 = mcpe::string());

    /// @vtable xbox::services::local_config _ZN4xbox8services12local_config28write_value_to_local_storageERKSsS3_S3_S3_
    xbox::services::xbox_live_result<void> write_value_to_local_storage(mcpe::string const& name, mcpe::string const& value, mcpe::string const& u1 = mcpe::string(), mcpe::string const& u2 = mcpe::string());

    static std::shared_ptr<xbox::services::local_config> get_local_config_singleton();

};

struct xsapi_singleton {

    ~xsapi_singleton();

};

namespace system {

struct java_rps_ticket {
    mcpe::string token;
    int error_code;
    mcpe::string error_text;
};

struct auth_flow_result {
    int code;
    mcpe::string s1, xbox_user_id, gamertag, age_group, privileges, user_settings_restrictions, user_enforcement_restrictions, user_title_restrictions, cid, event_token;
};

struct token_and_signature_result {
    mcpe::string token, signature, xbox_user_id, gamertag, xbox_user_hash, age_group, privileges, user_settings_restrictions, user_enforcement_restrictions, user_title_restrictions, web_account_id, reserved;
};

enum sign_in_status { success, interaction_required, cancelled };

struct sign_in_result {
    sign_in_status result;
    bool new_account;
};

struct user_impl {

    void user_signed_out();

};

struct user_impl_android : public user_impl {

    static std::shared_ptr<xbox::services::system::user_impl_android> get_instance();

};

}

};
}

namespace pplx {

struct task_completion_event_java_rps_ticket {
    char filler[0xc]; // shared_ptr

    /// @symbol _ZNK4pplx21task_completion_eventIN4xbox8services6system15java_rps_ticketEE3setES4_
    void set(xbox::services::system::java_rps_ticket);
};
struct task_completion_event_auth_flow_result {
    char filler[0xc]; // shared_ptr

    /// @symbol _ZNK4pplx21task_completion_eventIN4xbox8services6system16auth_flow_resultEE3setES4_
    void set(xbox::services::system::auth_flow_result);
};
struct task_completion_event_xbox_live_result_void {
    char filler[0xc]; // shared_ptr

    /// @symbol _ZNK4pplx21task_completion_eventIN4xbox8services16xbox_live_resultIvEEE3setES4_
    void set(xbox::services::xbox_live_result<void>);
};

struct task_impl {
    virtual ~task_impl() = 0;
};
struct task_xbox_live_result_void {
    std::shared_ptr<task_impl> impl;

    /// @symbol _ZNK4pplx4taskIN4xbox8services16xbox_live_resultIvEEE3getEv
    xbox::services::xbox_live_result<void> get();
};
struct task_xbox_live_result_token_and_signature_result {
    std::shared_ptr<task_impl> impl;

    /// @symbol _ZNK4pplx4taskIN4xbox8services16xbox_live_resultINS2_6system26token_and_signature_resultEEEE3getEv
    xbox::services::xbox_live_result<xbox::services::system::token_and_signature_result> get();
};

struct cancellation_token {
    static cancellation_token none() {
        return cancellation_token();
    }

    void* impl = nullptr;

    cancellation_token() {}
    cancellation_token(cancellation_token const& t) {
        if (t.impl != nullptr)
            throw std::runtime_error("Only null cancellation_token are supported");
        impl = t.impl;
    }
};


}


namespace xbox {
namespace services {
namespace system {

enum class token_identity_type { };

struct auth_config {

    char filler[0x3C];
    mcpe::string* errorDetail;

    /// @symbol _ZN4xbox8services6system11auth_config22set_xtoken_compositionESt6vectorINS1_19token_identity_typeESaIS4_EE
    void set_xtoken_composition(std::vector<xbox::services::system::token_identity_type>);

    mcpe::string const& xbox_live_endpoint() const;

};

struct auth_manager {

    static std::shared_ptr<xbox::services::system::auth_manager> get_auth_manager_instance();

    void set_rps_ticket(mcpe::string const&);
    /// @symbol _ZN4xbox8services6system12auth_manager23initialize_default_nsalEN4pplx18cancellation_tokenE
    pplx::task_xbox_live_result_void initialize_default_nsal(pplx::cancellation_token t = pplx::cancellation_token::none());
    /// @symbol _ZN4xbox8services6system12auth_manager21initialize_title_nsalERKSsN4pplx18cancellation_tokenE
    pplx::task_xbox_live_result_void initialize_title_nsal(mcpe::string const&, pplx::cancellation_token t = pplx::cancellation_token::none());
    /// @symbol _ZN4xbox8services6system12auth_manager32internal_get_token_and_signatureESsRKSsS4_SsRKSt6vectorIhSaIhEEbbS4_N4pplx18cancellation_tokenE
    pplx::task_xbox_live_result_token_and_signature_result internal_get_token_and_signature(mcpe::string, mcpe::string const&, mcpe::string const&, mcpe::string, std::vector<unsigned char> const&, bool, bool, mcpe::string const&, pplx::cancellation_token t = pplx::cancellation_token::none());
    std::shared_ptr<xbox::services::system::auth_config> get_auth_config();

};

struct user_auth_android {

    static pplx::task_completion_event_java_rps_ticket* s_rpsTicketCompletionEvent;
    static pplx::task_completion_event_xbox_live_result_void* s_signOutCompleteEvent;

    static std::shared_ptr<xbox::services::system::user_auth_android> get_instance();

    /// @symbol _ZN4xbox8services6system17user_auth_android24complete_sign_in_with_uiERKNS1_16auth_flow_resultE
    void complete_sign_in_with_ui(xbox::services::system::auth_flow_result const& result);

    char filler[0x8]; // 8
    mcpe::string xbox_user_id; // c
    char filler2[0x24-0xc]; // 24
    mcpe::string cid; // 28
    char filler3[0x58-0x28];
    pplx::task_completion_event_auth_flow_result auth_flow_event;
    char filler4[0x8c-0x58-0xc];
    xbox::services::system::auth_flow_result auth_flow_result;

    // xbox::services::system::user_auth_android::auth_flow_callback


};

struct xbox_live_services_settings {

public:
    static std::shared_ptr<xbox::services::system::xbox_live_services_settings> get_singleton_instance(bool);

    /// @symbol _ZN4xbox8services6system27xbox_live_services_settings27set_diagnostics_trace_levelENS0_37xbox_services_diagnostics_trace_levelE
    void set_diagnostics_trace_level(int);



};

}
}
}