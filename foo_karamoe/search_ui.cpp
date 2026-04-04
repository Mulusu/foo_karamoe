#include "stdafx.h"
#include <iostream>
#include "json.hpp"
#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>
#include <libPPUI/win32_op.h>
#include <foobar2000/helpers/CListControlFb2kColors.h>
#include <libPPUI/CEditWithButtons.h>
#include <libPPUI/CListControlSimple.h>
#include "foo_karamoe.h"
#include <iomanip>
#include <sstream>
#include <Windows.h>
#include <foobar2000/SDK/foobar2000-pfc.h>

namespace foo_karamoe {

#define TIMER_DEBOUNCE 1001  // Id for our timer
    class SearchUI : public ui_element_instance, public CWindowImpl<SearchUI> {
    private:
        ui_element_instance_callback::ptr m_callback;
        ui_element_config::ptr m_config;
        SearchStatus m_searchStatus;
        Colors m_colors;
        CEditWithButtons m_edit;
        CListControlFb2kColors<CListControlSimple> m_list;
        WTL::CStatic m_statusIcon;

    public:
        static void g_get_name(pfc::string_base& out) { out = "Karamoe UI Element"; }
        static const char* g_get_description() { return "UI Element for Karaoke Mugen plugin."; }
        static GUID g_get_guid() { return { 0x1230c9a2, 0xd37e, 0x4103, { 0xaa, 0x10, 0xba, 0xf7, 0x1f, 0x20, 0xe5, 0x28 } }; }
        static GUID g_get_subclass() { return ui_element_subclass_utility; }
        HWND get_wnd() { return m_hWnd; }

        static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
        void set_configuration(ui_element_config::ptr config) { m_config = config; }
        ui_element_config::ptr get_configuration() { return m_config; }

