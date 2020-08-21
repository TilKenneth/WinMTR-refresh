//*****************************************************************************
// FILE:            WinMTRDialog.h
//
//
// DESCRIPTION:
//   
//
// NOTES:
//    
//
//*****************************************************************************

#ifndef WINMTRDIALOG_H_
#define WINMTRDIALOG_H_

#define WINMTR_DIALOG_TIMER 100

#include "WinMTRStatusBar.h"
#include "WinMTRNet.h"
#include <string>
#include <memory>
#include <mutex>
#include "resource.h"


//*****************************************************************************
// CLASS:  WinMTRDialog
//
//
//*****************************************************************************

class WinMTRDialog : public CDialog
{
public:
	WinMTRDialog(CWnd* pParent = NULL);

	enum { IDD = IDD_WINMTR_DIALOG };
	enum class options_source : bool {
		none,
		cmd_line
	};

	afx_msg BOOL InitRegistry();

	WinMTRStatusBar	statusBar;

	enum class STATES {
		IDLE,
		TRACING,
		STOPPING,
		EXIT
	};

	enum STATE_TRANSITIONS {
		IDLE_TO_IDLE,
		IDLE_TO_TRACING,
		IDLE_TO_EXIT,
		TRACING_TO_TRACING,
		TRACING_TO_STOPPING,
		TRACING_TO_EXIT,
		STOPPING_TO_IDLE,
		STOPPING_TO_STOPPING,
		STOPPING_TO_EXIT
	};

	CButton	m_buttonOptions;
	CButton	m_buttonExit;
	CButton	m_buttonStart;
	CComboBox m_comboHost;
	CListCtrl	m_listMTR;

	CStatic	m_staticS;
	CStatic	m_staticJ;

	CButton	m_buttonExpT;
	CButton	m_buttonExpH;
	
	int InitMTRNet();

	int DisplayRedraw();
	void Transit(STATES new_state);

	STATES				state;
	STATE_TRANSITIONS	transition;
	std::recursive_mutex			traceThreadMutex; 
	double				interval;
	bool				hasIntervalFromCmdLine;
	int					pingsize;
	bool				hasPingsizeFromCmdLine;
	int					maxLRU;
	bool				hasMaxLRUFromCmdLine;
	int					nrLRU;
	bool				useDNS;
	bool				hasUseDNSFromCmdLine;
	std::unique_ptr<WinMTRNet>			wmtrnet;

	void SetHostName(std::wstring host);
	void SetInterval(float i, options_source fromCmdLine = options_source::none) noexcept;
	void SetPingSize(int ps, options_source fromCmdLine = options_source::none) noexcept;
	void SetMaxLRU(int mlru, options_source fromCmdLine = options_source::none) noexcept;
	void SetUseDNS(bool udns, options_source fromCmdLine = options_source::none) noexcept;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	int m_autostart;
	std::wstring msz_defaulthostname;
	
	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT, int, int);
	afx_msg void OnSizing(UINT, LPRECT); 
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnRestart();
	afx_msg void OnOptions();
	virtual void OnCancel();

	afx_msg void OnCTTC();
	afx_msg void OnCHTC();
	afx_msg void OnEXPT();
	afx_msg void OnEXPH();

	afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
	
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnCbnSelchangeComboHost();
	afx_msg void OnCbnSelendokComboHost();
private:
	void ClearHistory();
public:
	afx_msg void OnCbnCloseupComboHost();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnBnClickedCancel();
};

#endif // ifndef WINMTRDIALOG_H_
