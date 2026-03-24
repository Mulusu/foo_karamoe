#include "stdafx.h"
#include <iostream>
#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>
#include <libPPUI/win32_op.h>
#include <foobar2000/helpers/CListControlFb2kColors.h>
#include <libPPUI/CEditWithButtons.h>
#include <libPPUI/CListControlSimple.h>
#include "foo_karamoe.h"

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
                0, 0, max(width - SEARCH_STATUS_WIDTH, 0), SEARCH_BAR_HEIGHT,
                0
            );
            m_statusIcon.SetWindowPos(nullptr,
                width - SEARCH_STATUS_WIDTH, 0, width, SEARCH_BAR_HEIGHT,
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
                m_edit.SendMessage(WM_SETFONT, (WPARAM)fontDefault, TRUE);
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
                    wchar_t emoji[] = { m_searchStatus, 0 };
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
                fb2k::inCpuWorkerThread([this, selected] {
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
            pfc::string8 query = uGetWindowText(m_edit);
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
    };

    // Register the ui element to foobar
    class karamoe_ui_impl : public ui_element_impl<SearchUI> {};
    static service_factory_single_t<karamoe_ui_impl> g_karamoe_factory;
}