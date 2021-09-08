/*
WinMTR
Copyright (C)  2010-2019 Appnor MSP S.A. - http://www.appnor.com
Copyright (C) 2019-2021 Leetsoftwerx

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/



#include "WinMTRGlobal.h"
#include "WinMTRStatusBar.h"
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


namespace {
	class deferWindowPos {
		HDWP m_hdwp;
	public:
		deferWindowPos(int nNumWindows) noexcept
		:m_hdwp(::BeginDeferWindowPos(nNumWindows)){
		}

		~deferWindowPos() noexcept
		{
			VERIFY(EndDeferWindowPos(m_hdwp));
		}

		void defer(_In_ HWND hWnd,
			_In_opt_ HWND hWndInsertAfter,
			_In_ int x,
			_In_ int y,
			_In_ int cx,
			_In_ int cy,
			_In_ UINT uFlags) noexcept {
			m_hdwp = DeferWindowPos(m_hdwp, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
		}
	};
}


BEGIN_MESSAGE_MAP(WinMTRStatusBar, CStatusBar)
	//{{AFX_MSG_MAP(WinMTRStatusBar)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// WinMTRStatusBar message handlers
//////////////////////////////////////////////////////////////////////////

int WinMTRStatusBar::OnCreate(LPCREATESTRUCT lpCreateStruct) noexcept
{
	if( CStatusBar::OnCreate(lpCreateStruct) == -1 )
		return -1;
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////

LRESULT WinMTRStatusBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
	LRESULT lResult =CStatusBar::WindowProc(message, wParam, lParam);
	if( message == WM_SIZE ){
		RepositionControls();
	}
	return lResult;
}

//////////////////////////////////////////////////////////////////////////

void WinMTRStatusBar::RepositionControls() noexcept
{
	deferWindowPos dwp(gsl::narrow_cast<int>(m_arrPaneControls.size()));
	
	CRect rcClient;
	GetClientRect(&rcClient);
	for (const auto & cntl : m_arrPaneControls)
	{
		int   iIndex  = CommandToIndex(cntl.nID);
		HWND hWnd    = cntl.hWnd;
		const auto dpi = GetDpiForWindow(hWnd);
		CRect rcPane;
		GetItemRect(iIndex, &rcPane);
		
		// CStatusBar::GetItemRect() sometimes returns invalid size 
		// of the last pane - we will re-compute it
		int cx = ::GetSystemMetricsForDpi( SM_CXEDGE, dpi );
		DWORD dwPaneStyle = GetPaneStyle( iIndex );
		if( iIndex == (m_nCount-1) )
		{
			if( (dwPaneStyle & SBPS_STRETCH ) == 0 )
			{
				UINT nID, nStyle;
				int  cxWidth;
				GetPaneInfo( iIndex, nID, nStyle, cxWidth );
				rcPane.right = rcPane.left + cxWidth + cx*3;
			} // if( (dwPaneStyle & SBPS_STRETCH ) == 0 )
			else
			{
				CRect rcClient;
				GetClientRect( &rcClient );
				rcPane.right = rcClient.right;
				if( (GetStyle() & SBARS_SIZEGRIP) == SBARS_SIZEGRIP )
				{
					int cxSmIcon = ::GetSystemMetricsForDpi( SM_CXSMICON, dpi);
					rcPane.right -= cxSmIcon + cx;
				} // if( (GetStyle() & SBARS_SIZEGRIP) == SBARS_SIZEGRIP )
			} // else from if( (dwPaneStyle & SBPS_STRETCH ) == 0 )
		} // if( iIndex == (m_nCount-1) )
		
		if ((GetPaneStyle (iIndex) & SBPS_NOBORDERS) == 0){
			rcPane.DeflateRect(cx,cx);
		}else{
			rcPane.DeflateRect(cx,1,cx,1);
		}
		
		if (hWnd && ::IsWindow(hWnd)){
			dwp.defer( 
				hWnd, 
				nullptr, 
				rcPane.left,
				rcPane.top, 
				rcPane.Width(), 
				rcPane.Height(),
				SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_SHOWWINDOW
				);

			::RedrawWindow(
				hWnd,
				nullptr,
				nullptr,
				RDW_INVALIDATE|RDW_UPDATENOW
				|RDW_ERASE|RDW_ERASENOW
				);
			
		} // if (hWnd && ::IsWindow(hWnd)){ 
	}
};

//////////////////////////////////////////////////////////////////////////

BOOL WinMTRStatusBar::AddPane(
	 UINT nID,	// ID of the  pane
	 int nIndex	// index of the pane
	 )
{
	if (nIndex < 0 || nIndex > m_nCount){
		ASSERT(FALSE);
		return FALSE;
	}
	
	if (CommandToIndex(nID) != -1){
		ASSERT(FALSE);
		return FALSE;
	}
	
	std::vector<_STATUSBAR_PANE_> arrPanesTmp;
	arrPanesTmp.reserve(static_cast<size_t>(m_nCount) + 1);
	for (int iIndex = 0; iIndex < m_nCount+1; iIndex++)
	{
		_STATUSBAR_PANE_ pNewPane;
		
		if (iIndex == nIndex){
			pNewPane.nID    = nID;
			pNewPane.nStyle = SBPS_NORMAL;
		}else{
			int idx = iIndex;
			if (iIndex > nIndex) idx--;
			
			_STATUSBAR_PANE_* pOldPane  = GetPanePtr(idx);
			pNewPane = *pOldPane;
		}
		arrPanesTmp.emplace_back(std::move(pNewPane));
	}

	std::vector<UINT> IDArray;
	IDArray.reserve(arrPanesTmp.size());
	for (const auto& pane : arrPanesTmp) {
		IDArray.emplace_back(pane.nID);
	}
	
	// set the indicators 
	SetIndicators(IDArray);
	for (int iIndex = 0; const auto & pane : arrPanesTmp){
		if (iIndex != nIndex) {
			PaneInfoSet(iIndex, pane);
		}
			
		++iIndex;
	}
	
	
	RepositionControls();
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

BOOL WinMTRStatusBar::RemovePane(
	UINT nID	// ID of the pane
	)
{
	if ( CommandToIndex(nID) == -1 || m_nCount == 1 ){
		ASSERT(FALSE);
		return FALSE;
	}
	
	std::vector<_STATUSBAR_PANE_> arrPanesTmp;
	arrPanesTmp.reserve(m_nCount);
	for (int nIndex = 0; nIndex < m_nCount; nIndex++)
	{
		_STATUSBAR_PANE_* pOldPane = GetPanePtr(nIndex);
		
		if (pOldPane->nID == nID)
			continue;
		
		_STATUSBAR_PANE_ pNewPane = *pOldPane;
		arrPanesTmp.emplace_back(std::move(pNewPane));
	}
	std::vector<UINT> IDArray;
	IDArray.reserve(arrPanesTmp.size());
	for (const auto &pane : arrPanesTmp) {
		IDArray.emplace_back(pane.nID);
	}
	
	// set the indicators
	SetIndicators(IDArray);
	// free memory
	
	for (int nIndex = 0; const auto& pane : arrPanesTmp) {
		this->PaneInfoSet(nIndex, pane);
		++nIndex;
	}
	m_arrPaneControls.erase(std::remove_if(m_arrPaneControls.begin(), m_arrPaneControls.end(), [nID](const auto& cntl) {
		return cntl.nID == nID;
		}),
		m_arrPaneControls.end());
	
	RepositionControls();
	
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

BOOL WinMTRStatusBar::AddPaneControl(HWND hWnd, UINT nID, BOOL bAutoDestroy)
{
	if (CommandToIndex (nID) == -1) {
		return FALSE;
	}
	
	m_arrPaneControls.emplace_back(hWnd, nID, bAutoDestroy);

	RepositionControls();
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////

bool WinMTRStatusBar::PaneInfoGet(int nIndex, _STATUSBAR_PANE_* pPane)
{
	if( nIndex < m_nCount  && nIndex >= 0 )
	{
		GetPaneInfo( nIndex,  pPane->nID, pPane->nStyle, pPane->cxText );
		CString strPaneText;
		GetPaneText( nIndex , strPaneText );
		pPane->strText = LPCTSTR(strPaneText);
		return true;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool WinMTRStatusBar::PaneInfoSet(int nIndex, const _STATUSBAR_PANE_& pPane)
{
	if( nIndex < m_nCount  && nIndex >= 0 ){
		SetPaneInfo( nIndex, pPane.nID, pPane.nStyle, pPane.cxText );
		SetPaneText( nIndex, static_cast<LPCTSTR>( pPane.strText) );
		return true;
	}
	return false;
}

WinMTRStatusBar::_STATUSBAR_PANE_CTRL_::_STATUSBAR_PANE_CTRL_()
	:hWnd(nullptr),
	nID(0),
	bAutoDestroy(false)
{
}

WinMTRStatusBar::_STATUSBAR_PANE_CTRL_::~_STATUSBAR_PANE_CTRL_() noexcept
{
	if (this->hWnd && ::IsWindow(this->hWnd)) {
		::ShowWindow(this->hWnd, SW_HIDE);
		if (this->bAutoDestroy) {
			::DestroyWindow(this->hWnd);
		}
	}
}
