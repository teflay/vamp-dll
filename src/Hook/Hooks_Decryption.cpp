#include "Hooks_Decryption.h"
#include "HookMacros.h"
#include "dllmain.h"
#include <string>

namespace {

    void* g_pConfigStoreLocal = nullptr;

    HOOK_FUNC(ConfigStoreGetBinary, int32, void* pObject, EConfigStore eConfigStore, const char* KeyName, char* Key, uint32 KeySize) {
        if (eConfigStore == k_EConfigStoreUserLocal && pObject && !g_pConfigStoreLocal) {
            g_pConfigStoreLocal = pObject;
        }
        std::string name(KeyName);
        // Expected shape: ".../<DepotId>\DecryptionKey"
        if (size_t last = name.find("\\DecryptionKey"); last != std::string::npos) {
            if (size_t start = name.find_last_of("\\", last - 1); start != std::string::npos) {
                AppId_t depotId = std::stoul(name.substr(start + 1, last - start - 1));
                if (const auto& key = LuaConfig::GetDecryptionKey(depotId); !key.empty()) {
                    if (KeySize >= key.size()) {
                        memcpy(Key, key.data(), key.size());
                        return static_cast<int32>(key.size());
                    }
                }
            }
        }
        return oConfigStoreGetBinary(pObject, eConfigStore, KeyName, Key, KeySize);
    }

    std::vector<uint8_t> ReadConfigStoreLocalBinary(const std::string& keyName) {
        if (!g_pConfigStoreLocal || !oConfigStoreGetBinary) {
            return {};
        }

        std::vector<uint8_t> value(1024);
        int32 result = oConfigStoreGetBinary(g_pConfigStoreLocal, k_EConfigStoreUserLocal,
                                             keyName.c_str(),
                                             reinterpret_cast<char*>(value.data()),
                                             static_cast<uint32>(value.size()));
        if (result <= 0) {
            return {};
        }

        value.resize(result);
        return value;
    }
}

namespace Hooks_Decryption {
    void Install() {
        HOOK_BEGIN();
        INSTALL_HOOK_C(ConfigStoreGetBinary);
        HOOK_END();
    }

    void Uninstall() {
        UNHOOK_BEGIN();
        UNINSTALL_HOOK_C(ConfigStoreGetBinary);
        UNHOOK_END();
    }

    std::vector<uint8_t> GetCacheAppOwnershipTicket(AppId_t appId) {
        std::vector<uint8_t> ticket = ReadConfigStoreLocalBinary(std::format("apptickets\\{}", appId));
        return ticket;
    }
}