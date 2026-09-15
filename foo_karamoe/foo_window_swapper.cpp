#include "stdafx.h"

#include "foo_window_swapper.h"
#include <sstream>

namespace foo_window_swapper {

    class WindowSwapper : public initquit, private play_callback {

    private:
        HWND m_defaultWindow = NULL;
        HWND m_altWindow = NULL;

        void register_self() {
            play_callback_manager::get()->register_callback(this, play_callback::flag_on_playback_new_track, true);
        }

        void unregister_self() {
            play_callback_manager::get()->unregister_callback(this);
        }

    public:

        void on_init() {
            if (Configs::enabled) {
                register_self();
                FindWindows();
            }
        }

        void on_quit() {
            if (Configs::enabled) {
                unregister_self();
            }
        }

        bool IsActive() {
            return Configs::enabled;
        }

        void Toggle() {
            Configs::enabled = !Configs::enabled;
            if (Configs::enabled) {
                console::print("Window Swapper active");
                register_self();
                FindWindows();
            }
            else {
                console::print("Window Swapper Disabled");
                unregister_self();
            }
        }

        void FindWindows() {
            metadb_handle_ptr playing;
            static_api_ptr_t<playback_control>()->get_now_playing(playing);

            if (m_altWindow == NULL || !IsWindow(m_altWindow)) {
                std::wstring windowName = getWindowName(ALT, playing);
                if (windowName.empty()) {
                    m_altWindow = NULL;
                }
                else {
                    m_altWindow = FindWindow(NULL, windowName.c_str());
                }
            }

            if (m_defaultWindow == NULL || !IsWindow(m_defaultWindow)) {
                std::wstring windowName = getWindowName(DEFAULT, playing);
                if (windowName.empty()) {
                    m_defaultWindow = NULL;
                }
                else {
                    m_defaultWindow = FindWindow(NULL, windowName.c_str());
                }
            }

            popup_message_v3::query_t q;
            if (m_defaultWindow != NULL && m_altWindow != NULL) {
                q.title = "Success";
                if (Configs::enabled) {
                    q.msg = "Both windows found successfully\nWill automatically swap between video and audio lyric panels on song change";
                    q.buttons = popup_message_v3::buttonOK;
                }
                else {
                    q.msg = "Both windows found successfully\nHowever, the service is turned off.\nWant to turn it on now?";
                    q.buttons = popup_message_v3::buttonYes | popup_message_v3::buttonNo;
                    q.reply = fb2k::makeCompletionNotify([this](unsigned result) {
                        switch (result) {
                        case popup_message_v3::buttonYes:
                            Configs::enabled = true;
                            register_self();
                            break;
                        case popup_message_v3::buttonNo:
                            break;
                        }
                        });
                }
                q.icon = popup_message_v3::iconInformation;
            }
            else {
                q.title = "Window swapper error";
                q.icon = popup_message_v3::iconError;
                q.buttons = popup_message_v3::buttonIgnore | popup_message_v3::buttonRetry;

                std::string not_found;
                if (m_defaultWindow != NULL) {
                    not_found = "Failed to find alt window";
                }
                else if (m_altWindow != NULL) {
                    not_found = "Failed to find default window";
                }
                else {
                    not_found = "Failed to find either window";
                }

                std::string not_working = "Automatic swapping between windows might not work";

                std::string status;
                if (Configs::enabled) {
                    q.buttons |= popup_message_v3::buttonAbort;
                    status = "Abort to turn the service off";
                }
                else {
                    status = "Service is currently off";
                }

                // New pointer to keep the underlying string alive until message is no longer needed
                std::string* message = new std::string(not_found + "\n" + not_working + "\n" + status);
                q.msg = message->c_str();

                q.reply = fb2k::makeCompletionNotify([this, message](unsigned result) {
                    switch (result) {
                    case popup_message_v3::buttonIgnore:
                        break;
                    case popup_message_v3::buttonRetry:
                        FindWindows();
                        break;
                    case popup_message_v3::buttonAbort:
                        Configs::enabled = false;
                        unregister_self();
                        break;
                    }
                    delete message;
                });
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

                bool is_alt = false;
                std::string alt_extensions = Configs::alt_files.get_value().toString();
                std::istringstream iss(alt_extensions);
                std::string alt_ext;
                while (iss >> alt_ext) {
                    if (alt_ext == ext) {
                        is_alt = true;
                        break;
                    }
                }

                if (!IsWindow(is_alt ? m_altWindow : m_defaultWindow)) {
                    // No window yet
                    std::wstring wanted;
                    if (is_alt) {
                        m_altWindow = findAndFocusWindow(getWindowName(ALT, p_track));
                    }
                    else {
                        m_defaultWindow = findAndFocusWindow(getWindowName(DEFAULT, p_track));
                    }
                }
                else {
                    focusWindow(is_alt ? m_altWindow : m_defaultWindow);
                }
            }
            catch (std::exception e) {
                // Just print error to console, don't interrupt normal operation
                console::print(e.what());
            }
        }


        std::wstring getWindowName(Window window, metadb_handle_ptr& p_track) {
            try {
                if (p_track == nullptr) {
                    return L"";
                }

                auto pattern = (window == DEFAULT) ? Configs::default_pattern.get_value() : Configs::alt_pattern.get_value();
                service_ptr_t<titleformat_object> script;
                static_api_ptr_t<titleformat_compiler> compiler;
                if (!compiler->compile(script, pattern)) {
                    return L"";
                }
                pfc::string8 result;
                p_track->format_title(
                    NULL,
                    result,
                    script,
                    NULL
                );
                std::string name = result.toString();
                pfc::stringcvt::string_wide_from_utf8 wide_name(result);
                return wide_name.get_ptr();
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
    static service_factory_single_t<WindowSwapper> g_window_swapper_initquit;



    class WindowSwapperMenuGroup : public mainmenu_group_popup {
    public:
        GUID get_guid() { return window_swapper_menu_group_guid; }
        GUID get_parent() { return mainmenu_groups::view; }
        t_uint32 get_sort_priority() { return -99; }
        void get_display_string(pfc::string_base& p_out) { p_out = "Window Swapper"; }
    };
    static service_factory_single_t<WindowSwapperMenuGroup> g_window_swapper_menu_group;


    class WindowSwapperMenu : public mainmenu_commands {

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
                if (g_window_swapper_initquit.get_static_instance().IsActive()) {
                    p_flags = mainmenu_commands::flag_checked;
                    get_name(p_index, p_text);
                }
                else {
                    get_name(p_index, p_text);
                }
                return true;
            case cmd_find_windows:
                get_name(p_index, p_text);
                return true;
            }
            return false;
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
            return window_swapper_menu_group_guid;
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
                g_window_swapper_initquit.get_static_instance().Toggle();
                break;
            case cmd_find_windows:
                g_window_swapper_initquit.get_static_instance().FindWindows();
                break;
            }
        }
    };
    static mainmenu_commands_factory_t<WindowSwapperMenu> g_window_swapper_command;
}