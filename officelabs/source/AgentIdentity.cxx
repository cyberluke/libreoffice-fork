/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <officelabs/AgentIdentity.hxx>

#include <comphelper/hash.hxx>
#include <rtl/strbuf.hxx>
#include <osl/file.hxx>
#include <osl/process.h>
#include <osl/security.hxx>
#include <sal/log.hxx>

namespace officelabs {

namespace {

// secrets.token_urlsafe(32) is always exactly 43 base64url characters;
// install.secret is 32 bytes hex-encoded, so exactly 64.
const sal_Int32 SESSION_TOKEN_LENGTH = 43;
const sal_Int32 INSTALL_SECRET_HEX_LENGTH = 64;
const sal_uInt64 MAX_IDENTITY_BYTES = 256;

const sal_Int32 SHA256_BLOCK_BYTES = 64;

bool isBase64UrlChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
           || c == '-' || c == '_';
}

bool isHexChar(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// Read one identity file, rejecting anything that is not exactly what we expect.
///
/// Length is checked before the alphabet because the alphabet alone cannot
/// catch a truncated file: the first N characters of a real token are still
/// valid. A partial write mid-rotation must read as "no secret" — which the
/// caller retries — rather than as a bad one, which merely 401s later.
/// The directory holding session.token and install.secret, as a file URL.
///
/// NOT osl::Security::getHomeDir(). On Windows that is implemented as
/// GetSpecialFolder(FOLDERID_Documents) (sal/osl/w32/security.cxx), so with
/// OneDrive Known Folder Move it resolves to
/// C:/Users/<u>/OneDrive/Documents -- while the agent, which WRITES these
/// files, uses Python's Path.home(), i.e. the profile. The two runtimes
/// therefore looked in different directories and the sidebar could not
/// authenticate at all.
///
/// Windows now uses %LOCALAPPDATA%/OfficeLabs: per-user, per-machine, and not
/// roamed or cloud-synced. That last property is the point rather than a
/// nicety -- install.secret is persistent and authorises code execution, so
/// syncing it to Documents would copy a code-execution credential to the
/// cloud and to every other signed-in device, and would break its
/// "per-install" meaning. The agent's default_identity_dir() reads the same
/// LOCALAPPDATA variable, so the two agree by construction.
///
/// POSIX keeps ~/.officelabs, where $HOME already means the profile.
OUString identityDirUrl()
{
#ifdef _WIN32
    OUString sLocalAppData;
    if (osl_getEnvironment(u"LOCALAPPDATA"_ustr.pData, &sLocalAppData.pData)
            == osl_Process_E_None
        && !sLocalAppData.isEmpty())
    {
        OUString sUrl;
        if (osl::FileBase::getFileURLFromSystemPath(sLocalAppData, sUrl)
            == osl::FileBase::E_None)
        {
            return sUrl + "/OfficeLabs";
        }
    }
    SAL_WARN("officelabs.cef", "LOCALAPPDATA unusable; falling back to home dir");
#endif
    OUString sHomeUrl;
    osl::Security aSecurity;
    if (!aSecurity.getHomeDir(sHomeUrl) || sHomeUrl.isEmpty())
        return OUString();
    return sHomeUrl + "/.officelabs";
}

OString readIdentityFile(std::u16string_view aName, sal_Int32 nExpectedLength,
                         bool (*pIsValidChar)(char))
{
    const OUString sDir = identityDirUrl();
    if (sDir.isEmpty())
    {
        SAL_WARN("officelabs.cef", "could not resolve identity directory");
        return OString();
    }

    const OUString sUrl = sDir + "/" + aName;
    osl::File aFile(sUrl);
    if (aFile.open(osl_File_OpenFlag_Read) != osl::FileBase::E_None)
    {
        SAL_WARN("officelabs.cef", "cannot open identity file: " << sUrl);
        return OString();
    }

    char aBuf[MAX_IDENTITY_BYTES + 1];
    sal_uInt64 nRead = 0;
    const osl::FileBase::RC eRead = aFile.read(aBuf, sizeof(aBuf), nRead);
    aFile.close();

    if (eRead != osl::FileBase::E_None || nRead == 0 || nRead > MAX_IDENTITY_BYTES)
    {
        SAL_WARN("officelabs.cef", "cannot read identity file: " << sUrl);
        return OString();
    }

    const OString sRaw = OString(aBuf, static_cast<sal_Int32>(nRead)).trim();
    if (sRaw.getLength() != nExpectedLength)
    {
        SAL_WARN("officelabs.cef", "identity file is not a whole secret: " << sUrl);
        return OString();
    }

    for (sal_Int32 i = 0; i < sRaw.getLength(); ++i)
    {
        if (!pIsValidChar(sRaw[i]))
        {
            SAL_WARN("officelabs.cef", "identity file has invalid content: " << sUrl);
            return OString();
        }
    }

    return sRaw;
}

std::vector<unsigned char> sha256(const unsigned char* pData, size_t nLength)
{
    return comphelper::Hash::calculateHash(pData, nLength, comphelper::HashType::SHA256);
}

} // anonymous namespace

OString readSessionToken()
{
    return readIdentityFile(u"session.token", SESSION_TOKEN_LENGTH, isBase64UrlChar);
}

std::vector<unsigned char> readInstallSecret()
{
    const OString sHex = readIdentityFile(u"install.secret", INSTALL_SECRET_HEX_LENGTH, isHexChar);
    if (sHex.isEmpty())
        return {};

    std::vector<unsigned char> aBytes;
    aBytes.reserve(sHex.getLength() / 2);
    for (sal_Int32 i = 0; i < sHex.getLength(); i += 2)
    {
        const auto nibble = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
            return static_cast<unsigned char>(c - 'A' + 10);
        };
        aBytes.push_back(static_cast<unsigned char>((nibble(sHex[i]) << 4) | nibble(sHex[i + 1])));
    }
    return aBytes;
}

