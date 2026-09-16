#include "stdafx.h"
#include "karamoe_service.h"

namespace foo_karamoe {

    KaramoeService::KaramoeService() {
        // Make temp folder
        const std::string dirPath = core_api::get_profile_path() + fileDir;
        auto fs = filesystem::get(dirPath.c_str());
        if (fs->directory_exists(dirPath.c_str(), fb2k::noAbort)) {
            fs->remove_directory_content(dirPath.c_str(), fb2k::noAbort);
        }
        else {
            fs->make_directory(dirPath.c_str(), fb2k::noAbort);
        }
    }


    bool KaramoeService::write_to_disk(file::ptr &sourceFile, std::string &path) {
        try {
            t_filesize size = sourceFile->get_size(fb2k::noAbort);
            file::ptr targetFile;
            filesystem::g_open_write_new(targetFile, path.c_str(), fb2k::noAbort);
            targetFile->resize(size, fb2k::noAbort);
            sourceFile->g_transfer_file(sourceFile, targetFile, fb2k::noAbort);
            return true;
        }
        catch (const std::exception& e) {
            console::print("Error dumping file: ", e.what());
            return false;
        }
    }

    nlohmann::json KaramoeService::search(const std::string& query) {
        std::string filter = "filter=" + url_encode(query);
        std::string collections = "collections=" +
            url_encode(Collection::asia) +
            url_encode("," + Collection::geek) +
            url_encode("," + Collection::non_latin) +
            url_encode("," + Collection::shitpost) +
            url_encode("," + Collection::west);
        std::string url = KaramoeUrl::api + "karas/search?" + filter + "&" + collections;
        file::ptr data = http_get(url, fb2k::noAbort);
        pfc::string8 result;
        data->read_string_raw(result, fb2k::noAbort);
        nlohmann::json json_data = nlohmann::json::parse(result.toString());
        if (json_data.contains("content")) {
            std::vector<nlohmann::json> content = json_data["content"];
            return content;
        }
        console::print("Error with search results: %{public}s", result);
        return nullptr;
    }

