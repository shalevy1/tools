// StExBar - an explorer toolbar

// Copyright (C) 2007-2009, 2012, 2014 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "stdafx.h"
#include "SRBand.h"
#include "resource.h"
#include "version.h"
#include <algorithm>
#include "RenameDlg.h"
#include "InfoDlg.h"
#include "NumberReplacer.h"
#include <string>


#define IDT_RENAME 101


CRenameDlg::CRenameDlg(HWND hParent)
    : m_hParent(hParent)
{
}

CRenameDlg::~CRenameDlg(void)
{
}

LRESULT CRenameDlg::DlgFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        {
            InitDialog(hwndDlg, IDI_RENAME);

            m_resizer.Init(hwndDlg);
            m_resizer.AddControl(hwndDlg, IDC_MATCHLABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACELABEL, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDC_CASEINSENSITIVE, RESIZER_TOPLEFT);
            m_resizer.AddControl(hwndDlg, IDHELP, RESIZER_TOPRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_MATCHSTRING, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_REPLACESTRING, RESIZER_TOPLEFTRIGHT);
            m_resizer.AddControl(hwndDlg, IDC_FILELIST, RESIZER_TOPLEFTBOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDOK, RESIZER_BOTTOMRIGHT);
            m_resizer.AddControl(hwndDlg, IDCANCEL, RESIZER_BOTTOMRIGHT);

            // set up the list control
            DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
            HWND hListCtrl = GetDlgItem(hwndDlg, IDC_FILELIST);
            if (hListCtrl)
            {
                ListView_DeleteAllItems(hListCtrl);
                ListView_SetExtendedListViewStyle(hListCtrl, exStyle);
                LVCOLUMN lvc = {0};
                lvc.mask = LVCF_TEXT;
                lvc.fmt = LVCFMT_LEFT;
                lvc.cx = -1;
                lvc.pszText = _T("filename");
                ListView_InsertColumn(hListCtrl, 0, &lvc);
                lvc.pszText = _T("renamed");
                ListView_InsertColumn(hListCtrl, 1, &lvc);
            }
            FillRenamedList();
            CRegStdDWORD dlgWidth = CRegStdDWORD(_T("Software\\StExBar\\renameWidth"), 0);
            CRegStdDWORD dlgHeight = CRegStdDWORD(_T("Software\\StExBar\\renameHeight"), 0);

            if (DWORD(dlgWidth) && DWORD(dlgHeight))
            {
                // change the width and height of the dialog to the saved size
                ::SetWindowPos(*this, NULL, 0, 0, DWORD(dlgWidth), DWORD(dlgHeight), SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER);

                // and center the dialog again on the parent window (usually the explorer)
                HWND hwndOwner = ::GetParent(hwndDlg);
                if (hwndOwner == NULL)
                    hwndOwner = ::GetDesktopWindow();

                RECT rcOwner;
                RECT rcDlg;
                RECT rc;
                GetWindowRect(hwndOwner, &rcOwner);
                GetWindowRect(hwndDlg, &rcDlg);
                CopyRect(&rc, &rcOwner);

                OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
                OffsetRect(&rc, -rc.left, -rc.top);
                OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

                SetWindowPos(hwndDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0, SWP_NOSIZE);
            }

            m_AutoCompleteRen1.Load(_T("Software\\StExBar\\History"), _T("Ren1"));
            m_AutoCompleteRen1.Init(GetDlgItem(hwndDlg, IDC_MATCHSTRING));
            m_AutoCompleteRen2.Load(_T("Software\\StExBar\\History"), _T("Ren2"));
            m_AutoCompleteRen2.Init(GetDlgItem(hwndDlg, IDC_REPLACESTRING));

            CRegStdString ren1Reg = CRegStdString(_T("Software\\StExBar\\ren1Text"));
            CRegStdString ren2Reg = CRegStdString(_T("Software\\StExBar\\ren2Text"));
            std::wstring ren1Text = ren1Reg;
            std::wstring ren2Text = ren2Reg;
            if (ren1Text.size())
                SetDlgItemText(*this, IDC_MATCHSTRING, ren1Text.c_str());
            if (ren2Text.size())
                SetDlgItemText(*this, IDC_REPLACESTRING, ren2Text.c_str());

            SendDlgItemMessage(hwndDlg, IDC_CASEINSENSITIVE, BM_SETCHECK, BST_CHECKED, 0);
            ::SetFocus(::GetDlgItem(hwndDlg, IDC_MATCHSTRING));
        }
        return (INT_PTR)TRUE;
    case WM_SIZE:
        m_resizer.DoResize(LOWORD(lParam), HIWORD(lParam));
        return (INT_PTR)TRUE;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO * mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = m_resizer.GetDlgRect()->right;
            mmi->ptMinTrackSize.y = m_resizer.GetDlgRect()->bottom;
            return (INT_PTR)0;
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            {
                TCHAR buf[MAX_PATH];
                HWND hMatchstring = GetDlgItem(*this, IDC_MATCHSTRING);
                if (hMatchstring == NULL)
                    return (INT_PTR)TRUE;
                GetWindowText(hMatchstring, buf, _countof(buf));
                m_sMatch = buf;
                HWND hReplaceString = GetDlgItem(*this, IDC_REPLACESTRING);
                if (hReplaceString == NULL)
                    return (INT_PTR)TRUE;
                GetWindowText(hReplaceString, buf, _countof(buf));
                m_sReplace = buf;
                CRegStdString ren1Reg(_T("Software\\StExBar\\ren1Text"));
                CRegStdString ren2Reg(_T("Software\\StExBar\\ren2Text"));
                ren1Reg = m_sMatch;
                ren2Reg = m_sReplace;
                m_AutoCompleteRen1.AddEntry(m_sMatch.c_str());
                m_AutoCompleteRen2.AddEntry(m_sReplace.c_str());

                m_fl = std::regex_constants::ECMAScript;
                if (SendDlgItemMessage(*this, IDC_CASEINSENSITIVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    m_fl |= std::regex_constants::icase;
            }
            // fall through
        case IDCANCEL:
            {
                CRegStdDWORD dlgWidth(_T("Software\\StExBar\\renameWidth"), 0);
                CRegStdDWORD dlgHeight(_T("Software\\StExBar\\renameHeight"), 0);
                RECT rc;
                ::GetWindowRect(*this, &rc);
                dlgWidth = rc.right-rc.left;
                dlgHeight = rc.bottom-rc.top;
                m_AutoCompleteRen1.Save();
                m_AutoCompleteRen2.Save();

                EndDialog(*this, LOWORD(wParam));
            }
            return (INT_PTR)TRUE;
        case IDC_MATCHSTRING:
        case IDC_REPLACESTRING:
            if (HIWORD(wParam)==EN_CHANGE)
            {
                ::SetTimer(*this, IDT_RENAME, 500, NULL);
            }
            return (INT_PTR)TRUE;
        case IDHELP:
            CInfoDlg::ShowDialog(IDR_REGEXHELP, hResource);
            return (INT_PTR)TRUE;
        }
        break;
    case WM_TIMER:
        {
            ::KillTimer(*this, IDT_RENAME);
            FillRenamedList();
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR pnmhdr = (LPNMHDR)lParam;
            switch (pnmhdr->code)
            {
            case NM_CUSTOMDRAW:
                {
                    if (pnmhdr->idFrom == IDC_FILELIST)
                    {
                        LPNMLVCUSTOMDRAW lpcd = (LPNMLVCUSTOMDRAW)lParam;
                        // use the default processing if not otherwise specified
                        SetWindowLongPtr(*this, DWLP_MSGRESULT, CDRF_DODEFAULT);
                        if (lpcd->nmcd.dwDrawStage == CDDS_PREPAINT)
                        {
                            SetWindowLongPtr(*this, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                            return CDRF_NOTIFYITEMDRAW;
                        }
                        if (lpcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                        {
                            // This is the prepaint stage for an item. Here's where we set the
                            // item's text color. Our return value will tell Windows to draw the
                            // item itself, but it will use the new color we set here.
                            COLORREF crText = GetSysColor(COLOR_WINDOWTEXT);

                            if (lpcd->nmcd.lItemlParam == 0)
                                crText = GetSysColor(COLOR_GRAYTEXT);

                            // Store the color back in the NMLVCUSTOMDRAW struct.
                            lpcd->clrText = crText;

                            // Tell Windows to paint the control itself.
                            SetWindowLongPtr(*this, DWLP_MSGRESULT, CDRF_DODEFAULT);
                            return CDRF_DODEFAULT;
                        }
                    }
                }
            }
        }
        break;
    case WM_HELP:
        {
            CInfoDlg::ShowDialog(IDR_REGEXHELP, hResource);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

bool CRenameDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        switch (pMsg->wParam)
        {
        case VK_DELETE:
            {
                m_AutoCompleteRen1.RemoveSelected();
                m_AutoCompleteRen2.RemoveSelected();
            }
            break;
        }
    }
    return false;
}

void CRenameDlg::FillRenamedList()
{
    HWND hListCtrl = GetDlgItem(*this, IDC_FILELIST);
    TCHAR buf[MAX_PATH];
    HWND hMatchstring = GetDlgItem(*this, IDC_MATCHSTRING);
    if (hMatchstring == NULL)
        return;
    GetWindowText(hMatchstring, buf, _countof(buf));
    m_sMatch = buf;
    if (m_sMatch.empty())
        return;
    HWND hReplaceString = GetDlgItem(*this, IDC_REPLACESTRING);
    if (hReplaceString == NULL)
        return;
    GetWindowText(hReplaceString, buf, _countof(buf));
    m_sReplace = buf;

    SendMessage(hListCtrl, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(hListCtrl);
    // we also need a map which assigns each file its renamed equivalent
    std::map<std::wstring, std::wstring, __lesscasecmp> renamedmap;

    try
    {
        m_fl = std::regex_constants::ECMAScript;
        if (SendDlgItemMessage(*this, IDC_CASEINSENSITIVE, BM_GETCHECK, 0, 0) == BST_CHECKED)
            m_fl |= std::regex_constants::icase;
        const std::wregex regCheck(m_sMatch, m_fl);

        NumberReplaceHandler handler(m_sReplace);
        std::wstring replaced;
        for (auto it = m_filelist.begin(); it != m_filelist.end(); ++it)
        {
            replaced = std::regex_replace(*it, regCheck, m_sReplace);
            replaced = handler.ReplaceCounters(replaced);
            renamedmap[*it] = replaced;
        }
        // now fill in the list of files which got renamed
        int iItem = 0;
        TCHAR textbuf[MAX_PATH] = {0};
        for (std::map<std::wstring, std::wstring, __lesscasecmp>::iterator it = renamedmap.begin(); it != renamedmap.end(); ++it)
        {
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT|LVIF_PARAM;
            _tcscpy_s(textbuf, _countof(textbuf), it->first.c_str());
            lvi.pszText = textbuf;
            lvi.iItem = iItem;
            // we use the lParam to store the result of the comparison of the original and renamed string
            // later in the NM_CUSTOMDRAW handler, we use that information to draw items which stay the
            // same in the rename grayed out.
            lvi.lParam = it->first.compare(it->second);
            ListView_InsertItem(hListCtrl, &lvi);
            _tcscpy_s(textbuf, _countof(textbuf), it->second.c_str());
            ListView_SetItemText(hListCtrl, iItem, 1, textbuf);
            iItem++;
        }
        ListView_SetColumnWidth(hListCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(hListCtrl, 1, LVSCW_AUTOSIZE_USEHEADER);
    }
    catch (const std::exception&)
    {
        int iItem = 0;
        TCHAR textbuf[MAX_PATH] = {0};
        for (auto it = m_filelist.begin(); it != m_filelist.end(); ++it)
        {
            LVITEM lvi = {0};
            lvi.mask = LVIF_TEXT|LVIF_PARAM;
            _tcscpy_s(textbuf, _countof(textbuf), it->c_str());
            lvi.pszText = textbuf;
            lvi.iItem = iItem;
            // we use the lParam to store the result of the comparison of the original and renamed string
            // later in the NM_CUSTOMDRAW handler, we use that information to draw items which stay the
            // same in the rename grayed out.
            lvi.lParam = 0;
            ListView_InsertItem(hListCtrl, &lvi);
            ListView_SetItemText(hListCtrl, iItem, 1, textbuf);
            iItem++;
        }
        ListView_SetColumnWidth(hListCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(hListCtrl, 1, LVSCW_AUTOSIZE_USEHEADER);
    }
    SendMessage(hListCtrl, WM_SETREDRAW, TRUE, 0);
}

void CRenameDlg::SetFileList( const std::set<std::wstring>& list )
{
    m_filelist.clear();
    for (auto it = list.cbegin(); it != list.cend(); ++it)
    {
        m_filelist.insert(*it);
    }
}