OString hmacSha256Hex(const std::vector<unsigned char>& rKey, std::string_view aMessage)
{
    if (rKey.empty())
        return OString();

    // RFC 2104. LibreOffice ships SHA-256 but no HMAC over it, so this is the
    // standard construction rather than anything bespoke: keys longer than the
    // block are hashed first, shorter ones are zero-padded.
    std::vector<unsigned char> aKey = rKey;
    if (static_cast<sal_Int32>(aKey.size()) > SHA256_BLOCK_BYTES)
        aKey = sha256(aKey.data(), aKey.size());
    aKey.resize(SHA256_BLOCK_BYTES, 0);

    std::vector<unsigned char> aInner;
    aInner.reserve(SHA256_BLOCK_BYTES + aMessage.size());
    for (unsigned char c : aKey)
        aInner.push_back(c ^ 0x36);
    aInner.insert(aInner.end(), aMessage.begin(), aMessage.end());
    const std::vector<unsigned char> aInnerHash = sha256(aInner.data(), aInner.size());

    std::vector<unsigned char> aOuter;
    aOuter.reserve(SHA256_BLOCK_BYTES + aInnerHash.size());
    for (unsigned char c : aKey)
        aOuter.push_back(c ^ 0x5c);
    aOuter.insert(aOuter.end(), aInnerHash.begin(), aInnerHash.end());
    const std::vector<unsigned char> aMac = sha256(aOuter.data(), aOuter.size());

    OStringBuffer aHex(aMac.size() * 2);
    for (unsigned char c : aMac)
    {
        static const char* pDigits = "0123456789abcdef";
        aHex.append(pDigits[(c >> 4) & 0x0f]);
        aHex.append(pDigits[c & 0x0f]);
    }
    return aHex.makeStringAndClear();
}

} // namespace officelabs

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
