#include "api/nswgf.hpp"
#include "api/romm.hpp"
#include "data/extractor/archive_extractor.hpp"
#include "data/installer/nsp_installer.hpp"
#include "data/settings_manager.hpp"
#include "data/library_manager.hpp"
#include "data/remote/host_resolver.hpp"
#include <borealis.hpp>
#include <iostream>
#include <cassert>
#include <fstream>

using namespace switchfs;

static int tests_passed = 0;
static int tests_total = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        tests_total++; \
        if (cond) { \
            tests_passed++; \
            std::cout << "  [PASS] " << msg << std::endl; \
        } else { \
            std::cerr << "  [FAIL] " << msg << " (" << #cond << ")" << std::endl; \
        } \
    } while(0)

void testNswgfParser() {
    std::cout << "[TEST] Running testNswgfParser..." << std::endl;

    std::string sample_html = 
        R"(<div><img src="https://blogger.googleusercontent.com/img/a/sample_cover.jpg" />)"
        R"(<p>Format: NSP, Size: 4.2 GB, Languages: En, Ja, Fr</p>)"
        R"(<a href="https://gofile.io/d/abc123">Gofile Link</a>)"
        R"(<a href="https://1fichier.com/?xyz987">1fichier Link</a>)"
        R"(<a href="https://megaup.net/rarfile.rar">MegaUp Link</a></div>)";

    auto game = api::NswgfApi::parsePostContent(12345, "Super Mario Odyssey &#8211; NSP", "2026-11-07T12:00:00", sample_html);

    TEST_ASSERT(game.id == 12345, "Game ID preserved");
    TEST_ASSERT(game.title == "Super Mario Odyssey - NSP", "HTML entities decoded properly");
    TEST_ASSERT(game.date == "2026-11-07", "Date formatted properly");
    TEST_ASSERT(game.cover_url == "https://blogger.googleusercontent.com/img/a/sample_cover.jpg", "Cover URL extracted");
    TEST_ASSERT(game.format == "NSP", "Format recognized as NSP");
    TEST_ASSERT(game.size == "4.2 GB", "Size extracted properly");
    TEST_ASSERT(game.language == "En, Ja, Fr", "Languages extracted");
    TEST_ASSERT(game.download_links.size() == 3, "All 3 host download links parsed");
}

void testArchiveExtractor() {
    std::cout << "[TEST] Running testArchiveExtractor..." << std::endl;

    TEST_ASSERT(data::ArchiveExtractor::isArchive("game.rar") == true, "Identifies .rar");
    TEST_ASSERT(data::ArchiveExtractor::isArchive("update.zip") == true, "Identifies .zip");
    TEST_ASSERT(data::ArchiveExtractor::isArchive("pack.7z") == true, "Identifies .7z");
    TEST_ASSERT(data::ArchiveExtractor::isArchive("game.nsp") == false, "Rejects .nsp");
    TEST_ASSERT(data::ArchiveExtractor::isArchive("game.xci") == false, "Rejects .xci");
    TEST_ASSERT(data::ArchiveExtractor::isArchive("video.mp4") == false, "Rejects .mp4");
}

void testNspInstallerValidation() {
    std::cout << "[TEST] Running testNspInstallerValidation..." << std::endl;

    std::string temp_non_nsp = "/tmp/test_invalid.nsp";
    {
        std::ofstream out(temp_non_nsp, std::ios::binary);
        out << "NOT_A_PFS0_FILE_HEADER";
    }
    TEST_ASSERT(data::NspInstaller::isValidNsp(temp_non_nsp) == false, "Rejects non-PFS0 file");
    remove(temp_non_nsp.c_str());

    std::string temp_valid_pfs0 = "/tmp/test_valid.nsp";
    {
        std::ofstream out(temp_valid_pfs0, std::ios::binary);
        uint32_t magic = 0x30534650; // "PFS0"
        uint32_t num_files = 0;
        uint32_t string_table_size = 0;
        uint32_t reserved = 0;
        out.write(reinterpret_cast<const char*>(&magic), 4);
        out.write(reinterpret_cast<const char*>(&num_files), 4);
        out.write(reinterpret_cast<const char*>(&string_table_size), 4);
        out.write(reinterpret_cast<const char*>(&reserved), 4);
    }
    TEST_ASSERT(data::NspInstaller::isValidNsp(temp_valid_pfs0) == true, "Validates PFS0 magic correctly");
    remove(temp_valid_pfs0.c_str());
}

