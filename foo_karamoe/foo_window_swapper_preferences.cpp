#include "stdafx.h"
#include "foo_window_swapper.h"
#include "helpers/atl-misc.h"
#include <SDK/coreDarkMode.h>
#include "resource.h"


namespace foo_window_swapper {
    class window_swapper_preference_instance
        : public preferences_page_instance, public CDialogImpl<window_swapper_preference_instance> {
    private:
        preferences_page_callback::ptr m_callback;

        fb2k::CCoreDarkModeHooks m_dark;

    public:
        enum {IDD = IDD_WSPREFERENCES};

        BEGIN_MSG_MAP(window_swapper_preference_instance)
            MSG_WM_INITDIALOG(OnInitDialog)
            COMMAND_HANDLER_EX(IDC_TITLE_PATTERN_DEFAULT, EN_CHANGE, OnEditChange)
            COMMAND_HANDLER_EX(IDC_TITLE_PATTERN_ALT, EN_CHANGE, OnEditChange)
            COMMAND_HANDLER_EX(IDC_FILE_EXTENSIONS, EN_CHANGE, OnEditChange)
        END_MSG_MAP()

        void OnEditChange(UINT, int, CWindow) {
            OnChanged();
        }

        BOOL OnInitDialog(CWindow, LPARAM) {
            m_dark.AddDialogWithControls(m_hWnd);

            SetDlgItemText(IDC_TITLE_PATTERN_DEFAULT, pfc::stringcvt::string_wide_from_utf8(Configs::default_pattern.get_value()).get_ptr());
            SetDlgItemText(IDC_TITLE_PATTERN_ALT, pfc::stringcvt::string_wide_from_utf8(Configs::alt_pattern.get_value()).get_ptr());
            SetDlgItemText(IDC_FILE_EXTENSIONS, pfc::stringcvt::string_wide_from_utf8(Configs::alt_files.get_value()).get_ptr());

            return FALSE;
        }

        
        window_swapper_preference_instance(preferences_page_callback::ptr callback) {
            m_callback = callback;
        }


        t_uint32 get_state() override {
            t_uint32 state = preferences_state::dark_mode_supported;
            state |= preferences_state::resettable;

            if (hasChanged()) {
                state |= preferences_state::changed;
            }
            return state;
        }

        pfc::string8 get_text_from_dlg_item(int item) {
            int len = GetDlgItemTextW(item, nullptr, 0);
            std::wstring wtext(len + 1, L'\0');
            GetDlgItemTextW(item, wtext.data(), len + 1);

            uGetDlgItemText(m_hWnd, item);
            pfc::stringcvt::string_utf8_from_wide text(wtext.c_str());
            pfc::string8 retval = text.toString();
            return retval;
        }

        void apply() override {
            Configs::default_pattern = uGetDlgItemText(m_hWnd, IDC_TITLE_PATTERN_DEFAULT);
            Configs::alt_pattern = uGetDlgItemText(m_hWnd, IDC_TITLE_PATTERN_ALT);
            Configs::alt_files = uGetDlgItemText(m_hWnd, IDC_FILE_EXTENSIONS);
        }

        void reset() override {
            SetDlgItemText(IDC_TITLE_PATTERN_DEFAULT, pfc::stringcvt::string_wide_from_utf8(Defaults::default_pattern).get_ptr());
            SetDlgItemText(IDC_TITLE_PATTERN_ALT, pfc::stringcvt::string_wide_from_utf8(Defaults::alt_pattern).get_ptr());
            SetDlgItemText(IDC_FILE_EXTENSIONS, pfc::stringcvt::string_wide_from_utf8(Defaults::alt_files).get_ptr());
            // The field values are changed, but until the user presses apply the actual configs remain
            OnChanged();
        }

        bool hasChanged() {
            return uGetDlgItemText(m_hWnd, IDC_TITLE_PATTERN_DEFAULT) != Configs::default_pattern ||
                uGetDlgItemText(m_hWnd, IDC_TITLE_PATTERN_ALT) != Configs::alt_pattern ||
                uGetDlgItemText(m_hWnd, IDC_FILE_EXTENSIONS) != Configs::alt_pattern;
        }

        void OnChanged() {
            m_callback->on_state_changed();
        }
    };

    class window_swapper_preferences : public preferences_page_impl<window_swapper_preference_instance> {
    private:

    public:
        const char* get_name() override {
            return "Window Swapper";
        }

        GUID get_guid() override {
            return { 0x7de21a6a, 0xb94e, 0x4cb6, { 0xa4, 0x7c, 0x6b, 0x21, 0xee, 0x72, 0xb1, 0x8e } };
        }

        GUID get_parent_guid() override {
            return preferences_page::guid_tools;
        }
    };
    static preferences_page_factory_t<window_swapper_preferences> g_fs_swap_preferences;
}