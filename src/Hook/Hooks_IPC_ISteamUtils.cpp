#include "Hooks_IPC.h"
#include "Hooks_IPC_ISteamUtils.h"
#include "Hooks_Misc.h"
#include "Steam/Callback.h"

namespace {
    using namespace IPCMessages::IClientUtils;

    // [Post-Handler]: IClientUtils::GetAppID
    //  SpawnProcess rewrites pGameID to 480 for OnlineFix games,
    //  so steamclient returns 480.  Restore the real app_id.
    //  GetAppID reads and updates the response steamclient pre-filled.
    void HandlerPost_IClientUtils_GetAppID(CPipeClient* pipe, CUtlBuffer* pRead, CUtlBuffer* pWrite)
    {
        AppId_t realAppId = Hooks_Misc::ResolveAppId();
        if (!realAppId) return;

        GetAppIDResp resp{pWrite};
        if (!resp.ok()) return;

        // Read what steamclient just wrote, decide whether to spoof.
        const AppId_t current = resp.returnValue();
        if (current == realAppId) return;
        resp.set_returnValue(realAppId);
    }

} // namespace

namespace Hooks_IPC_ISteamUtils {
    void Register() {
        IPCHandlerEntry UtilsEntries[] = {
            ADD_IPC_POST_HANDLER(IClientUtils, GetAppID),
        };
        Hooks_IPC::RegisterHandlers(UtilsEntries);
    }
}