        BEGIN_MSG_MAP(SearchUI)
            MESSAGE_HANDLER(WM_CREATE, OnCreate)
            MESSAGE_HANDLER(WM_SIZE, OnSize)
            MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
            MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnColor)
            MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnColor)
            MESSAGE_HANDLER(WM_TIMER, OnTimer)
            COMMAND_CODE_HANDLER(EN_CHANGE, OnSearchChange)
            MESSAGE_HANDLER(WM_CONTEXTMENU, OnRightClick)
        END_MSG_MAP()

        SearchUI(ui_element_config::ptr config, ui_element_instance_callback::ptr callback) {
            m_searchStatus = Idle;
            m_callback = callback;
            if (get_configuration() == NULL) {
                // Set empty config, so position in layout can be saved by foobar
                set_configuration(g_get_default_configuration());
            }
            make_temp_folder();
        }

        void initialize_window(HWND parent) {
            WIN32_OP(Create(parent) != NULL);
        }

        /* Create necesssary UI elements */
        LRESULT OnCreate(UINT uint, WPARAM wparam, LPARAM lparam, BOOL& handled) {
            // Create the UI elements. Sizes left to nullptr, they are set in OnSize

            m_edit.Create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL);
            m_edit.SendMessage(EM_SETCUEBANNER, FALSE, (LPARAM)L"Search...");
            m_edit.ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

            // Request status icon
            m_statusIcon.Create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE);
            m_statusIcon.ModifyStyleEx(WS_EX_CLIENTEDGE, 0);
            LOGFONT lf = {};
            lf.lfHeight = SEARCH_BAR_HEIGHT;
            wcscpy_s(lf.lfFaceName, L"Segoe UI Emoji");
            HFONT hFont = CreateFontIndirect(&lf);
            m_statusIcon.SetFont(hFont);
            SetSearchStatus(Idle);

            // Result list
            m_list.Create(m_hWnd);
            m_list.ModifyStyleEx(LVS_EX_HEADERDRAGDROP, 0);
            for (ResultCol row : rows) {
                m_list.AddColumn(row.m_label.c_str(), row.m_width);
            }
            ApplyFoobarColors();
            return 0;
        }

        LRESULT OnSize(UINT, WPARAM, LPARAM lParam, BOOL&) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            m_edit.SetWindowPos(nullptr,
                SEARCH_STATUS_WIDTH, 0, width, SEARCH_BAR_HEIGHT,
                0
            );
            m_statusIcon.SetWindowPos(nullptr,
                0 , 0, SEARCH_STATUS_WIDTH, SEARCH_BAR_HEIGHT,
                0
            );
            m_list.SetWindowPos(nullptr,
                0, SEARCH_BAR_HEIGHT, width, height - SEARCH_BAR_HEIGHT,
                0
            );
            return 0;
        }

        /* Override windows default colors of the editable text field with foobar colors */
        LRESULT OnColor(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
            if ((HWND)lParam == m_edit.m_hWnd) {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, m_colors.Bg);
                SetTextColor(hdc, m_colors.Text);
                return (LRESULT)m_colors.Brush.m_hBrush;
            }
            if ((HWND)lParam == m_statusIcon.m_hWnd) {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, m_colors.Bg);
                SetTextColor(hdc, m_colors.SelBg);
                return (LRESULT)m_colors.Brush.m_hBrush;
            }
            return 0;
        }

        void ClearResultList() {
            for (unsigned int i = 0; i < m_list.GetItemCount(); i++) {
                free((Kara*)m_list.GetItemUserData(i));  // Destroy Kara stored there
            }
            m_list.RemoveAllItems();
        }

        LRESULT OnSearchChange(WORD, WORD, HWND, BOOL&) {
            KillTimer(TIMER_DEBOUNCE);  // Stop existing timer
            pfc::string8 query;
            uGetWindowText(m_edit, query);
            if (query.get_length() == 0) {
                SetSearchStatus(Idle);
                ClearResultList();
                return 0;
            }
            SetSearchStatus(Waiting);
            SetTimer(TIMER_DEBOUNCE, DEBOUNCE_WAIT, nullptr);
            return 0;
        }

        LRESULT OnDestroy(UINT, WPARAM, LPARAM lParam, BOOL&) {
            KillTimer(TIMER_DEBOUNCE);
            return 0;
        }

        void ApplyFoobarColors() {
            m_colors.Bg = m_callback->query_std_color(ui_color_background);
            m_colors.Text = m_callback->query_std_color(ui_color_text);
            m_colors.SelBg = m_callback->query_std_color(ui_color_selection);
            m_colors.SelText = m_callback->query_std_color(ui_color_highlight);

            if (m_colors.Brush) { m_colors.Brush.DeleteObject(); }
            m_colors.Brush.CreateSolidBrush(m_colors.Bg);

            // Set fonts to match foobar themes
            t_ui_font fontDefault = m_callback->query_font_ex(ui_font_default);
            t_ui_font fontLists = m_callback->query_font_ex(ui_font_lists);
            if (m_edit.m_hWnd && fontDefault) {
                LOGFONT lf = {};
                GetObject(fontDefault, sizeof(lf), &lf);
                lf.lfHeight = lf.lfHeight * 1.2;
                t_ui_font hNewFont = CreateFontIndirect(&lf);
                m_edit.SendMessage(WM_SETFONT, (WPARAM)hNewFont, TRUE);
            }
            if (m_list.m_hWnd && fontLists) {
                m_list.SendMessage(WM_SETFONT, (WPARAM)fontLists, TRUE);
            }
            Invalidate();
        }

        void SetSearchStatus(SearchStatus status) {
            m_searchStatus = status;

            // Update UI from main thread
            fb2k::inMainThread([this] {
                if (m_searchStatus > 0xFFFF) {
                    wchar_t offset = m_searchStatus - 0x10000;
                    wchar_t high = ((offset >> 10) + 0xD800);
                    wchar_t low = ((offset & 0x3FF) + 0xDC00);
                    wchar_t emoji[] = { high, low, 0 };
                    m_statusIcon.SetWindowTextW(emoji);
                }
                else {
                    wchar_t emoji[] = { (wchar_t)m_searchStatus, 0 };
                    m_statusIcon.SetWindowTextW(emoji);
                }
            });
        }

        LRESULT OnRightClick(UINT, WPARAM wparam, LPARAM lParam, BOOL&) {
            if ((HWND)wparam != m_list.m_hWnd) {
                // Not on a list, don't care
                return 0;
            }
            CPoint pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (pt.x == -1 && pt.y == -1) {
                console::print("Keyboard context menu");
                size_t sel = m_list.GetFirstSelected();
                if (sel >= 0)
                {
                    CRect rc = m_list.GetItemRect(sel);
                    pt = rc.CenterPoint();
                    m_list.ClientToScreen(&pt);  // Convert to screen coordinates
                }
            }

            CMenu menu;
            menu.CreatePopupMenu();
            menu.AppendMenuW(MF_STRING, 1, _T("Add to playback queue"));

            int cmd = menu.TrackPopupMenu(
                TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                pt.x, pt.y, m_hWnd
            );

            switch (cmd) {
            case 1:
                size_t selectedItem = m_list.GetFirstSelected();
                Kara* selected = (Kara*)m_list.GetItemUserData(selectedItem);
                SetSearchStatus(Save);
                // Queueing involves downloading the files --> do in worker thread
                fb2k::inWorkerThread([this, selected] {
                    QueueSong(selected);
                    SetSearchStatus(Done);
                });
                break;
                // TODO: add more options?
            }
            return 0;
        }


        static void ErrorPop(std::string title, std::string msg) {
            popup_message_v3::query_t q;
            q.title = title.c_str();
            q.msg = msg.c_str();
            q.buttons = popup_message_v3::buttonOK;
            q.icon = popup_message_v3::iconError;
            q.show();
        }

        LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
            KillTimer(TIMER_DEBOUNCE);
            SetSearchStatus(InProgress);
            std::string query = uGetWindowText(m_edit).toString();
            ClearResultList();
            // Might take a moment to work, queue to worker thread to not freeze UI
            fb2k::inWorkerThread([this, query] {
                std::vector<Kara*> results;
                try {
                    results = search(query);
                }
                catch (std::exception e) {
                    console::print("Error getting karamoe search results: ", e.what());
                    SetSearchStatus(Error);
                }
                for (Kara* kara : results) {
                    AddResultRow(kara);
                }
                SetSearchStatus(Done);
                });
            return 0;
        }

        void AddResultRow(Kara* kara) {
            // Altering UI, better do it in main
            fb2k::inMainThread([this, kara] {
                size_t rowIndex = m_list.InsertItem(m_list.GetItemCount());
                for (unsigned int i = 0; i < rows.size(); i++) {
                    ResultCol row = rows[i];
                    std::string value = (*kara)[row.m_field];
                    m_list.SetItemText(rowIndex, i, value.c_str());
                }
                m_list.SetItemUserData(rowIndex, (size_t)kara);
                });
        }

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

        file::ptr http_get(std::string url, abort_callback& p_abort) {
            http_request::ptr req = http_client::get()->create_request("GET");
            file::ptr data = req->run(url.c_str(), p_abort);
            return data;
        }

        std::vector<Kara*> search(const std::string& query) {
            std::string filter = "filter=" + url_encode(query);
            std::string collections = "collections=" + 
                url_encode(Collection::asia) +
                url_encode("," + Collection::geek) + 
                url_encode("," + Collection::non_latin) + 
                url_encode("," + Collection::shitpost) +
                url_encode("," + Collection::west);
            std::string url = KaramoeUrl::api + "karas/search?" + filter + "&" + collections;
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
                        kara->insert({ LENGTH, std::to_string(minutes) + (seconds < 10 ? ":0" : ":") + std::to_string(seconds) });

                        // Songtype
                        std::string songtypes = parseNames(song["songtypes"]);
                        std::string misc = parseNames(song["misc"]);
                        std::string versions = parseNames(song["versions"]);
                        std::string types = "";
                        types += (!types.empty() && !versions.empty() ? ", " : "") + versions;
                        types += (!types.empty() && !songtypes.empty() ? ", " : "") + songtypes;
                        types += (!types.empty() && !misc.empty() ? ", " : "") + misc;
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

                        // Loudnorm
                        kara->insert({ LOUDNORM, song["loudnorm"] });

                        // Collections
                        kara->insert({ COLLECTIONS, parseNames(song["collections"]) });

                        // Warnings
                        kara->insert({ WARNINGS, parseNames(song["warnings"]) });

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

        std::string url_encode(const std::string& raw) {
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
                t_filesize size = sourceFile->get_size(fb2k::noAbort);
                file::ptr targetFile;
                filesystem::g_open_write_new(targetFile, path.c_str(), fb2k::noAbort);
                targetFile->resize(size, fb2k::noAbort);
                sourceFile->g_transfer_file(sourceFile, targetFile, fb2k::noAbort);
                return true;
            }
            catch (const std::exception& e) {
                console::print("Error dumping file: ", e.what());
                SetSearchStatus(Error);
            }
            return false;
        }

        // Returns the file path adjusted for the name of the kara, and if the file already exists
        std::pair<std::string, bool> make_filepath(std::string name, std::string file) {
            try {
                // Replace windows forbidden symbols
                const std::string forbidden = R"(<>:"/\|?*)";
                std::replace_if(name.begin(), name.end(), [&](char c) {
                    return forbidden.find(c) != std::string::npos; 
                }, '_');

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
                    SetSearchStatus(Download);
                    std::string mediaUrl = !use_hs ? KaramoeUrl::media_dl + kara[MEDIAFILE] : KaramoeUrl::hardsub_dl + kara[HS_MEDIAFILE];
                    std::string lyricUrl = KaramoeUrl::lyric_dl + kara[SUBFILE];

                    file_info_impl info;
                    float i, tp, lra, measured_thresh, offset;
                    sscanf_s(kara[LOUDNORM].c_str(), "%f, %f, %f, %f, %f", &i, &tp, &lra, &measured_thresh, &offset);
                    float gain = REPLAY_GAIN_LUFT_TARGET - i;
                    float peak = (float)std::pow(10, (tp / 20));

                    info.meta_set("ARTIST", kara[SINGER].c_str());
                    info.meta_set("ALBUM", kara[FRANCHISE].c_str());
                    info.meta_set("TITLE", kara[TITLE].c_str());

                    // Loudnorm is for non hs file, might differ
                    if (!use_hs) {
                        info.info_set_replaygain_track_gain(gain);
                        info.info_set_replaygain_track_peak(peak);
                    }

                    fb2k::inWorkerThread([this, use_hs, mediaUrl, lyricUrl, mediaFile, subFile, info] {
                        file::ptr media = http_get(mediaUrl, fb2k::noAbort);
                        file::ptr lyrics;
                        if (!use_hs) {
                            lyrics = http_get(lyricUrl, fb2k::noAbort);
                        }

                        SetSearchStatus(Save);
                        write_to_disk(media, mediaFile.first);
                        if (!use_hs) {
                            write_to_disk(lyrics, subFile.first);
                        }

                        // Write tags
                        service_ptr_t<input_info_writer> writer;
                        file::ptr tagFile;
                        input_entry::g_open_for_info_write(writer, tagFile, mediaFile.first.c_str(), fb2k::noAbort);
                        writer->set_info(0, info, fb2k::noAbort);
                        writer->commit(fb2k::noAbort);

                        SetSearchStatus(Done);
                    });
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
                }
                catch (...) {
                    console::print("ERROR: Failed to queue");
                }
            });
        }
    };

    // Register the ui element to foobar
    class karamoe_ui_impl : public ui_element_impl<SearchUI> {};
    static service_factory_single_t<karamoe_ui_impl> g_karamoe_factory;

}