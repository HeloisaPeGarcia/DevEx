#include "CredentialVault.h"
#include "TextUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "Advapi32.lib")
#endif

std::optional<UserSession> CredentialVault::Authenticate(const std::string& personalAccessToken) const
{
    const std::string token = TextUtil::Trim(personalAccessToken);
    if (token.size() < 12)
    {
        return std::nullopt;
    }

    // Accept any token with at least 12 characters for prototype purposes

    UserSession session;
    session.username = "octocat"; // Standard GitHub user representation
    session.tokenPreview = token.substr(0, 4) + "..." + token.substr(token.size() - 4);
    session.token = token;

    if (token.find("admin") != std::string::npos)
    {
        session.role = UserRole::Admin;
    }
    else if (token.find("maintainer") != std::string::npos)
    {
        session.role = UserRole::Maintainer;
    }

    return session;
}

bool CredentialVault::SaveToken(const std::string& token) const
{
#ifdef _WIN32
    std::string trimmed = TextUtil::Trim(token);
    CREDENTIALW cred = { 0 };
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<wchar_t*>(L"OrbitDesktop/GitHubToken");
    cred.CredentialBlobSize = static_cast<DWORD>(trimmed.size() * sizeof(char));
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(trimmed.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return ::CredWriteW(&cred, 0) == TRUE;
#else
    return false;
#endif
}

std::string CredentialVault::GetStoredToken() const
{
#ifdef _WIN32
    PCREDENTIALW cred = nullptr;
    if (::CredReadW(L"OrbitDesktop/GitHubToken", CRED_TYPE_GENERIC, 0, &cred) == TRUE)
    {
        std::string token(reinterpret_cast<char*>(cred->CredentialBlob), cred->CredentialBlobSize);
        ::CredFree(cred);
        return token;
    }
#endif
    return "";
}

bool CredentialVault::DeleteToken() const
{
#ifdef _WIN32
    return ::CredDeleteW(L"OrbitDesktop/GitHubToken", CRED_TYPE_GENERIC, 0) == TRUE;
#else
    return false;
#endif
}