void testSettingsManager() {
    std::cout << "[TEST] Running testSettingsManager..." << std::endl;

    auto& sm = data::SettingsManager::getInstance();
    sm.getSettings().auto_install = true;
    sm.getSettings().default_storage = 0;
    sm.getSettings().gofile_token = "TEST_TOKEN_12345";
    sm.save();

    sm.getSettings().gofile_token = "";
    sm.load();
    TEST_ASSERT(sm.getSettings().gofile_token == "TEST_TOKEN_12345", "Settings saved and reloaded via JSON");
}

void testHostResolver() {
    std::cout << "[TEST] Running testHostResolver..." << std::endl;

    std::string digest = data::HostResolver::sha256Hex("abc");
    TEST_ASSERT(digest == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "SHA-256 hex digest matches NIST test vector");

    auto send_res = data::HostResolver::resolve("https://send.now/zp35i838lzwq", "Send.now");
    TEST_ASSERT(send_res.ok() && send_res.direct_url.find(".rar") != std::string::npos,
                "Send.now resolves directly to .rar DDL stream");
    std::cout << "    Resolved Send.now DDL: " << send_res.direct_url << " err: " << send_res.error << std::endl;
}

void testRommParser() {
    std::cout << "[TEST] Running testRommParser..." << std::endl;

    std::string sample_romm_json = R"({
        "items": [
            {
                "id": 42,
                "name": "The Legend of Zelda: Breath of the Wild",
                "fs_name": "Zelda_BOTW.nsp",
                "fs_size_bytes": 14495514624,
                "path_cover_s": "/assets/romm/resources/roms/42/cover/small.webp",
                "updated_at": "2026-11-07T10:00:00Z"
            }
        ]
    })";

    auto items = api::RommApi::parseRomsResponse(sample_romm_json, "http://192.168.1.100:8080/");
    TEST_ASSERT(items.size() == 1, "RomM paginated response parsed 1 item");
    TEST_ASSERT(items[0].id == 42, "RomM ID extracted");
    TEST_ASSERT(items[0].title == "The Legend of Zelda: Breath of the Wild", "RomM name extracted");
    TEST_ASSERT(items[0].format == "NSP", "RomM format detected from fs_name");
    TEST_ASSERT(items[0].cover_url == "http://192.168.1.100:8080/assets/romm/resources/roms/42/cover/small.webp", "RomM cover URL normalized");
    TEST_ASSERT(!items[0].download_links.empty() && items[0].download_links[0].host_name == "RomM", "RomM download link generated");
}

void testLibraryManager() {
    std::cout << "[TEST] Running testLibraryManager..." << std::endl;

    auto& lib = data::LibraryManager::getInstance();
    api::GameItem g;
    g.id = 99999;
    g.title = "Hollow Knight Silksong";
    g.format = "NSP";

    if (lib.isFavorite(g.id, g.title)) {
        lib.toggleFavorite(g);
    }
    bool added = lib.toggleFavorite(g);
    TEST_ASSERT(added && lib.isFavorite(g.id, g.title), "LibraryManager adds favorite and persists");
    bool removed = lib.toggleFavorite(g);
    TEST_ASSERT(!removed && !lib.isFavorite(g.id, g.title), "LibraryManager removes favorite");
}

int main() {
    std::cout << "=== Running Switch FS Unit Test Suite ===" << std::endl;
    testNswgfParser();
    testArchiveExtractor();
    testNspInstallerValidation();
    testSettingsManager();
    testHostResolver();
    testRommParser();
    testLibraryManager();

    std::cout << "=========================================" << std::endl;
    std::cout << "Results: " << tests_passed << "/" << tests_total << " tests passed." << std::endl;

    return (tests_passed == tests_total) ? 0 : 1;
}