    std::vector<Kara*> KaramoeService::parse_karas(nlohmann::json& search_results) {
        std::vector<Kara*> results;

        for (nlohmann::json song : search_results) {
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
                kara->insert({ LENGTH, std::to_string(minutes) + (seconds < 10 ? ":0" : ":") + std::to_string(seconds) });

                // Songtype
                std::string songtypes = parseNames(song, "songtypes");
                std::string misc = parseNames(song, "misc");
                std::string versions = parseNames(song, "versions");
                std::string types = "";
                types += (!types.empty() && !versions.empty() ? ", " : "") + versions;
                types += (!types.empty() && !songtypes.empty() ? ", " : "") + songtypes;
                types += (!types.empty() && !misc.empty() ? ", " : "") + misc;
                kara->insert({ TYPE, types });

                // Creators
                std::string artist = "";
                artist += parseNames(song, "singergroups");
                std::string singer = parseNames(song, "singers");
                artist += (!artist.empty() && !singer.empty() ? " / " : "") + singer;
                kara->insert({ ARTIST, artist });
                kara->insert({ WRITER, parseNames(song, "songwriters") });

                // Lang
                kara->insert({ LANG, parseNames(song, "langs") });

                // Franchise
                kara->insert({ FRANCHISE, parseNames(song, "series") });

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

                // Loudnorm
                kara->insert({ LOUDNORM, song["loudnorm"] });

                // Collections
                kara->insert({ COLLECTIONS, parseNames(song, "collections") });

                // Warnings
                kara->insert({ WARNINGS, parseNames(song, "warnings") });

                results.push_back(kara);
            }
            catch (std::exception e) {
                console::print("Error with karamoe search result: ", e);
                delete kara;
                continue;
            }
        }
        return results;
    }
 

    file::ptr KaramoeService::http_get(std::string &url, abort_callback& p_abort) {
        http_request::ptr req = http_client::get()->create_request("GET");
        file::ptr data = req->run(url.c_str(), p_abort);
        return data;
    }


    std::string KaramoeService::parseNames(nlohmann::json &data, const char* field) {
        std::string name = "";
        if (!data.contains(field)) {
            return name;
        }
        std::vector<nlohmann::json> values = data[field];
        for (nlohmann::json item : values) {
            if (!name.empty()) {
                name += ", ";
            }
            name += item["name"];
        }
        return name;
    }


    std::string KaramoeService::url_encode(const std::string& raw) {
        std::string escaped;
        for (unsigned int i = 0; i < raw.length(); i++) {
            char c = raw[i];
            if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
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


    bool should_use_hs(const Kara &kara) {
        return filesystem::g_get_extension(kara.at(MEDIAFILE).c_str()) != "mp4";
    }


    /** Returns the eventual file paths for the kara. Does NOT download anything yet */
    std::pair<std::string, std::string> KaramoeService::prepare_files(const Kara &kara) {
        std::pair<std::string, std::string> files;

        // Replace windows forbidden symbols
        std::string name = kara.at(NAME);
        const std::string forbidden = R"(<>:"/\|?*)";
        std::replace_if(name.begin(),
            name.end(),
            [&](char c) { return forbidden.find(c) != std::string::npos; },
            '_'
        );
        const std::string dirPath = core_api::get_profile_path() + fileDir;
        auto fs = filesystem::get(dirPath.c_str());

        files.first = dirPath + name + ".mp4";
        files.second = should_use_hs(kara) ? "" : dirPath + name + ".ass";

        return files;
    }


    file_info_impl KaramoeService::queue_file(const Kara &kara, std::string &file_path) {
        metadb_handle_ptr mediahandle;
        mediahandle = metadb::get()->handle_create(file_path.c_str(), 0);

        // Prepare metadata
        file_info_impl info;
        float i, tp, lra, measured_thresh, offset;
        sscanf_s(kara.at(LOUDNORM).c_str(), "%f, %f, %f, %f, %f", &i, &tp, &lra, &measured_thresh, &offset);
        float gain = REPLAY_GAIN_LUFT_TARGET - i;
        float peak = (float)std::pow(10, (tp / 20));

        info.meta_set("ARTIST", kara.at(ARTIST).c_str());
        info.meta_set("ALBUM", kara.at(FRANCHISE).c_str());
        info.meta_set("TITLE", kara.at(TITLE).c_str());

        // Loudnorm is for non hs file, might differ
        if (!should_use_hs(kara)) {
            info.info_set_replaygain_track_gain(gain);
            info.info_set_replaygain_track_peak(peak);
        }

        t_filestats stats;

        // Write tags
        metadb_hint_list_v4::ptr hints = metadb_hint_list_v4::create();
        hints->add_hint_forced(mediahandle, info, stats, false);

        // Playlist manipulation is unsafe in non-main thread
        fb2k::inMainThread([mediahandle] {
            try {
                static_api_ptr_t<playlist_manager> plm;
                plm->queue_add_item(mediahandle);
                playback_control::ptr pbc = playback_control::get();
            }
            catch (...) {
                console::print("ERROR: Failed to queue");
            }
        });

        return info;  // Return info so the same data can be ACTUALLY be written on the file, currently only in queued handle
    }

    void KaramoeService::write_tags(file_info_impl &info, std::string &filepath) {
        service_ptr_t<input_info_writer> writer;
        file::ptr tagFile;
        input_entry::g_open_for_info_write(writer, tagFile, filepath.c_str(), fb2k::noAbort);
        writer->set_info(0, info, fb2k::noAbort);
        writer->commit(fb2k::noAbort);
    }

    std::pair<file::ptr, file::ptr> KaramoeService::download_files(const Kara &kara) {
        bool use_hs = should_use_hs(kara);
        std::string mediaUrl = !use_hs ? KaramoeUrl::media_dl + kara.at(MEDIAFILE) : KaramoeUrl::hardsub_dl + kara.at(HS_MEDIAFILE);
        std::string lyricUrl = KaramoeUrl::lyric_dl + kara.at(SUBFILE);

        file::ptr media = http_get(mediaUrl, fb2k::noAbort);
        file::ptr lyrics = nullptr;
        if (!use_hs) {
            lyrics = http_get(lyricUrl, fb2k::noAbort);
        }
        return std::make_pair(media, lyrics);
    }
};