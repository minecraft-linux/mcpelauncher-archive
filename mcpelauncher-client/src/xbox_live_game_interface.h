#pragma once

#include <memory>
#include <string>
#if 0
#include <minecraft/Xbox.h>
#endif

class XboxLiveGameInterface {

public:
    static XboxLiveGameInterface& getInstance();
    virtual std::string getCllXToken(bool refresh) = 0;

    virtual std::string getCllXTicket(std::string const& xuid) = 0;


};

class XboxLiveDefaultGameInterface : public XboxLiveGameInterface {

protected:
    static const char* const TAG;



public:
    std::string getCllXToken(bool refresh) override;

    std::string getCllXTicket(std::string const &xuid) override;

};