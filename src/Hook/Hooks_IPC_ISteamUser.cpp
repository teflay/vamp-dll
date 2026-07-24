#include "Hooks_IPC.h"
#include "Hooks_IPC_ISteamUser.h"
#include "Hooks_Misc.h"
#include "Utils/Logging/Log.h"

namespace {
    using namespace IPCMessages::IClientUser;

    // [Post-Handler]: IClientUser::GetSteamID
    void HandlerPost_IClientUser_GetSteamID(CPipeClient* pipe,CUtlBuffer* pRead, CUtlBuffer* pWrite)
    {
        // No spoofing needed
    }

} // namespace

namespace Hooks_IPC_ISteamUser {
    void Register() {
        // No handlers needed
    }
}