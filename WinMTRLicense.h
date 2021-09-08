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

//*****************************************************************************
// FILE:            WinMTRLicense.h
//
//
// DESCRIPTION:
//   
//
// NOTES:
//    
//
//*****************************************************************************

#ifndef WINMTRLICENSE_H_
#define WINMTRLICENSE_H_


#include "resource.h"

//*****************************************************************************
// CLASS:  WinMTRLicense
//
//
//*****************************************************************************

class WinMTRLicense : public CDialog
{
public:
	WinMTRLicense(CWnd* pParent = NULL);

	
	enum { IDD = IDD_DIALOG_LICENSE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	virtual BOOL OnInitDialog();
	
	DECLARE_MESSAGE_MAP()
};

#endif // ifndef WINMTRLICENSE_H_
