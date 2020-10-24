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

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/wrapsizer.h>

#include "ConfigUtil.hh"
#include "BoardPanel.hh"

#include <limits>
#include <sstream>

#include <math.h>
#include <string.h>

using namespace SCSI2SD;

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

}

BoardPanel::BoardPanel(wxWindow* parent, const S2S_BoardCfg& initialConfig) :
	wxPanel(parent),
	myParent(parent),
	myDelayValidator(new wxIntegerValidator<uint8_t>)
{
	wxFlexGridSizer *fgs = new wxFlexGridSizer(13, 2, 9, 25);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myTermCtrl =
		new wxCheckBox(
			this,
			ID_termCtrl,
			_("Enable SCSI terminator"));
	myTermCtrl->SetToolTip(_("Enable active terminator. Both ends of the SCSI chain must be terminated."));
	fgs->Add(myTermCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, _("SCSI Speed Limit")));
	wxString speeds[] = {
		wxT("No limit (safe)"),
		wxT("Async, 1.5MB/s"),
		wxT("Async, 3.3MB/s"),
		wxT("Async, 5 MB/s"),
		wxT("Sync, 5 MB/s"),
		wxT("Sync, 10 MB/s"),
		wxT("Turbo (less reliable)")};

	myScsiSpeedCtrl =
		new wxChoice(
			this,
			ID_scsiSpeedCtrl,
			wxDefaultPosition,
			wxDefaultSize,
			sizeof(speeds) / sizeof(wxString),
			speeds
			);
	myScsiSpeedCtrl->SetToolTip(_("Limit SCSI interface speed"));
	fgs->Add(myScsiSpeedCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, _("Startup Delay (seconds)")));
	myStartDelayCtrl =
		new wxTextCtrl(
			this,
			ID_startDelayCtrl,
			"0",
			wxDefaultPosition,
			wxDefaultSize,
			0,
			*myDelayValidator);
	myStartDelayCtrl->SetToolTip(_("Extra delay on power on, normally set to 0"));
	fgs->Add(myStartDelayCtrl);


	fgs->Add(new wxStaticText(this, wxID_ANY, _("SCSI Selection Delay (ms, 255 = auto)")));
	mySelDelayCtrl =
		new wxTextCtrl(
			this,
			ID_selDelayCtrl,
			"255",
			wxDefaultPosition,
			wxDefaultSize,
			0,
			*myDelayValidator);
	mySelDelayCtrl->SetToolTip(_("Delay before responding to SCSI selection. SCSI1 hosts usually require 1ms delay, however some require no delay"));
	fgs->Add(mySelDelayCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myParityCtrl =
		new wxCheckBox(
			this,
			ID_parityCtrl,
			_("Enable Parity"));
	myParityCtrl->SetToolTip(_("Enable to require valid SCSI parity bits when receiving data. Some hosts don't provide parity. SCSI2SD always outputs valid parity bits."));
	fgs->Add(myParityCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myUnitAttCtrl =
		new wxCheckBox(
			this,
			ID_unitAttCtrl,
			_("Enable Unit Attention"));
	myUnitAttCtrl->SetToolTip(_("Enable this to inform the host of changes after hot-swapping SD cards. Causes problems with Mac Plus."));
	fgs->Add(myUnitAttCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myScsi2Ctrl =
		new wxCheckBox(
			this,
			ID_scsi2Ctrl,
			_("Enable SCSI2 Mode"));
	myScsi2Ctrl->SetToolTip(_("Enable high-performance mode. May cause problems with SASI/SCSI1 hosts."));
	fgs->Add(myScsi2Ctrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	mySelLatchCtrl =
		new wxCheckBox(
			this,
			ID_selLatchCtrl,
			_("Respond to short SCSI selection pulses"));
	mySelLatchCtrl->SetToolTip(_("Respond to very short duration selection attempts. This supports non-standard hardware, but is generally safe to enable.  Required for Philips P2000C."));
	fgs->Add(mySelLatchCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myMapLunsCtrl =
		new wxCheckBox(
			this,
			ID_mapLunsCtrl,
			_("Map LUNS to SCSI IDs"));
	myMapLunsCtrl->SetToolTip(_("Treat LUNS as IDs instead. Supports multiple drives on XEBEC S1410 SASI Bridge"));
	fgs->Add(myMapLunsCtrl);


	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myBlindWriteCtrl =
		new wxCheckBox(
			this,
			ID_blindWriteCtrl,
			_("Enable Blind Writes"));
	myBlindWriteCtrl->SetToolTip(_("Enable writing to the SD card before all the SCSI data has been received."));
	fgs->Add(myBlindWriteCtrl);

	wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(fgs, 1, wxALL | wxEXPAND, 15);
	this->SetSizer(hbox);
	Centre();

	setConfig(initialConfig);
}


S2S_BoardCfg
BoardPanel::getConfig() const
{
	S2S_BoardCfg config;

	// Try and keep unknown/unused fields as-is to support newer firmware
	// versions.
	memcpy(&config, &myConfig, sizeof(config));

	config.flags =
		(myParityCtrl->IsChecked() ? S2S_CFG_ENABLE_PARITY : 0) |
		(myUnitAttCtrl->IsChecked() ? S2S_CFG_ENABLE_UNIT_ATTENTION : 0) |
		(myScsi2Ctrl->IsChecked() ? S2S_CFG_ENABLE_SCSI2 : 0) |
		(mySelLatchCtrl->IsChecked() ? S2S_CFG_ENABLE_SEL_LATCH : 0) |
		(myMapLunsCtrl->IsChecked() ? S2S_CFG_MAP_LUNS_TO_IDS : 0);

	config.flags6 = (myTermCtrl->IsChecked() ? S2S_CFG_ENABLE_TERMINATOR : 0) |
		(myBlindWriteCtrl->IsChecked() ? S2S_CFG_ENABLE_BLIND_WRITES : 0);

	config.startupDelay = CtrlGetValue<unsigned int>(myStartDelayCtrl).first;
	config.selectionDelay = CtrlGetValue<unsigned int>(mySelDelayCtrl).first;
	config.scsiSpeed = myScsiSpeedCtrl->GetSelection();
	return config;
}

void
BoardPanel::setConfig(const S2S_BoardCfg& config)
{
	memcpy(&myConfig, &config, sizeof(config));

	myParityCtrl->SetValue(config.flags & S2S_CFG_ENABLE_PARITY);
	myUnitAttCtrl->SetValue(config.flags & S2S_CFG_ENABLE_UNIT_ATTENTION);
	myScsi2Ctrl->SetValue(config.flags & S2S_CFG_ENABLE_SCSI2);
	myTermCtrl->SetValue(config.flags6 & S2S_CFG_ENABLE_TERMINATOR);
	myBlindWriteCtrl->SetValue(config.flags6 & S2S_CFG_ENABLE_BLIND_WRITES);
	mySelLatchCtrl->SetValue(config.flags & S2S_CFG_ENABLE_SEL_LATCH);
	myMapLunsCtrl->SetValue(config.flags & S2S_CFG_MAP_LUNS_TO_IDS);

	{
		std::stringstream conv;
		conv << static_cast<unsigned int>(config.startupDelay);
		myStartDelayCtrl->ChangeValue(conv.str());
	}
	{
		std::stringstream conv;
		conv << static_cast<unsigned int>(config.selectionDelay);
		mySelDelayCtrl->ChangeValue(conv.str());
	}
	myScsiSpeedCtrl->SetSelection(config.scsiSpeed);
}


