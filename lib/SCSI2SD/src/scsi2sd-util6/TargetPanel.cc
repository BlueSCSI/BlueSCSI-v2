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

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/wrapsizer.h>

#include "ConfigUtil.hh"
#include "TargetPanel.hh"

#include <limits>
#include <sstream>

#include <math.h>
#include <string.h>

using namespace SCSI2SD;

wxDEFINE_EVENT(SCSI2SD::ConfigChangedEvent, wxCommandEvent);

namespace
{
	template<typename IntType, class WXCTRL> std::pair<IntType, bool>
	CtrlGetValue(WXCTRL* ctrl)
	{
		IntType value;
		std::stringstream conv;
		conv << ctrl->GetValue();
		conv >> value;
		return std::make_pair(value, static_cast<bool>(conv));
	}

	void CtrlGetFixedString(wxTextEntry* ctrl, char* dest, size_t len)
	{
		memset(dest, ' ', len);
		std::string str(ctrl->GetValue().ToAscii());
		// Don't use strncpy - we need to avoid NULL's
		memcpy(dest, str.c_str(), std::min(len, str.size()));
	}

	bool CtrlIsAscii(wxTextEntry* ctrl)
	{
		return ctrl->GetValue().IsAscii();
	}

}

TargetPanel::TargetPanel(wxWindow* parent, const S2S_TargetCfg& initialConfig) :
	wxPanel(parent),
	myParent(parent),
	myAutoStartSector(0),
	myStartSDSectorValidator(new wxIntegerValidator<uint32_t>),
	mySectorSizeValidator(new wxIntegerValidator<uint16_t>),
	myNumSectorValidator(new wxIntegerValidator<uint32_t>),
	mySizeValidator(new wxFloatingPointValidator<float>(2))
{
	wxFlexGridSizer *fgs = new wxFlexGridSizer(11, 3, 9, 25);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myEnableCtrl =
		new wxCheckBox(
			this,
			ID_enableCtrl,
			_("Enable SCSI Target"));
	fgs->Add(myEnableCtrl);
	// Set a non-visible string to leave room in the column for future messages
	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("                                        ")));
	Bind(wxEVT_CHECKBOX, &TargetPanel::onInput<wxCommandEvent>, this, ID_enableCtrl);


	fgs->Add(new wxStaticText(this, wxID_ANY, _("SCSI ID")));
	myScsiIdCtrl =
		new wxSpinCtrl
			(this,
			ID_scsiIdCtrl,
			wxEmptyString,
			wxDefaultPosition,
			wxDefaultSize,
			wxSP_WRAP | wxSP_ARROW_KEYS,
			0,
			7,
			0);
	fgs->Add(myScsiIdCtrl);
	myScsiIdMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(myScsiIdMsg);
	Bind(wxEVT_SPINCTRL, &TargetPanel::onInput<wxSpinEvent>, this, ID_scsiIdCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, _("Device Type")));
	wxString deviceTypes[] =
	{
		_("Hard Drive"),
		_("Removable"),
		_("CDROM"),
		_("3.5\" Floppy"),
		_("Magneto optical")
	};
	myDeviceTypeCtrl =
		new wxChoice(
			this,
			ID_deviceTypeCtrl,
			wxDefaultPosition,
			wxDefaultSize,
			sizeof(deviceTypes) / sizeof(wxString),
			deviceTypes
			);
	myDeviceTypeCtrl->SetSelection(0);
	fgs->Add(myDeviceTypeCtrl);
	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	Bind(wxEVT_CHOICE, &TargetPanel::onInput<wxCommandEvent>, this, ID_deviceTypeCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("SD card start sector")));
	wxWrapSizer* startContainer = new wxWrapSizer();
	myStartSDSectorCtrl =
		new wxTextCtrl(
			this,
			ID_startSDSectorCtrl,
			"0",
			wxDefaultPosition,
			wxDefaultSize,
			0,
			*myStartSDSectorValidator);
	myStartSDSectorCtrl->SetToolTip(wxT("Supports multiple SCSI targets "
		"on a single memory card. In units of 512-byte sectors."));
	startContainer->Add(myStartSDSectorCtrl);
	myAutoStartSectorCtrl =
		new wxCheckBox(
			this,
			ID_autoStartSectorCtrl,
			wxT("Auto"));
	startContainer->Add(myAutoStartSectorCtrl);
	Bind(wxEVT_CHECKBOX, &TargetPanel::onInput<wxCommandEvent>, this, ID_autoStartSectorCtrl);
	fgs->Add(startContainer);
	myStartSDSectorMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(myStartSDSectorMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onInput<wxCommandEvent>, this, ID_startSDSectorCtrl);


	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Sector size (bytes)")));
	mySectorSizeCtrl =
		new wxTextCtrl(
			this,
			ID_sectorSizeCtrl,
			"512",
			wxDefaultPosition,
			wxDefaultSize,
			0,
			*mySectorSizeValidator);
	mySectorSizeCtrl->SetToolTip(wxT("Between 64 and 8192. Default of 512 is suitable in most cases."));
	fgs->Add(mySectorSizeCtrl);
	mySectorSizeMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(mySectorSizeMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onSizeInput, this, ID_sectorSizeCtrl);


	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Sector count")));
	myNumSectorCtrl =
		new wxTextCtrl(
			this,
			ID_numSectorCtrl,
			"",
			wxDefaultPosition,
			wxDefaultSize,
			0,
			*myNumSectorValidator);
	myNumSectorCtrl->SetToolTip(wxT("Number of sectors (device size)"));
	fgs->Add(myNumSectorCtrl);
	myNumSectorMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(myNumSectorMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onSizeInput, this, ID_numSectorCtrl);


	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Device size")));
	wxWrapSizer* sizeContainer = new wxWrapSizer();
	mySizeCtrl =
		new wxTextCtrl(
			this,
			ID_sizeCtrl,
			"",
			wxDefaultPosition,
			wxDefaultSize,
			0,
			*mySizeValidator);
	mySizeCtrl->SetToolTip(wxT("Device size"));
	sizeContainer->Add(mySizeCtrl);
	wxString units[] = {wxT("KB"), wxT("MB"), wxT("GB")};
	mySizeUnitCtrl =
		new wxChoice(
			this,
			ID_sizeUnitCtrl,
			wxDefaultPosition,
			wxDefaultSize,
			sizeof(units) / sizeof(wxString),
			units
			);
	sizeContainer->Add(mySizeUnitCtrl);
	fgs->Add(sizeContainer);
	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	Bind(wxEVT_TEXT, &TargetPanel::onSizeInput, this, ID_sizeCtrl);
	Bind(wxEVT_CHOICE, &TargetPanel::onSizeInput, this, ID_sizeUnitCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Vendor")));
	myVendorCtrl =
		new wxTextCtrl(
			this,
			ID_vendorCtrl,
			wxEmptyString,
			wxDefaultPosition,
			wxSize(GetCharWidth() * 10, -1));
	myVendorCtrl->SetMaxLength(8);
	myVendorCtrl->SetToolTip(wxT("SCSI Vendor string. eg. ' codesrc'"));
	fgs->Add(myVendorCtrl);
	myVendorMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(myVendorMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onInput<wxCommandEvent>, this, ID_vendorCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Product ID")));
	myProductCtrl =
		new wxTextCtrl(
			this,
			ID_productCtrl,
			wxEmptyString,
			wxDefaultPosition,
			wxSize(GetCharWidth() * 17, -1));
	myProductCtrl->SetMaxLength(18);
	myProductCtrl->SetToolTip(wxT("SCSI Product ID string. eg. 'SCSI2SD'"));
	fgs->Add(myProductCtrl);
	myProductMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(myProductMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onInput<wxCommandEvent>, this, ID_productCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Revision")));
	myRevisionCtrl =
		new wxTextCtrl(
			this,
			ID_revisionCtrl,
			wxEmptyString,
			wxDefaultPosition,
			wxSize(GetCharWidth() * 6, -1));
	myRevisionCtrl->SetMaxLength(4);
	myRevisionCtrl->SetToolTip(wxT("SCSI device revision string. eg. '3.5a'"));
	fgs->Add(myRevisionCtrl);
	myRevisionMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(myRevisionMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onInput<wxCommandEvent>, this, ID_revisionCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("Serial number")));
	mySerialCtrl =
		new wxTextCtrl(
			this,
			ID_serialCtrl,
			wxEmptyString,
			wxDefaultPosition,
			wxSize(GetCharWidth() * 18, -1));
	mySerialCtrl->SetMaxLength(16);
	mySerialCtrl->SetToolTip(wxT("SCSI serial number. eg. '13eab5632a'"));
	fgs->Add(mySerialCtrl);
	mySerialMsg = new wxStaticText(this, wxID_ANY, wxT(""));
	fgs->Add(mySerialMsg);
	Bind(wxEVT_TEXT, &TargetPanel::onInput<wxCommandEvent>, this, ID_serialCtrl);

	wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(fgs, 1, wxALL | wxEXPAND, 15);
	this->SetSizer(hbox);
	Centre();


	setConfig(initialConfig);
	evaluate();
}

bool
TargetPanel::evaluate()
{
	bool valid = true;
	std::stringstream conv;

	bool enabled = myEnableCtrl->IsChecked();
	{
		myScsiIdCtrl->Enable(enabled);
		myDeviceTypeCtrl->Enable(enabled);
		myStartSDSectorCtrl->Enable(enabled && !myAutoStartSectorCtrl->IsChecked());
		myAutoStartSectorCtrl->Enable(enabled);
		mySectorSizeCtrl->Enable(enabled);
		myNumSectorCtrl->Enable(enabled);
		mySizeCtrl->Enable(enabled);
		mySizeUnitCtrl->Enable(enabled);
		myVendorCtrl->Enable(enabled);
		myProductCtrl->Enable(enabled);
		myRevisionCtrl->Enable(enabled);
		mySerialCtrl->Enable(enabled);
	}

	switch (myDeviceTypeCtrl->GetSelection())
	{
	case S2S_CFG_FLOPPY_14MB:
		mySectorSizeCtrl->ChangeValue("512");
		mySectorSizeCtrl->Enable(false);
		myNumSectorCtrl->ChangeValue("2880");
		myNumSectorCtrl->Enable(false);
		mySizeUnitCtrl->Enable(false);
		mySizeCtrl->Enable(false);
		evaluateSize();
		break;
	};

	if (myAutoStartSectorCtrl->IsChecked())
	{
		std::stringstream ss; ss << myAutoStartSector;
		myStartSDSectorCtrl->ChangeValue(ss.str());
	}

	uint32_t startSDsector;
	{
		conv << myStartSDSectorCtrl->GetValue();
		conv >> startSDsector;
	}

	if (!conv)
		// TODO check if it is beyond the current SD card.
	{
		myStartSDSectorMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Number &gt;= 0</span>"));
		valid = false;
	}
	else
	{
		myStartSDSectorMsg->SetLabelMarkup("");
	}
	conv.str(std::string()); conv.clear();

	uint16_t sectorSize(CtrlGetValue<uint16_t>(mySectorSizeCtrl).first);
	if (sectorSize < 64 || sectorSize > 8192)
	{
		mySectorSizeMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Must be between 64 and 8192</span>"));
		valid = false;
	}
	else
	{
		mySectorSizeMsg->SetLabelMarkup("");
	}
	conv.str(std::string()); conv.clear();

	std::pair<uint32_t, bool> numSectors(CtrlGetValue<uint32_t>(myNumSectorCtrl));
	if (!numSectors.second ||
		numSectors.first == 0 ||
		!convertUnitsToSectors().second)
	{
		myNumSectorMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Invalid size</span>"));
		valid = false;
	}
	else
	{
		myNumSectorMsg->SetLabelMarkup("");
	}

	if (!CtrlIsAscii(myVendorCtrl))
	{
		myVendorMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Invalid characters</span>"));
		valid = false;
	}
	else
	{
		myVendorMsg->SetLabelMarkup("");
	}

	if (!CtrlIsAscii(myProductCtrl))
	{
		myProductMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Invalid characters</span>"));
		valid = false;
	}
	else
	{
		myProductMsg->SetLabelMarkup("");
	}

	if (!CtrlIsAscii(myRevisionCtrl))
	{
		myRevisionMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Invalid characters</span>"));
		valid = false;
	}
	else
	{
		myRevisionMsg->SetLabelMarkup("");
	}

	if (!CtrlIsAscii(mySerialCtrl))
	{
		mySerialMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Invalid characters</span>"));
		valid = false;
	}
	else
	{
		mySerialMsg->SetLabelMarkup("");
	}

	return valid || !enabled;
}

template<typename EvtType> void
TargetPanel::onInput(EvtType& event)
{
	if (event.GetId() == ID_deviceTypeCtrl)
	{
		switch (myDeviceTypeCtrl->GetSelection())
		{
		case S2S_CFG_OPTICAL:
			mySectorSizeCtrl->ChangeValue("2048");
			evaluateSize();
			break;
		}
	}
	wxCommandEvent changeEvent(ConfigChangedEvent);
	wxPostEvent(myParent, changeEvent);
}

void
TargetPanel::onSizeInput(wxCommandEvent& event)
{
	if (event.GetId() != ID_numSectorCtrl)
	{
		std::pair<uint32_t, bool> sec = convertUnitsToSectors();
		if (sec.second)
		{
			std::stringstream ss;
			ss << sec.first;
			myNumSectorCtrl->ChangeValue(ss.str());
		}
	}
	if (event.GetId() != ID_sizeCtrl)
	{
		evaluateSize();
	}
	onInput(event); // propagate
}

void
TargetPanel::evaluateSize()
{
	uint32_t numSectors;
	std::stringstream conv;
	conv << myNumSectorCtrl->GetValue();
	conv >> numSectors;

	conv.str(""); conv.clear();

	if (conv)
	{
		uint64_t bytes =
			uint64_t(numSectors) *
				CtrlGetValue<uint16_t>(mySectorSizeCtrl).first;

		if (bytes >= 1024 * 1024 * 1024)
		{
			conv << (bytes / (1024.0 * 1024 * 1024));
			mySizeUnitCtrl->SetSelection(UNIT_GB);
		}
		else if (bytes >= 1024 * 1024)
		{
			conv << (bytes / (1024.0 * 1024));
			mySizeUnitCtrl->SetSelection(UNIT_MB);
		}
		else
		{
			conv << (bytes / 1024.0);
			mySizeUnitCtrl->SetSelection(UNIT_KB);
		}
		mySizeCtrl->ChangeValue(conv.str());
	}
}

std::pair<uint32_t, bool>
TargetPanel::convertUnitsToSectors() const
{
	bool valid = true;

	uint64_t multiplier(0);
	switch (mySizeUnitCtrl->GetSelection())
	{
		case UNIT_KB: multiplier = 1024; break;
		case UNIT_MB: multiplier = 1024 * 1024; break;
		case UNIT_GB: multiplier = 1024 * 1024 * 1024; break;
		default: valid = false;
	}

	double size;
	std::stringstream conv;
	conv << mySizeCtrl->GetValue();
	conv >> size;
	valid = valid && conv;

	uint16_t sectorSize = CtrlGetValue<uint16_t>(mySectorSizeCtrl).first;
	uint64_t sectors = ceil(multiplier * size / sectorSize);

	if (sectors > std::numeric_limits<uint32_t>::max())
	{
		sectors = std::numeric_limits<uint32_t>::max();
		valid = false;
	}

	return std::make_pair(static_cast<uint32_t>(sectors), valid);
}


S2S_TargetCfg
TargetPanel::getConfig() const
{
	S2S_TargetCfg config;

	// Try and keep unknown/unused fields as-is to support newer firmware
	// versions.
	memcpy(&config, &myConfig, sizeof(config));

	bool valid = true;

	auto scsiId = CtrlGetValue<uint8_t>(myScsiIdCtrl);
	config.scsiId = scsiId.first & S2S_CFG_TARGET_ID_BITS;
	valid = valid && scsiId.second;
	if (myEnableCtrl->IsChecked())
	{
		config.scsiId = config.scsiId | S2S_CFG_TARGET_ENABLED;
	}

	config.deviceType = myDeviceTypeCtrl->GetSelection();

	auto startSDSector = CtrlGetValue<uint32_t>(myStartSDSectorCtrl);
	config.sdSectorStart = startSDSector.first;
	valid = valid && startSDSector.second;

	auto numSectors = CtrlGetValue<uint32_t>(myNumSectorCtrl);
	config.scsiSectors = numSectors.first;
	valid = valid && numSectors.second;

	auto sectorSize = CtrlGetValue<uint16_t>(mySectorSizeCtrl);
	config.bytesPerSector = sectorSize.first;
	valid = valid && sectorSize.second;

	CtrlGetFixedString(myVendorCtrl, config.vendor, sizeof(config.vendor));
	CtrlGetFixedString(myProductCtrl, config.prodId, sizeof(config.prodId));
	CtrlGetFixedString(myRevisionCtrl, config.revision, sizeof(config.revision));
	CtrlGetFixedString(mySerialCtrl, config.serial, sizeof(config.serial));

	return config;
}

void
TargetPanel::setConfig(const S2S_TargetCfg& config)
{
	memcpy(&myConfig, &config, sizeof(config));

	myScsiIdCtrl->SetValue(config.scsiId & S2S_CFG_TARGET_ID_BITS);
	myEnableCtrl->SetValue(config.scsiId & S2S_CFG_TARGET_ENABLED);

	myDeviceTypeCtrl->SetSelection(config.deviceType);

	{
		std::stringstream ss; ss << config.sdSectorStart;
		myStartSDSectorCtrl->ChangeValue(ss.str());
		myAutoStartSectorCtrl->SetValue(0);
	}

	{
		std::stringstream ss; ss << config.scsiSectors;
		myNumSectorCtrl->ChangeValue(ss.str());
	}

	{
		std::stringstream ss; ss << config.bytesPerSector;
		mySectorSizeCtrl->ChangeValue(ss.str());
	}

	myVendorCtrl->ChangeValue(std::string(config.vendor, sizeof(config.vendor)));
	myProductCtrl->ChangeValue(std::string(config.prodId, sizeof(config.prodId)));
	myRevisionCtrl->ChangeValue(std::string(config.revision, sizeof(config.revision)));
	mySerialCtrl->ChangeValue(std::string(config.serial, sizeof(config.serial)));

	// Set the size fields based on sector size, and evaluate inputs.
	wxCommandEvent fakeEvent(wxEVT_NULL, ID_numSectorCtrl);
	onSizeInput(fakeEvent);
}

bool
TargetPanel::isEnabled() const
{
	return myEnableCtrl->IsChecked();
}

uint8_t
TargetPanel::getSCSIId() const
{
	return CtrlGetValue<uint8_t>(myScsiIdCtrl).first & S2S_CFG_TARGET_ID_BITS;
}

std::pair<uint32_t, uint64_t>
TargetPanel::getSDSectorRange() const
{
	std::pair<uint32_t, uint64_t> result;
	result.first = CtrlGetValue<uint32_t>(myStartSDSectorCtrl).first;

	uint32_t numSCSISectors = CtrlGetValue<uint32_t>(myNumSectorCtrl).first;
	uint16_t scsiSectorSize = CtrlGetValue<uint16_t>(mySectorSizeCtrl).first;

	const int sdSector = 512; // Always 512 for SDHC/SDXC
	result.second = result.first +
		(
			((uint64_t(numSCSISectors) * scsiSectorSize) + (sdSector - 1))
				/ sdSector
		);
	return result;
}

void
TargetPanel::setDuplicateID(bool duplicate)
{
	if (duplicate)
	{
		myScsiIdMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Duplicate ID</span>"));
	}
	else
	{
		myScsiIdMsg->SetLabelMarkup("");
	}
}

void
TargetPanel::setSDSectorOverlap(bool overlap)
{
	if (overlap)
	{
		myStartSDSectorMsg->SetLabelMarkup(wxT("<span foreground='red' weight='bold'>Overlapping data</span>"));
	}
	else
	{
		myStartSDSectorMsg->SetLabelMarkup("");
	}
}

void
TargetPanel::setAutoStartSector(uint32_t start)
{
	myAutoStartSector = start;
}

