/*
** Taiga, a lightweight client for MyAnimeList
** Copyright (C) 2010, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../std.h"
#include "../animelist.h"
#include "dlg_anime_info.h"
#include "dlg_anime_info_page.h"
#include "../event.h"
#include "../gfx.h"
#include "../http.h"
#include "../myanimelist.h"
#include "../resource.h"
#include "../settings.h"
#include "../string.h"
#include "../taiga.h"
#include "../theme.h"

CAnimeWindow AnimeWindow;

// =============================================================================

CAnimeWindow::CAnimeWindow() :
  m_hbrDarkBlue(NULL), m_hbrLightBlue(NULL), 
  m_hfTitle(NULL), m_pAnimeItem(NULL),
  m_iCurrentPage(0)
{
  RegisterDlgClass(L"TaigaAnimeInfoW");
 
  m_Page.resize(TAB_COUNT);
  for (size_t i = 0; i < TAB_COUNT; i++) {
    m_Page[i].Index = i;
  }
}

CAnimeWindow::~CAnimeWindow() {
  if (m_hbrDarkBlue)  ::DeleteObject(m_hbrDarkBlue);
  if (m_hbrLightBlue) ::DeleteObject(m_hbrLightBlue);
  if (m_hfTitle)      ::DeleteObject(m_hfTitle);
}

// =============================================================================

BOOL CAnimeWindow::OnInitDialog() {
  // Set anime index
  if (!m_pAnimeItem) m_pAnimeItem = &AnimeList.Item[AnimeList.Index];

  // Create GDI objects
  if (!m_hbrDarkBlue) m_hbrDarkBlue = ::CreateSolidBrush(MAL_DARKBLUE);
  if (!m_hbrLightBlue) m_hbrLightBlue = ::CreateSolidBrush(MAL_LIGHTBLUE);
  if (!m_hfTitle) {
    LOGFONT lFont;
    ::GetObject(GetFont(), sizeof(LOGFONT), &lFont);
    CDC dc = GetDC();
    lFont.lfCharSet = DEFAULT_CHARSET;
    lFont.lfHeight = -MulDiv(12, GetDeviceCaps(dc.Get(), LOGPIXELSY), 72);
    lFont.lfWeight = FW_BOLD;
    m_hfTitle = ::CreateFontIndirect(&lFont);
  }
  SendDlgItemMessage(IDC_STATIC_ANIME_TITLE, WM_SETFONT, reinterpret_cast<WPARAM>(m_hfTitle), FALSE);

  // Create tabs
  m_Tab.Attach(GetDlgItem(IDC_TAB_ANIME));
  m_Tab.InsertItem(0, L"Series information", NULL);
  if (m_pAnimeItem->Index > -1) {
    m_Tab.InsertItem(1, L"My information", NULL);
  }

  // Set image position
  if (AnimeImage.Rect.IsEmpty()) {
    CRect rcTemp;
    ::GetWindowRect(GetDlgItem(IDC_STATIC_ANIME_IMG), &rcTemp);
    ::GetWindowRect(GetDlgItem(IDC_STATIC_ANIME_IMG), &AnimeImage.Rect);
    ::ScreenToClient(m_hWindow, reinterpret_cast<LPPOINT>(&rcTemp));
    AnimeImage.Rect.Offset(rcTemp.left - AnimeImage.Rect.left, rcTemp.top - AnimeImage.Rect.top);
  }

  // Create pages
  CRect rcPage;
  m_Tab.GetWindowRect(&rcPage);
  m_Tab.AdjustRect(FALSE, &rcPage);
  ::ScreenToClient(m_hWindow, reinterpret_cast<LPPOINT>(&rcPage));
  for (size_t i = 0; i < TAB_COUNT; i++) {
    m_Page[i].Create(IDD_ANIME_INFO_PAGE01 + i, m_hWindow, false);
    m_Page[i].SetPosition(NULL, rcPage.left, rcPage.top, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE);
    EnableThemeDialogTexture(m_Page[i].GetWindowHandle(), ETDT_ENABLETAB);
  }

  // Refresh
  SetCurrentPage(m_iCurrentPage);
  Refresh(m_pAnimeItem);
  return TRUE;
}

void CAnimeWindow::OnOK() {
  if (!m_pAnimeItem || m_pAnimeItem->Index == -1) {
    EndDialog(IDOK);
    return;
  }

  // Episodes watched
  int episode = m_Page[TAB_MYINFO].GetDlgItemInt(IDC_EDIT_ANIME_PROGRESS);
  if (!MAL.IsValidEpisode(episode, 0, m_pAnimeItem->Series_Episodes)) {
    wstring msg = L"Please enter a valid episode number between 0-" + 
      ToWSTR(m_pAnimeItem->Series_Episodes) + L".";
    MessageBox(msg.c_str(), L"Episodes watched", MB_OK | MB_ICONERROR);
    return;
  }
  
  // Score
  int score = 10 - m_Page[TAB_MYINFO].GetComboSelection(IDC_COMBO_ANIME_SCORE);
  
  // Status
  int status = m_Page[TAB_MYINFO].GetComboSelection(IDC_COMBO_ANIME_STATUS) + 1;
  if (status == MAL_UNKNOWN) status++;
  
  // Tags
  wstring tags;
  m_Page[TAB_MYINFO].GetDlgItemText(IDC_EDIT_ANIME_TAGS, tags);

  // Start date
  SYSTEMTIME stMyStart;
  if (m_Page[TAB_MYINFO].SendDlgItemMessage(IDC_DATETIME_START, DTM_GETSYSTEMTIME, 0, 
    reinterpret_cast<LPARAM>(&stMyStart)) == GDT_NONE) {
      m_pAnimeItem->My_StartDate = L"0000-00-00";
  } else {
    wstring year = ToWSTR(stMyStart.wYear);
    wstring month = (stMyStart.wMonth < 10 ? L"0" : L"") + ToWSTR(stMyStart.wMonth);
    wstring day = (stMyStart.wDay < 10 ? L"0" : L"") + ToWSTR(stMyStart.wDay);
    m_pAnimeItem->SetStartDate(year + L"-" + month + L"-" + day, true);
  }
  // Finish date
  SYSTEMTIME stMyFinish;
  if (m_Page[TAB_MYINFO].SendDlgItemMessage(IDC_DATETIME_FINISH, DTM_GETSYSTEMTIME, 0, 
    reinterpret_cast<LPARAM>(&stMyFinish)) == GDT_NONE) {
      m_pAnimeItem->My_FinishDate = L"0000-00-00";
  } else {
    wstring year = ToWSTR(stMyFinish.wYear);
    wstring month = (stMyFinish.wMonth < 10 ? L"0" : L"") + ToWSTR(stMyFinish.wMonth);
    wstring day = (stMyFinish.wDay < 10 ? L"0" : L"") + ToWSTR(stMyFinish.wDay);
    m_pAnimeItem->SetFinishDate(year + L"-" + month + L"-" + day, true);
  }

  // Alternative titles
  wstring titles;
  m_Page[TAB_MYINFO].GetDlgItemText(IDC_EDIT_ANIME_ALT, titles);
  m_pAnimeItem->Synonyms = titles;
  Settings.Anime.SetItem(m_pAnimeItem->Series_ID, EMPTY_STR, titles);
  if (CurrentEpisode.Index == -1) CurrentEpisode.Index = 0;
  
  // Add to buffer
  EventQueue.Add(L"", m_pAnimeItem->Index, m_pAnimeItem->Series_ID, 
    episode, score, status, tags, L"", HTTP_MAL_AnimeEdit);

  // Exit
  EndDialog(IDOK);
}

// =============================================================================

BOOL CAnimeWindow::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_CTLCOLORSTATIC: {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      HWND hwnd_static = reinterpret_cast<HWND>(lParam);
      if (hwnd_static == GetDlgItem(IDC_STATIC_ANIME_TITLE)) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, MAL_DARKBLUE);
        return reinterpret_cast<INT_PTR>(m_hbrLightBlue);
      }
      break;
    }

    // Drag window
    case WM_ENTERSIZEMOVE: {
      if (::IsAppThemed() && GetWinVersion() >= WINVERSION_VISTA) {
        SetTransparency(200);
      }
      break;
    }
    case WM_EXITSIZEMOVE: {
      if (::IsAppThemed() && GetWinVersion() >= WINVERSION_VISTA) {
        SetTransparency(255);
      }
      break;
    }    
    case WM_LBUTTONDOWN: {
      SetCursor(reinterpret_cast<HCURSOR>(LoadImage(NULL, IDC_SIZEALL, IMAGE_CURSOR, 0, 0, LR_SHARED)));
      ReleaseCapture();
      SendMessage(WM_NCLBUTTONDOWN, HTCAPTION, NULL);
      break;
    }

    // Close window
    case WM_RBUTTONUP: {
      Destroy();
      return TRUE;
    }
  }
  
  return DialogProcDefault(hwnd, uMsg, wParam, lParam);
}

LRESULT CAnimeWindow::OnNotify(int idCtrl, LPNMHDR pnmh) {
  if (idCtrl == IDC_TAB_ANIME) {
    switch (pnmh->code) {
      // Tab select
      case TCN_SELCHANGE: {
        SetCurrentPage(m_Tab.GetCurrentlySelected());
        break;
      }
    }
  }
  
  return 0;
}

BOOL CAnimeWindow::PreTranslateMessage(MSG* pMsg) {
  if (pMsg->message == WM_KEYDOWN) {
    // Close window
    if (pMsg->wParam == VK_ESCAPE) {
      Destroy();
      return TRUE;
    }
  }
  return FALSE;
}

// =============================================================================

void CAnimeWindow::SetCurrentPage(int index) {
  m_iCurrentPage = index;
  if (IsWindow()) {
    for (int i = 0; i < TAB_COUNT; i++) {
      if (i != index) m_Page[i].Show(SW_HIDE);
    }
    m_Page[index].Show();
    m_Tab.SetCurrentlySelected(index);
  }
}

void CAnimeWindow::Refresh(CAnime* pAnimeItem, bool series_info, bool my_info) {
  // Set anime index
  if (pAnimeItem) m_pAnimeItem = pAnimeItem;
  if (!m_pAnimeItem || !IsWindow()) return;

  // Set title
  wstring text = L"  " + m_pAnimeItem->Series_Title;
  SetDlgItemText(IDC_STATIC_ANIME_TITLE, text.c_str());

  // Load image
  if (AnimeImage.Load(Taiga.GetDataPath() + L"Image\\" + ToWSTR(m_pAnimeItem->Series_ID) + L".jpg")) {
    CWindow img = GetDlgItem(IDC_STATIC_ANIME_IMG);
    img.SetPosition(NULL, AnimeImage.Rect);
    img.SetWindowHandle(NULL);
  } else {
    MAL.DownloadImage(m_pAnimeItem);
  }
  InvalidateRect(&AnimeImage.Rect);

  // Refresh pages
  if (series_info) {
    m_Page[TAB_SERIESINFO].Refresh(m_pAnimeItem);
  }
  if (my_info) {
    m_Page[TAB_MYINFO].Refresh(m_pAnimeItem);
  }
}

// =============================================================================

bool CAnimeWindow::CAnimeImage::Load(const wstring& file) {
  ::DeleteObject(DC.DetachBitmap());
  
  HBITMAP hbmp = NULL;
  HDC hScreen = ::GetDC(NULL);
  DC = ::CreateCompatibleDC(hScreen);
  ::ReleaseDC(NULL, hScreen);

  Gdiplus::Bitmap bmp(file.c_str());
  Width = bmp.GetWidth();
  Height = bmp.GetHeight();
  bmp.GetHBITMAP(NULL, &hbmp);
  
  if (!hbmp || !Width || !Height) {
    ::DeleteObject(hbmp);
    return false;
  } else {
    Rect.bottom = Rect.top + static_cast<int>(Rect.Width() * 
      (static_cast<float>(Height) / static_cast<float>(Width)));
    DC.AttachBitmap(hbmp);
    return true;
  }
}

void CAnimeWindow::OnPaint(HDC hdc, LPPAINTSTRUCT lpps) {
  CDC dc = hdc;
  CRect rect;

  // Paint background
  GetClientRect(&rect);
  rect.top = AnimeImage.Rect.top + 20;
  dc.FillRect(rect, RGB(255, 255, 255));
  rect.bottom = rect.top + 200;
  GradientRect(dc.Get(), &rect, ::GetSysColor(COLOR_BTNFACE), RGB(255, 255, 255), true);

  // Paint image
  rect.Copy(AnimeImage.Rect);
  rect.right -= 3; rect.bottom -= 3;
  dc.FillRect(rect, RGB(255, 255, 255));
  dc.SetStretchBltMode(HALFTONE);
  dc.StretchBlt(rect.left, rect.top, rect.Width(), rect.Height(), 
    AnimeImage.DC.Get(), 0, 0, AnimeImage.Width, AnimeImage.Height, SRCCOPY);
}