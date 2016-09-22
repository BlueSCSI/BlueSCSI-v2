//	Copyright (C) 2015 Michael McMaster <michael@codesrc.com>
//
//	This file is part of SCSI2SD.
//
//	SCSI2SD is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	SCSI2SD is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with SCSI2SD.  If not, see <http://www.gnu.org/licenses/>.
#ifndef BoardPanel_hh
#define BoardPanel_hh

#include "scsi2sd.h"


#include <wx/wx.h>
#include <wx/valnum.h>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

#if __cplusplus >= 201103L
#include <cstdint>
#else
#include <stdint.h>
#endif

#include <utility>

namespace SCSI2SD
{

class BoardPanel : public wxPanel
{
public:
	BoardPanel(wxWindow* parent, const S2S_BoardCfg& initialConfig);

	S2S_BoardCfg getConfig() const;
	void setConfig(const S2S_BoardCfg& config);

private:
	void initConfig();

	enum
	{
		ID_parityCtrl = wxID_HIGHEST + 1,
		ID_unitAttCtrl,
		ID_scsi2Ctrl,
		ID_glitchCtrl,
		ID_termCtrl,
		ID_cacheCtrl,
		ID_disconnectCtrl,
		ID_selLatchCtrl,
		ID_mapLunsCtrl,
		ID_startDelayCtrl,
		ID_selDelayCtrl
	};

	wxWindow* myParent;

	S2S_BoardCfg myConfig;

	wxCheckBox* myParityCtrl;
	wxCheckBox* myUnitAttCtrl;
	wxCheckBox* myScsi2Ctrl;
	wxCheckBox* myGlitchCtrl;
	wxCheckBox* myTermCtrl;
	wxCheckBox* myCacheCtrl;
	wxCheckBox* myDisconnectCtrl;
	wxCheckBox* mySelLatchCtrl;
	wxCheckBox* myMapLunsCtrl;

	wxIntegerValidator<uint8_t>* myDelayValidator;
	wxTextCtrl* myStartDelayCtrl;
	wxTextCtrl* mySelDelayCtrl;
};

} // namespace SCSI2SD
#endif
