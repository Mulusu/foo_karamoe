#include "stdafx.h"
#include <foobar2000/helpers/foobar2000+atl.h>
#include <foobar2000/helpers/atl-misc.h>
#include <foobar2000/helpers/CListControlFb2kColors.h>
#include <libPPUI/CEditWithButtons.h>
#include <libPPUI/CListControlSimple.h>
#include <SDK/coreDarkMode.h>
#include "karamoe_service.h"

namespace foo_karamoe {

    enum SearchStatus {
        // Numbers are the unicode number of the emoji used to indicate that status
        Idle = 0x1F4A4,         // System is idle
        Waiting = 0x23f3,       // Waiting for typing debounce timer to fire search
        Network = 0x1F4E1,      // HTTP search query or download underway
        Parsing = 0x1F50D,      // Parsing response of the HTTP search query
        Done = 0x2705,          // Search / Queueing+download is done
        Filing = 0x1F4DD,       // Preparing files
        Save = 0x1F4BE,         // Saving files to filesystem
        Error = 0x26A0          // Operation failed
    };

    const int ID_TIMER_DEBOUNCE = 1001;  // Id for typing debounce timer
    const int DEBOUNCE_WAIT_TIME = 750;  // Wait after typing stops before search is fired, ms
    const int SEARCH_BAR_HEIGHT = 30;
    const int SEARCH_STATUS_WIDTH = SEARCH_BAR_HEIGHT;


    class ResultCol {
    public:
        std::string m_label;	// Label for the column in the header
        KaraField m_field;		// Which kara field value to write in this column
        unsigned int m_width;	// Width of the column

        ResultCol(unsigned int width, std::string label, KaraField field) : m_width(width), m_label(label), m_field(field) {};
    };

    const static std::vector<ResultCol> rows{
        // The columns are put in the order defined here
        ResultCol(200, "Title", TITLE),
        ResultCol(200, "Band / Singer", ARTIST),
        ResultCol(200, "Writer", WRITER),
        ResultCol(200, "Franchise", FRANCHISE),
        ResultCol(200, "Type", TYPE),
        ResultCol(75, "Language", LANG),
        ResultCol(75, "Length", LENGTH),
        ResultCol(200, "Collections", COLLECTIONS),
        ResultCol(200, "Warnings", WARNINGS)
    };


	class KaramoeUI_impl : public ui_element_instance, public CWindowImpl<KaramoeUI_impl> {
    private:
        KaramoeService* m_service;
        ui_element_instance_callback::ptr m_callback;
        ui_element_config::ptr m_config;
        SearchStatus m_searchStatus;
        CEditWithButtons m_edit;
        CListControlFb2kColors<CListControlSimple> m_list;

        WTL::CStatic m_statusIcon;
        std::unordered_map<SearchStatus, WTL::CStatic> m_status_icons;
        fb2k::CCoreDarkModeHooks m_dark;


    public:
        static void g_get_name(pfc::string_base & out) { out = "Karamoe UI Element"; }
        static const char* g_get_description() { return "UI Element for Karaoke Mugen plugin."; }
        static GUID g_get_guid() { return { 0x1230c9a2, 0xd37e, 0x4103, { 0xaa, 0x10, 0xba, 0xf7, 0x1f, 0x20, 0xe5, 0x28 } }; }
        static GUID g_get_subclass() { return ui_element_subclass_utility; }
        HWND get_wnd() { return m_hWnd; }

        static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
        void set_configuration(ui_element_config::ptr config) { m_config = config; }
        ui_element_config::ptr get_configuration() { return m_config; }


        BEGIN_MSG_MAP(KaramoeUI_impl)
            MESSAGE_HANDLER(WM_CREATE, OnCreate)
            MESSAGE_HANDLER(WM_SIZE, OnSize)
            MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
            MESSAGE_HANDLER(WM_TIMER, OnTimer)
            COMMAND_CODE_HANDLER(EN_CHANGE, OnSearchChange)
            MESSAGE_HANDLER(WM_CONTEXTMENU, OnRightClick)
        END_MSG_MAP()


        KaramoeUI_impl(ui_element_config::ptr config, ui_element_instance_callback::ptr callback) {
            m_service = new KaramoeService();
            m_searchStatus = Idle;
            m_callback = callback;
            if (get_configuration() == NULL) {
                // Set empty config, so position in layout can be saved by foobar
                set_configuration(g_get_default_configuration());
            }
        }


        void initialize_window(HWND parent) {
            WIN32_OP(Create(parent) != NULL);
        }


        /* Create necessary UI elements */
        LRESULT OnCreate(UINT uint, WPARAM wparam, LPARAM lparam, BOOL & handled) {
            // Create the UI elements. Sizes left to nullptr, they are set in OnSize
            m_edit.Create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL);
            m_edit.SendMessage(EM_SETCUEBANNER, FALSE, (LPARAM)L"Search...");
            m_edit.ModifyStyleEx(0, WS_EX_CLIENTEDGE, 0);

