//	Copyright (C) 2014 Michael McMaster <michael@codesrc.com>
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
#ifndef TargetPanel_hh
#define TargetPanel_hh

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
#include <memory>

namespace SCSI2SD
{

// A parent class needs to call evaluate on all SCSI targets to sort
// out conflicting SCSI IDs and overlapping memory card use.
// Our custom event is fired whenever a new evaluation is required.
wxDECLARE_EVENT(ConfigChangedEvent, wxCommandEvent);

class TargetPanel : public wxPanel
{
public:
	TargetPanel(wxWindow* parent, const S2S_TargetCfg& initialConfig);

	S2S_TargetCfg getConfig() const;
	void setConfig(const S2S_TargetCfg& config);

	bool evaluate(); // Return true if current config is valid.

	bool isEnabled() const;
	uint8_t getSCSIId() const;
	std::pair<uint32_t, uint64_t> getSDSectorRange() const;

	// Error messages set by external validation
	void setDuplicateID(bool duplicate);
	void setSDSectorOverlap(bool overlap);
	void setAutoStartSector(uint32_t start);

private:
	template<typename EvtType> void onInput(EvtType& event);
	void onSizeInput(wxCommandEvent& event);
	void evaluateSize();

	std::pair<uint32_t, bool> convertUnitsToSectors() const;

	void initConfig();

	enum
	{
		ID_enableCtrl = wxID_HIGHEST + 1,
		ID_scsiIdCtrl,
		ID_deviceTypeCtrl,
		ID_startSDSectorCtrl,
		ID_autoStartSectorCtrl,
		ID_sectorSizeCtrl,
		ID_numSectorCtrl,
		ID_sizeCtrl,
		ID_sizeUnitCtrl,
		ID_vendorCtrl,
		ID_productCtrl,
		ID_revisionCtrl,
		ID_serialCtrl
	};

	enum // Must match the order given to the mySizeUnitCtrl ctor.
	{
		UNIT_KB,
		UNIT_MB,
		UNIT_GB
	};

	wxWindow* myParent;
	wxWindow* myChangedEventHandler;

	S2S_TargetCfg myConfig;
	uint32_t myAutoStartSector;

	wxCheckBox* myEnableCtrl;
	wxSpinCtrl* myScsiIdCtrl;
	wxStaticText* myScsiIdMsg;

	wxChoice* myDeviceTypeCtrl;

	std::unique_ptr<wxIntegerValidator<uint32_t>> myStartSDSectorValidator;
	wxTextCtrl* myStartSDSectorCtrl;
	wxCheckBox* myAutoStartSectorCtrl;
	wxStaticText* myStartSDSectorMsg;

	std::unique_ptr<wxIntegerValidator<uint16_t>> mySectorSizeValidator;
	wxTextCtrl* mySectorSizeCtrl;
	wxStaticText* mySectorSizeMsg;

	std::unique_ptr<wxIntegerValidator<uint32_t>> myNumSectorValidator;
	wxTextCtrl* myNumSectorCtrl;
	wxStaticText* myNumSectorMsg;

	std::unique_ptr<wxFloatingPointValidator<float>> mySizeValidator;
	wxTextCtrl* mySizeCtrl;
	wxChoice* mySizeUnitCtrl;

	wxTextCtrl* myVendorCtrl;
	wxStaticText* myVendorMsg;
	wxTextCtrl* myProductCtrl;
	wxStaticText* myProductMsg;
	wxTextCtrl* myRevisionCtrl;
	wxStaticText* myRevisionMsg;
	wxTextCtrl* mySerialCtrl;
	wxStaticText* mySerialMsg;
};

} // namespace SCSI2SD
#endif
