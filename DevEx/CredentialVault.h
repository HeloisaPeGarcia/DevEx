#pragma once

#include "Types.h"
#include <string>

class CredentialVault
{
public:
    UserSession Authenticate(const std::string& personalAccessToken) const;
};
