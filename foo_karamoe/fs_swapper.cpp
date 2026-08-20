#include "stdafx.h"
#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>
#include <foobar2000/helpers/readers.h>

namespace foo_fsswap {

    const GUID guid_enabled = { 0x6b70f7d5, 0xd379, 0x450a, { 0x86, 0xf3, 0x59, 0x8a, 0xa, 0xf0, 0xde, 0xbd } };
    const GUID fs_swap_menu_group_guid = { 0x80934b3b, 0x9bd, 0x40cb, { 0x98, 0x73, 0x62, 0x81, 0x55, 0x1f, 0x2c, 0xbb } };

    class FSSwapper : public initquit, private play_callback {

    private:
        cfg_bool cfg_enabled = cfg_bool(guid_enabled, true);

        HWND m_audioWindow = NULL;
        HWND m_videoWindow = NULL;

        std::vector<std::function<void(boolean enabled, boolean hasVideo, boolean hasAudio)>> listeners;

    public:

        void on_init() {
            console::print("Window swapper initialized");
            if (cfg_enabled) {
                console::print("...As active");
                play_callback_manager::get()->register_callback(this, play_callback::flag_on_playback_new_track, true);
                FindWindows();
            }
        }

        void on_quit() {
            if (cfg_enabled) {
                play_callback_manager::get()->unregister_callback(this);
            }
        }

        bool IsActive() {
            return cfg_enabled;
        }

        void Toggle() {
            cfg_enabled = !cfg_enabled;
            if (cfg_enabled) {
                console::print("FSSwap active");
                play_callback_manager::get()->register_callback(this, play_callback::flag_on_playback_new_track, true);
                FindWindows();
            }
            else {
                console::print("FSSwap Disabled");
                play_callback_manager::get()->unregister_callback(this);
            }
        }

        void FindWindows() {
            metadb_handle_ptr playing;
            static_api_ptr_t<playback_control>()->get_now_playing(playing);

            if (m_videoWindow == NULL || !IsWindow(m_videoWindow)) {
                std::wstring windowName = getVideoWindowName(playing);
                if (windowName.empty()) {
                    m_videoWindow = NULL;
                }
                else {
                    m_videoWindow = FindWindow(NULL, windowName.c_str());
                }
            }

            if (m_audioWindow == NULL || !IsWindow(m_audioWindow)) {
                std::wstring windowName = getAudioWindowName(playing);
                if (windowName.empty()) {
                    m_audioWindow = NULL;
                }
                else {
                    m_audioWindow = FindWindow(NULL, windowName.c_str());
                }
            }

            popup_message_v3::query_t q;
            if (m_audioWindow != NULL && m_videoWindow != NULL) {
                q.title = "Success";
                q.msg = "Both windows found successfully\nWill automatically swap between video and audio lyric panels on song change";
                q.buttons = popup_message_v3::buttonOK;
                q.icon = popup_message_v3::iconInformation;
            }
            else {
                q.title = "Window swapper error";
                q.icon = popup_message_v3::iconError;
                q.buttons = popup_message_v3::buttonOK | popup_message_v3::buttonRetry | popup_message_v3::buttonAbort;
                if (m_audioWindow != NULL) {
                    q.msg = "Failed to find video window\nAutomatic swapping between video and audio lyric panels might not work";
                }
                else if (m_videoWindow != NULL) {
                    q.msg = "Failed to find audio window\nAutomatic swapping between video and audio lyric panels might not work";
                }
                else {
                    q.msg = "Failed to find either window\nAutomatic swapping between video and audio lyric panels might not work";
                }
            }
            q.show();
        }


        // Dummy implementations of member methods that do nothing in our case
        void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override {}
        void on_playback_stop(play_control::t_stop_reason p_reason) override {}
        void on_playback_seek(double p_time) override {}
        void on_playback_pause(bool p_state) override {}
        void on_playback_edited(metadb_handle_ptr p_track) override {}
        void on_playback_dynamic_info(const file_info& p_info) override {}
        void on_playback_dynamic_info_track(const file_info& p_info) override {}
        void on_playback_time(double p_time) override {}
        void on_volume_change(float p_new_val) override {}

        void on_playback_new_track(metadb_handle_ptr p_track) override {
            try {
                std::string path = p_track->get_path();
                auto fs = filesystem::get(path.c_str());
                std::string ext = fs->get_extension(path.c_str()).toString();

                bool is_video = ext == "mp4";

                if (!IsWindow(is_video ? m_videoWindow : m_audioWindow)) {
                    // No window yet
                    std::wstring wanted;
                    if (is_video) {
                        m_videoWindow = findAndFocusWindow(getVideoWindowName(p_track));
                    }
                    else {
                        m_audioWindow = findAndFocusWindow(getAudioWindowName(p_track));
                    }
                }
                else {
                    focusWindow(is_video ? m_videoWindow : m_audioWindow);
                }
            }
            catch (std::exception e) {
                // Just print error to console, don't interrupt normal operation
                console::print(e.what());
            }
        }

