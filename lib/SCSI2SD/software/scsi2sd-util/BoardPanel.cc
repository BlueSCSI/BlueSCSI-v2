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

BoardPanel::BoardPanel(wxWindow* parent, const BoardConfig& initialConfig) :
	wxPanel(parent),
	myParent(parent),
	myDelayValidator(new wxIntegerValidator<uint8_t>)
{
	wxFlexGridSizer *fgs = new wxFlexGridSizer(8, 2, 9, 25);

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
	myGlitchCtrl =
		new wxCheckBox(
			this,
			ID_glitchCtrl,
			_("Disable glitch filter"));
	myGlitchCtrl->SetToolTip(_("Improve performance at the cost of noise immunity. Only use with short cables."));
	fgs->Add(myGlitchCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myCacheCtrl =
		new wxCheckBox(
			this,
			ID_cacheCtrl,
			_("Enable disk cache (experimental)"));
	myCacheCtrl->SetToolTip(_("SD IO commands aren't completed when SCSI commands complete"));
	fgs->Add(myCacheCtrl);

	fgs->Add(new wxStaticText(this, wxID_ANY, wxT("")));
	myDisconnectCtrl =
		new wxCheckBox(
			this,
			ID_disconnectCtrl,
			_("Enable SCSI Disconnect"));
	myDisconnectCtrl->SetToolTip(_("Release the SCSI bus while waiting for SD card writes to complete. Must also be enabled in host OS."));
	fgs->Add(myDisconnectCtrl);

	wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(fgs, 1, wxALL | wxEXPAND, 15);
	this->SetSizer(hbox);
	Centre();

	setConfig(initialConfig);
}


BoardConfig
BoardPanel::getConfig() const
{
	BoardConfig config;

	// Try and keep unknown/unused fields as-is to support newer firmware
	// versions.
	memcpy(&config, &myConfig, sizeof(config));

	config.flags =
		(myParityCtrl->IsChecked() ? CONFIG_ENABLE_PARITY : 0) |
		(myUnitAttCtrl->IsChecked() ? CONFIG_ENABLE_UNIT_ATTENTION : 0) |
		(myScsi2Ctrl->IsChecked() ? CONFIG_ENABLE_SCSI2 : 0) |
		(myGlitchCtrl->IsChecked() ? CONFIG_DISABLE_GLITCH : 0) |
		(myCacheCtrl->IsChecked() ? CONFIG_ENABLE_CACHE: 0) |
		(myDisconnectCtrl->IsChecked() ? CONFIG_ENABLE_DISCONNECT: 0);

	config.startupDelay = CtrlGetValue<unsigned int>(myStartDelayCtrl).first;
	config.selectionDelay = CtrlGetValue<unsigned int>(mySelDelayCtrl).first;
	return config;
}

void
BoardPanel::setConfig(const BoardConfig& config)
{
	memcpy(&myConfig, &config, sizeof(config));

	myParityCtrl->SetValue(config.flags & CONFIG_ENABLE_PARITY);
	myUnitAttCtrl->SetValue(config.flags & CONFIG_ENABLE_UNIT_ATTENTION);
	myScsi2Ctrl->SetValue(config.flags & CONFIG_ENABLE_SCSI2);
	myGlitchCtrl->SetValue(config.flags & CONFIG_DISABLE_GLITCH);
	myCacheCtrl->SetValue(config.flags & CONFIG_ENABLE_CACHE);
	myDisconnectCtrl->SetValue(config.flags & CONFIG_ENABLE_DISCONNECT);

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
}


