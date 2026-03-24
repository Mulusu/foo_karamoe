#include "stdafx.h"
#include "json.hpp"
#include "foo_karamoe.h"
#include <iomanip>
#include <sstream>
#include <foobar2000/SDK/foobar2000-pfc.h>
#include <Windows.h>

/* Declare plugin info to foobar */
DECLARE_COMPONENT_VERSION(
	"Karamoe Integration",
	"0.0.3",
	"Karaoke Mugen ( https://kara.moe ) Integration Plugin"
);
VALIDATE_COMPONENT_FILENAME("foo_karamoe.dll");


namespace foo_karamoe {
    std::string parseNames(std::vector<nlohmann::json> data) {
        std::string name;
        for (nlohmann::json item : data) {
            if (!name.empty()) {
                name += ", ";
            }
            name += item["name"];
        }
        return name;
    }

    std::vector<Kara*> search(const pfc::string8& query) {
        std::string url = KaramoeUrl::api + "karas/search?filter=" + url_encode(query);
        console::print(url.c_str());
        file::ptr data = http_get(url, fb2k::noAbort);
        pfc::string8 result;
        data->read_string_raw(result, fb2k::noAbort);
        nlohmann::json json_data = nlohmann::json::parse(result.toString());
        std::vector<Kara*> results;

        if (json_data.contains("content")) {
            std::vector<nlohmann::json> content = json_data["content"];
            for (nlohmann::json song : content) {
                Kara* kara = new Kara;
                try {
                    // Name
                    kara->insert({ NAME , song["songname"] });

                    // Title
                    std::string def_lang = song["titles_default_language"];
                    kara->insert({ TITLE, song["titles"][def_lang] });

                    // Duration
                    int duration = song["duration"];
                    int minutes = duration / 60;
                    int seconds = duration % 60;
                    kara->insert({ LENGTH, std::to_string(minutes) + (seconds < 10 ? ":0" : ":") + std::to_string(seconds)});

                    // Songtype
                    std::string songtypes = parseNames(song["songtypes"]);
                    std::string misc = parseNames(song["misc"]);
                    std::string versions = parseNames(song["versions"]);
                    std::string types = songtypes;
                    types += (!types.empty() && !misc.empty() ? ", " : "") + misc;
                    types += (!types.empty() && !versions.empty() ? ", " : "") + versions;
                    kara->insert({ TYPE, types });

                    // Creators
                    kara->insert({ SINGER, parseNames(song["singers"]) });
                    kara->insert({ WRITER, parseNames(song["songwriters"]) });

                    // Lang
                    kara->insert({ LANG, parseNames(song["langs"]) });

                    // Franchise
                    kara->insert({ FRANCHISE, parseNames(song["series"]) });

                    // Mediafiles
                    kara->insert({ MEDIAFILE, song["mediafile"] });
                    kara->insert({ HS_MEDIAFILE, song["hardsubbed_mediafile"] });
                    if (song.contains("lyrics_infos")) {
                        for (nlohmann::json option : song["lyrics_infos"]) {
                            if (option["default"]) {
                                kara->insert({ SUBFILE, option["filename"] });
                                break;
                            }
                        }
                    }
                    else if (song.contains("subfile")) {
                        kara->insert({ SUBFILE, song["subfile"] });
                    }
                    else {
                        console::print("ERROR: No lyric files found. Ignoring song.");
                        delete kara;
                        continue;
                    }
                    results.push_back(kara);
                }
                catch (std::exception e) {
                    console::print("Error with karamoe search result: ", e.what());
                    delete kara;
                    continue;
                }
            }
        }
        return results;
    }

    file::ptr http_get(std::string& url, abort_callback& p_abort) {
        http_request::ptr req = http_client::get()->create_request("GET");
        file::ptr data = req->run(url.c_str(), p_abort);
        return data;
    }

    std::string url_encode(const pfc::string8& raw) {
        std::string escaped;
        for (unsigned int i = 0; i < raw.get_length(); i++) {
            char c = raw[i];
            if (isalnum((unsigned char)c) ||
                c == '-' ||
                c == '_' ||
                c == '.' ||
                c == '~'
                ) {
                escaped += c;
            }
            else if (c == ' ') {
                escaped += "%20";
            }
            else {
                escaped += '%';
                char hex[3];
                sprintf_s(hex, sizeof(hex), "%02X", (unsigned char)c);
                escaped += hex;
            }
        }
        return escaped;
    }