            // Request status icon
            m_statusIcon.Create(m_hWnd, nullptr, nullptr, WS_CHILD | WS_VISIBLE);
            m_statusIcon.ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

            LOGFONT lf = {};
            lf.lfHeight = SEARCH_BAR_HEIGHT;
            //wcscpy_s(lf.lfFaceName, L"Segoe UI Emoji");
            HFONT hFont = CreateFontIndirect(&lf);
            m_statusIcon.SetFont(hFont);
            SetSearchStatus(Idle);

            // Result list
            m_list.Create(m_hWnd);
            m_list.ModifyStyleEx(LVS_EX_HEADERDRAGDROP, 0);
            for (ResultCol row : rows) {
                m_list.AddColumn(row.m_label.c_str(), row.m_width);
            }

            // Tell foobar to manage the colors for darkmode
            m_dark.AddDialogWithControls(m_hWnd);

            return 0;
        }


        LRESULT OnSize(UINT, WPARAM, LPARAM lParam, BOOL&) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            m_edit.SetWindowPos(nullptr,
                SEARCH_STATUS_WIDTH, 0, width - SEARCH_STATUS_WIDTH, SEARCH_BAR_HEIGHT,
                0
            );

            m_statusIcon.SetWindowPos(nullptr,
                0, 0, SEARCH_STATUS_WIDTH, SEARCH_BAR_HEIGHT,
                0
            );

            m_list.SetWindowPos(nullptr,
                0, SEARCH_BAR_HEIGHT, width, height - SEARCH_BAR_HEIGHT,
                0
            );
            return 0;
        }


        void ClearResultList() {
            for (unsigned int i = 0; i < m_list.GetItemCount(); i++) {
                delete (Kara*)m_list.GetItemUserData(i);  // Destroy Kara stored there
            }
            m_list.RemoveAllItems();
        }


        LRESULT OnSearchChange(WORD, WORD, HWND, BOOL&) {
            KillTimer(ID_TIMER_DEBOUNCE);  // Stop existing timer
            pfc::string8 query;
            uGetWindowText(m_edit, query);
            if (query.get_length() == 0) {
                SetSearchStatus(Idle);
                ClearResultList();
                return 0;
            }
            SetSearchStatus(Waiting);
            SetTimer(ID_TIMER_DEBOUNCE, DEBOUNCE_WAIT_TIME, nullptr);
            return 0;
        }


        LRESULT OnDestroy(UINT, WPARAM, LPARAM lParam, BOOL&) {
            KillTimer(ID_TIMER_DEBOUNCE);
            return 0;
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
            enum {ADD_TO_QUEUE = 1};
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
            menu.AppendMenuW(MF_STRING, ADD_TO_QUEUE, L"Add to playback queue");

            int cmd = menu.TrackPopupMenu(
                TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                pt.x, pt.y, m_hWnd
            );

            switch (cmd) {
            case ADD_TO_QUEUE:
                size_t selectedItem = m_list.GetFirstSelected();
                Kara* selected = (Kara*)m_list.GetItemUserData(selectedItem);
                queue_kara(selected);
                break;
                // TODO: add more options?
            }
            return 0;
        }


        void queue_kara(Kara* selected) {
            Kara kara = *selected;
            // Queueing involves downloading the files --> do in worker thread
            fb2k::inWorkerThread([this, kara] {

                SetSearchStatus(Filing);
                std::pair<std::string, std::string> paths = m_service->prepare_files(kara);
                file_info_impl info = m_service->queue_file(kara, paths.first);

                // Download files if they don't yet exist
                if (!filesystem::g_exists(paths.first.c_str(), fb2k::noAbort) || (!paths.second.empty() && !filesystem::g_exists(paths.second.c_str(), fb2k::noAbort))) {

                    SetSearchStatus(Network);
                    std::pair<file::ptr, file::ptr> files = m_service->download_files(kara);

                    SetSearchStatus(Save);
                    m_service->write_to_disk(files.first, paths.first);
                    if (!paths.second.empty()) {
                        m_service->write_to_disk(files.second, paths.second);
                    }

                    // Lastly, write the already calculated metadata to the actual file
                    m_service->write_tags(info, paths.first);
                }
                
                SetSearchStatus(Done);
                });
        }


        LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
            KillTimer(ID_TIMER_DEBOUNCE);
            std::string query = uGetWindowText(m_edit).toString();
            ClearResultList();
            // Might take a moment to work, queue to worker thread to not freeze UI
            fb2k::inWorkerThread([this, query] {
                try {
                    SetSearchStatus(Network);
                    nlohmann::json search_results = m_service->search(query);

                    SetSearchStatus(Parsing);
                    std::vector<Kara*> results = m_service->parse_karas(search_results);

                    for (Kara* kara : results) {
                        AddResultRow(kara);
                    }
                    SetSearchStatus(Done);
                }
                catch (std::exception e) {
                    SetSearchStatus(Error);
                    // TODO: error message popup?
                    console::print("Error getting karamoe search results: ", e.what());
                }
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
	static service_factory_single_t<ui_element_impl<KaramoeUI_impl>> g_karamoe_factory;
}