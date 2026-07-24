#include "RemoteToml.h"
#include "OSTPlatform/include/Http.h"
#include "Utils/Config/Config.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

std::string Sha256OfFile(const std::string& path) {
    return OSTPlatform::Hash::Sha256OfFile(path);
}

namespace RemoteToml {

namespace {
    constexpr const char* kGithubTemplate =
        "https://raw.githubusercontent.com/OpenSteam001/steam-monitor/"
        "{channel}/{component}/{sha256}.toml";
    constexpr const char* kJsdelivrTemplate =
        "https://cdn.jsdelivr.net/gh/OpenSteam001/steam-monitor@"
        "{channel}/{component}/{sha256}.toml";

    static bool HasPlaceholder(std::string_view text, std::string_view placeholder)
    {
        return text.find(placeholder) != std::string_view::npos;
    }

    static bool IsValidTemplate(std::string_view urlTemplate)
    {
        return HasPlaceholder(urlTemplate, "{channel}") &&
               HasPlaceholder(urlTemplate, "{component}") &&
               HasPlaceholder(urlTemplate, "{sha256}");
    }

    static void ReplaceAll(std::string& text,
                           std::string_view from,
                           std::string_view to)
    {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    static std::string ExpandTemplate(std::string urlTemplate,
                                      const Request& request,
                                      std::string_view sha256)
    {
        ReplaceAll(urlTemplate, "{channel}", request.channel);
        ReplaceAll(urlTemplate, "{component}", request.component);
        ReplaceAll(urlTemplate, "{sha256}", sha256);
        return urlTemplate;
    }

    static std::vector<std::string> BuildUrlTemplates()
    {
        const std::string remoteUrlTemplate = Config::GetRemoteUrlTemplate();
        if (remoteUrlTemplate.empty())
            return { kGithubTemplate, kJsdelivrTemplate };

        if (!IsValidTemplate(remoteUrlTemplate)) {
            return {};
        }

        return { remoteUrlTemplate };
    }
} // namespace

Result Fetch(const Request& request)
{
    namespace fs = std::filesystem;
    Result out;

    // 1. SHA-256 of the DLL.
    const auto hashStart = std::chrono::steady_clock::now();
    out.sha256 = Sha256OfFile(request.dllPath);
    const auto hashMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - hashStart).count();

    if (out.sha256.empty()) {
        return out;
    }

    // 2. Cache path & dir.
    fs::path steamRoot = fs::path(request.dllPath).parent_path();
    fs::path cacheDir  = steamRoot / "vamp" / request.channel / request.component;
    fs::path cachePath = cacheDir / (out.sha256 + ".toml");
    const std::string cachePathText = cachePath.string();

    std::error_code mkdirEc;
    fs::create_directories(cacheDir, mkdirEc);
    if (!mkdirEc) {
        // Ocultar la carpeta vamp
        SetFileAttributesW(cacheDir.wstring().c_str(), FILE_ATTRIBUTE_HIDDEN);
    }

    // 3. Try remote (mirror chain with early-out on 404).
    const std::vector<std::string> urlTemplates = BuildUrlTemplates();
    OSTPlatform::Http::Result http;
    std::string lastUrl;

    for (size_t i = 0; i < urlTemplates.size(); ++i) {
        lastUrl = ExpandTemplate(urlTemplates[i], request, out.sha256);
        http = OSTPlatform::Http::Execute(L"GET", lastUrl.c_str(),
                                          nullptr, 0, nullptr);

        if (http.ok && http.status == 200) break;

        if (http.ok && http.status == 404) {
            break;   // all mirrors serve same data
        }

        if (i + 1 < urlTemplates.size()) {
            // fallback
        }
    }

    // 4. Remote OK → write cache, return body.
    if (http.ok && http.status == 200 && !http.body.empty()) {
        std::ofstream ofs(cachePath, std::ios::binary);
        if (ofs) {
            ofs.write(http.body.data(),
                      static_cast<std::streamsize>(http.body.size()));
        }
        out.body = std::move(http.body);
        out.ok = true;
        return out;
    }

    // 5. Remote failed → fall back to whatever is cached for this exact SHA.
    if (fs::exists(cachePath)) {
        std::ifstream ifs(cachePath, std::ios::binary);
        if (ifs) {
            std::string buf((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
            if (!buf.empty()) {
                out.body = std::move(buf);
                out.ok = true;
                out.fromCache = true;
                return out;
            }
        }
    }

    return out;
}

} // namespace RemoteToml