        std::wstring getAudioWindowName(metadb_handle_ptr& p_track) {
            // TODO: actually handle different names, for now hardcoded
            return L"ESLyric";
        }

        std::wstring getVideoWindowName(metadb_handle_ptr& p_track) {
            try {
                if (p_track == nullptr) {
                    return L"";
                }
                const file_info& info = p_track->get_full_info_ref(fb2k::noAbort)->info();
                std::string title = info.meta_exists("title") ? info.meta_get("title", 0) : fb2k::filename(p_track->get_path()).toString();
                std::string artist = info.meta_exists("artist") ? info.meta_get("artist", 0) : "?";
                std::string album = info.meta_exists("album") ? info.meta_get("album", 0) : "";
                std::string windowName = title + " - " + artist + (!album.empty() ? " (" + album + ")" : "");
                std::wstring wide_name(windowName.begin(), windowName.end());
                return wide_name;
            }
            catch (std::exception& e) {
                console::print(e.what());
                std::string path = p_track->get_path();
                std::wstring wide_name(path.begin(), path.end());
                return wide_name;
            }
        }

        HWND findAndFocusWindow(std::wstring name) {
            if (name.empty()) {
                return NULL;
            }
            HWND window = FindWindow(NULL, name.c_str());
            if (window) {
                focusWindow(window);
                return window;
            }
            return NULL;
        }

        void focusWindow(HWND window) {
            SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

    };
   static service_factory_single_t<FSSwapper> g_fs_swapper_initquit;



    class FSSwapMenuGroup : public mainmenu_group_popup {
    public:
        GUID get_guid() {
            return fs_swap_menu_group_guid;
        }

        GUID get_parent() {
            return mainmenu_groups::view;
        }

        t_uint32 get_sort_priority() {
            return -99;
        }

        void get_display_string(pfc::string_base& p_out) {
            p_out = "Window Swapper";
        }
    };
    static service_factory_single_t<FSSwapMenuGroup> g_fs_swapper_menu_group;



    class FSSwapMenu : public mainmenu_commands {

    public:

        enum Command {
            cmd_activate_toggle,
            cmd_find_windows,
            cmd_count,  // Last enum, automatically gets the number of commands above it
        };

        unsigned get_command_count() override {
            return cmd_count;
        }

        void get_name(unsigned index, pfc::string_base& out) override {
            switch (index) {
            case cmd_activate_toggle:
                out = "Activate / disable";
                break;
            case cmd_find_windows:
                out = "Find windows";
                break;
            default:
                break;
            }
        }

        bool get_display(t_uint32 p_index, pfc::string_base& p_text, t_uint32& p_flags) override {
            switch (p_index) {
            case cmd_activate_toggle:

                if (g_fs_swapper_initquit.get_static_instance().IsActive()) {
                    p_flags = mainmenu_commands::flag_checked;
                    get_name(p_index, p_text);
                }
                else {
                    get_name(p_index, p_text);
                }
                return true;
            case cmd_find_windows:
                get_name(p_index, p_text);
                break;
            default:
                return false;
            }
        }


        GUID get_command(unsigned index) override {
            switch (index) {
            case cmd_activate_toggle:
                return { 0xab6b8c55, 0x7545, 0x4bf6, { 0xb6, 0xcd, 0xaf, 0x3b, 0x55, 0x3a, 0xc, 0x8b } };
            case cmd_find_windows:
                return { 0xa6b2cd27, 0x8b5a, 0x47b6, { 0x93, 0xfe, 0x4d, 0xa6, 0x41, 0x72, 0x55, 0x9c } };

            default:
                return pfc::guid_null;
            }
        }

        GUID get_parent() override {
            return fs_swap_menu_group_guid;
        }

        bool get_description(unsigned p_index, pfc::string_base& p_out) {
            switch (p_index) {
            case cmd_activate_toggle:
                p_out = "Activate or disable the window swapper. Also searches for windows on activation.";
                break;
            case cmd_find_windows:
                p_out = "Searches and saves windows, if not already found.";
                break;
            default:
                return false;
            }
            return true;
        }

        void execute(t_uint32 p_index, ctx_t p_callback) {
            switch (p_index) {
            case cmd_activate_toggle:
                g_fs_swapper_initquit.get_static_instance().Toggle();
                break;
            case cmd_find_windows:
                g_fs_swapper_initquit.get_static_instance().FindWindows();
                break;
            }
        }
    };
    static mainmenu_commands_factory_t<FSSwapMenu> g_fs_swapper_command;
}