    void make_temp_folder() {
        const std::string dirPath = core_api::get_profile_path() + fileDir;
        auto fs = filesystem::get(dirPath.c_str());
        if (fs->directory_exists(dirPath.c_str(), fb2k::noAbort)) {
            fs->remove_directory_content(dirPath.c_str(), fb2k::noAbort);
        }
        else {
            fs->make_directory(dirPath.c_str(), fb2k::noAbort);
        }
    }

    bool write_to_disk(file::ptr sourceFile, std::string path) {
        try {
            size_t size = sourceFile->get_size(fb2k::noAbort);
            file::ptr targetFile;
            filesystem::g_open_write_new(targetFile, path.c_str(), fb2k::noAbort);
            targetFile->resize(size, fb2k::noAbort);
            sourceFile->g_transfer_file(sourceFile, targetFile, fb2k::noAbort);
            return true;
        }
        catch (const std::exception& e) {
            console::print("Error dumping file: ", e.what());
        }
        return false;
    }

    // Returns the file path adjusted for the name of the kara, and if the file already exists
    std::pair<std::string, bool> make_filepath(std::string name, std::string file) {
        try {
            const std::string dirPath = core_api::get_profile_path() + fileDir;
            auto fs = filesystem::get(dirPath.c_str());
            std::string filepath = dirPath + name + "." + fs->get_extension(file.c_str()).toString();
            bool exists = fs->file_exists(filepath.c_str(), fb2k::noAbort);
            return std::make_pair(filepath, exists);
        }
        catch (std::exception& e) {
            console::print("Error getting filepaths: ", e.what());
            return std::make_pair("", false);
        }
    }

    void QueueSong(Kara* karaPtr) {
        Kara kara = *karaPtr;
        std::pair<std::string, bool> mediaFile;
        std::pair<std::string, bool> subFile;
        metadb_handle_ptr mediahandle;

        bool use_hs = filesystem::g_get_extension(kara[MEDIAFILE].c_str()) != "mp4";
        bool use_url = false;
/*
        bool use_mem = false;

        if (use_mem) {
            std::string path = "MEMFILE://" + kara[HS_MEDIAFILE];
            mediahandle = metadb::get()->handle_create(path.c_str(), 0);

            fb2k::inMainThread([mediahandle] {
                try {
                    static_api_ptr_t<playlist_manager> plm;
                    plm->queue_add_item(mediahandle);
                    playback_control::ptr pbc = playback_control::get();
                    if (plm->queue_get_count() == 1 && !pbc->is_playing() && !pbc->is_paused()) {
                        pbc->start();  // Only item in queue, not playing, not paused... just play it
                    }
                }
                catch (...) {
                    console::print("ERROR: Failed to queue");
                }
            });
            return;
        }
    */

        if (!use_url) {
            if (!use_hs) {
                mediaFile = make_filepath(kara[NAME], kara[MEDIAFILE]);
                subFile = make_filepath(kara[NAME], kara[SUBFILE]);
            }
            else {
                mediaFile = make_filepath(kara[NAME], kara[HS_MEDIAFILE]);
            }
        
            // If the file doesn't already exist, download it from karamoe and dump to disk
            if (!mediaFile.second) {
                std::string mediaUrl = !use_hs ? KaramoeUrl::media_dl + kara[MEDIAFILE] : KaramoeUrl::hardsub_dl + kara[HS_MEDIAFILE];
                file::ptr media = http_get(mediaUrl, fb2k::noAbort);
                bool success = write_to_disk(media, mediaFile.first);
            }
            if (!use_hs && !subFile.second) {
                std::string lyricUrl = KaramoeUrl::lyric_dl + kara[SUBFILE];
                file::ptr lyrics = http_get(lyricUrl, fb2k::noAbort);
                bool success = write_to_disk(lyrics, subFile.first);
            }
            mediahandle = metadb::get()->handle_create(mediaFile.first.c_str(), 0);
        }
        else {
            mediahandle = metadb::get()->handle_create((KaramoeUrl::hardsub_dl + kara[HS_MEDIAFILE]).c_str(), 0);
        }

        // Playlist manipulation is unsafe in non-main thread
        fb2k::inMainThread([mediahandle] {
            try {
                static_api_ptr_t<playlist_manager> plm;
                plm->queue_add_item(mediahandle);
                playback_control::ptr pbc = playback_control::get();
                if (plm->queue_get_count() == 1 && !pbc->is_playing() && !pbc->is_paused()) {
                    pbc->start();  // Only item in queue, not playing, not paused... just play it
                }
            }
            catch (...) {
                console::print("ERROR: Failed to queue");
            }
        });
    }
}
