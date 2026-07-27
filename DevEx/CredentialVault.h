#pragma once

#include "Types.h"
#include <string>
#include <optional>

class CredentialVault
{
public:
    std::optional<UserSession> Authenticate(const std::string& personalAccessToken) const;
};
