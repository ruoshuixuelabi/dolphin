// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Globals.h"
#include "Frame.h"
#include "FileUtil.h"

#include "GameListCtrl.h"
#include "BootManager.h"

#include "Common.h"
#include "Config.h"
#include "Core.h"
#include "HW\DVDInterface.h"
#include "State.h"
#include "ConfigMain.h"
#include "PluginManager.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "AboutDolphin.h"

#include <wx/mstream.h>

// ----------------------------------------------------------------------------
// resources
// ----------------------------------------------------------------------------

extern "C" {
#include "../resources/Dolphin.c"
#include "../resources/toolbar_browse.c"
#include "../resources/toolbar_file_open.c"
#include "../resources/toolbar_fullscreen.c"
#include "../resources/toolbar_help.c"
#include "../resources/toolbar_pause.c"
#include "../resources/toolbar_play.c"
#include "../resources/toolbar_plugin_dsp.c"
#include "../resources/toolbar_plugin_gfx.c"
#include "../resources/toolbar_plugin_options.c"
#include "../resources/toolbar_plugin_pad.c"
#include "../resources/toolbar_refresh.c"
#include "../resources/toolbar_stop.c"
};

using namespace DVDInterface;

#define wxGetBitmapFromMemory(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	return(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
}


// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const long TOOLBAR_STYLE = wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;

// ----------------------------------------------------------------------------
// event tables
// ----------------------------------------------------------------------------

// Notice that wxID_HELP will be processed for the 'About' menu and the toolbar
// help button.

const wxEventType wxEVT_HOST_COMMAND = wxNewEventType();

BEGIN_EVENT_TABLE(CFrame, wxFrame)
EVT_CLOSE(CFrame::OnClose)
EVT_MENU(wxID_OPEN, CFrame::OnOpen)
EVT_MENU(wxID_EXIT, CFrame::OnQuit)
EVT_MENU(IDM_HELPWEBSITE, CFrame::OnHelp)
EVT_MENU(IDM_HELPGOOGLECODE, CFrame::OnHelp)
EVT_MENU(IDM_HELPABOUT, CFrame::OnHelp)
EVT_MENU(wxID_REFRESH, CFrame::OnRefresh)
EVT_MENU(IDM_PLAY, CFrame::OnPlay)
EVT_MENU(IDM_STOP, CFrame::OnStop)
EVT_MENU(IDM_CONFIG_MAIN, CFrame::OnConfigMain)
EVT_MENU(IDM_CONFIG_GFX_PLUGIN, CFrame::OnPluginGFX)
EVT_MENU(IDM_CONFIG_DSP_PLUGIN, CFrame::OnPluginDSP)
EVT_MENU(IDM_CONFIG_PAD_PLUGIN, CFrame::OnPluginPAD)
EVT_MENU(IDM_CONFIG_WIIMOTE_PLUGIN, CFrame::OnPluginWiimote)
EVT_MENU(IDM_BROWSE, CFrame::OnBrowse)
EVT_MENU(IDM_MEMCARD, CFrame::OnMemcard)
EVT_MENU(IDM_CHEATS, CFrame::OnShow_CheatsWindow)
EVT_MENU(IDM_SWAPDISC, CFrame::OnSwapDisc)
EVT_MENU(IDM_TOGGLE_FULLSCREEN, CFrame::OnToggleFullscreen)
EVT_MENU(IDM_TOGGLE_DUALCORE, CFrame::OnToggleDualCore)
EVT_MENU(IDM_TOGGLE_SKIPIDLE, CFrame::OnToggleSkipIdle)
EVT_MENU(IDM_TOGGLE_TOOLBAR, CFrame::OnToggleToolbar)
EVT_MENU(IDM_TOGGLE_STATUSBAR, CFrame::OnToggleStatusbar)
EVT_MENU_RANGE(IDM_LOADSLOT1, IDM_LOADSLOT10, CFrame::OnLoadState)
EVT_MENU_RANGE(IDM_SAVESLOT1, IDM_SAVESLOT10, CFrame::OnSaveState)
EVT_SIZE(CFrame::OnResize)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// Other Windows
// ----------------------------------------------------------------------------

wxCheatsWindow* CheatsWindow;

// ----------------------------------------------------------------------------
// implementation
// ----------------------------------------------------------------------------

CFrame::CFrame(wxFrame* parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		long style)
	: wxFrame(parent, id, title, pos, size, style)
	, m_pStatusBar(NULL)
	, HaveLeds(false), HaveSpeakers(false)
        , m_Panel(NULL)
	, m_pMenuBar(NULL)
          
{
	InitBitmaps();

	// Give it an icon
	wxIcon IconTemp;
	IconTemp.CopyFromBitmap(wxGetBitmapFromMemory(dolphin_png));
	SetIcon(IconTemp);

	// Give it a status bar
	m_pStatusBar = CreateStatusBar();

	CreateMenu();

	// This panel is the parent for rendering and it holds the gamelistctrl
	m_Panel = new wxPanel(this);

	m_GameListCtrl = new CGameListCtrl(m_Panel, LIST_CTRL,
			wxDefaultPosition, wxDefaultSize,
			wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT);

	sizerPanel = new wxBoxSizer(wxHORIZONTAL);
	sizerPanel->Add(m_GameListCtrl, 2, wxEXPAND);
	m_Panel->SetSizer(sizerPanel);

	// Create the toolbar
	RecreateToolbar();
	
	Show();

	CPluginManager::GetInstance().ScanForPlugins(this);

	//if we are ever going back to optional iso caching: 
	//m_GameListCtrl->Update(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableIsoCache);
	m_GameListCtrl->Update();
	//sizerPanel->SetSizeHints(m_Panel);

	wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN,
			wxKeyEventHandler(CFrame::OnKeyDown),
			(wxObject*)0, this);

	UpdateGUI();
}

#ifdef _WIN32

WXLRESULT CFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	switch (nMsg)
	{
	case WM_SYSCOMMAND:
		switch (wParam) 
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		}
	default:
		return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
	}
}

#endif

// =======================================================
// Create menu items
// -------------
void CFrame::CreateMenu()
{
	delete m_pMenuBar;
	m_pMenuBar = new wxMenuBar(wxMB_DOCKABLE);

	// file menu
	wxMenu* fileMenu = new wxMenu;
	fileMenu->Append(wxID_OPEN, _T("&Open...\tCtrl+O"));
	fileMenu->Append(wxID_REFRESH, _T("&Refresh"));
	fileMenu->Append(IDM_BROWSE, _T("&Browse for ISOs..."));

	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _T("E&xit"), _T("Alt+F4"));
	m_pMenuBar->Append(fileMenu, _T("&File"));

	// emulation menu
	wxMenu* emulationMenu = new wxMenu;
	m_pMenuItemPlay = emulationMenu->Append(IDM_PLAY, _T("&Play"));
	m_pMenuItemStop = emulationMenu->Append(IDM_STOP, _T("&Stop"));
	emulationMenu->AppendSeparator();
	wxMenu *saveMenu = new wxMenu;
	wxMenu *loadMenu = new wxMenu;
	m_pMenuItemLoad = emulationMenu->AppendSubMenu(saveMenu, _T("&Load State"));
	m_pMenuItemSave = emulationMenu->AppendSubMenu(loadMenu, _T("Sa&ve State"));
	for (int i = 1; i < 10; i++) {
		saveMenu->Append(IDM_LOADSLOT1 + i - 1, wxString::Format(_T("Slot %i\tF%i"), i, i));
		loadMenu->Append(IDM_SAVESLOT1 + i - 1, wxString::Format(_T("Slot %i\tShift+F%i"), i, i));
	}
	m_pMenuBar->Append(emulationMenu, _T("&Emulation"));

	// options menu
	wxMenu* pOptionsMenu = new wxMenu;
	m_pPluginOptions = pOptionsMenu->Append(IDM_CONFIG_MAIN, _T("Co&nfigure..."));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_CONFIG_GFX_PLUGIN, _T("&GFX settings"));
	pOptionsMenu->Append(IDM_CONFIG_DSP_PLUGIN, _T("&DSP settings"));
	pOptionsMenu->Append(IDM_CONFIG_PAD_PLUGIN, _T("&PAD settings"));
#ifdef _WIN32
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_TOGGLE_FULLSCREEN, _T("&Fullscreen\tAlt+Enter"));
#endif			
	m_pMenuBar->Append(pOptionsMenu, _T("&Options"));

	// misc menu
	wxMenu* miscMenu = new wxMenu;
	miscMenu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _T("View &toolbar"));
	miscMenu->Check(IDM_TOGGLE_TOOLBAR, true);
	miscMenu->AppendCheckItem(IDM_TOGGLE_STATUSBAR, _T("View &statusbar"));
	miscMenu->Check(IDM_TOGGLE_STATUSBAR, true);
	miscMenu->AppendSeparator();
	miscMenu->Append(IDM_MEMCARD, _T("&Memcard manager"));
	miscMenu->Append(IDM_CHEATS, _T("Action &Replay Manager"));
	// miscMenu->Append(IDM_SWAPDISC, _T("S&wap Disc"));
	m_pMenuBar->Append(miscMenu, _T("&Misc"));

	// help menu
	wxMenu* helpMenu = new wxMenu;
	/*helpMenu->Append(wxID_HELP, _T("&Help"));
	re-enable when there's something useful to display*/
	helpMenu->Append(IDM_HELPWEBSITE, _T("Dolphin &web site"));
	helpMenu->Append(IDM_HELPGOOGLECODE, _T("Dolphin at &Google Code"));
	helpMenu->AppendSeparator();
	helpMenu->Append(IDM_HELPABOUT, _T("&About..."));
	m_pMenuBar->Append(helpMenu, _T("&Help"));

	// Associate the menu bar with the frame
	SetMenuBar(m_pMenuBar);
}


void CFrame::PopulateToolbar(wxToolBar* toolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
	    h = m_Bitmaps[Toolbar_FileOpen].GetHeight();

	toolBar->SetToolBitmapSize(wxSize(w, h));
	toolBar->AddTool(wxID_OPEN,    _T("Open"),    m_Bitmaps[Toolbar_FileOpen], _T("Open file..."));
	toolBar->AddTool(wxID_REFRESH, _T("Refresh"), m_Bitmaps[Toolbar_Refresh], _T("Refresh"));
	toolBar->AddTool(IDM_BROWSE, _T("Browse"),   m_Bitmaps[Toolbar_Browse], _T("Browse for an ISO directory..."));
	toolBar->AddSeparator();
	m_pToolPlay = toolBar->AddTool(IDM_PLAY, _T("Play"),   m_Bitmaps[Toolbar_Play], _T("Play")); 

	toolBar->AddTool(IDM_STOP, _T("Stop"),   m_Bitmaps[Toolbar_Stop], _T("Stop"));
#ifdef _WIN32
	toolBar->AddTool(IDM_TOGGLE_FULLSCREEN, _T("Fullscr."),  m_Bitmaps[Toolbar_FullScreen], _T("Toggle Fullscreen"));
#endif
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_CONFIG_MAIN, _T("Config"), m_Bitmaps[Toolbar_PluginOptions], _T("Configure..."));
	toolBar->AddTool(IDM_CONFIG_GFX_PLUGIN, _T("GFX"),  m_Bitmaps[Toolbar_PluginGFX], _T("GFX settings"));
	toolBar->AddTool(IDM_CONFIG_DSP_PLUGIN, _T("DSP"),  m_Bitmaps[Toolbar_PluginDSP], _T("DSP settings"));
	toolBar->AddTool(IDM_CONFIG_PAD_PLUGIN, _T("PAD"),  m_Bitmaps[Toolbar_PluginPAD], _T("PAD settings"));
	toolBar->AddTool(IDM_CONFIG_WIIMOTE_PLUGIN, _T("Wiimote"),  m_Bitmaps[Toolbar_PluginPAD], _T("Wiimote settings"));
	toolBar->AddSeparator();
	toolBar->AddTool(IDM_HELPABOUT, _T("About"), m_Bitmaps[Toolbar_Help], _T("About Dolphin"));

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	toolBar->Realize();
}


void CFrame::RecreateToolbar()
{
	// delete and recreate the toolbar
	wxToolBarBase* toolBar = GetToolBar();
	long style = toolBar ? toolBar->GetWindowStyle() : TOOLBAR_STYLE;

	delete toolBar;
	SetToolBar(NULL);

	style &= ~(wxTB_HORIZONTAL | wxTB_VERTICAL | wxTB_BOTTOM | wxTB_RIGHT | wxTB_HORZ_LAYOUT | wxTB_TOP);
	wxToolBar* theToolBar = CreateToolBar(style, ID_TOOLBAR);

	PopulateToolbar(theToolBar);
	SetToolBar(theToolBar);
	UpdateGUI();
}


void CFrame::InitBitmaps()
{
	// load orignal size 48x48
	m_Bitmaps[Toolbar_FileOpen] = wxGetBitmapFromMemory(toolbar_file_open_png);
	m_Bitmaps[Toolbar_Refresh] = wxGetBitmapFromMemory(toolbar_refresh_png);
	m_Bitmaps[Toolbar_Browse] = wxGetBitmapFromMemory(toolbar_browse_png);
	m_Bitmaps[Toolbar_Play] = wxGetBitmapFromMemory(toolbar_play_png);
	m_Bitmaps[Toolbar_Stop] = wxGetBitmapFromMemory(toolbar_stop_png);
	m_Bitmaps[Toolbar_Pause] = wxGetBitmapFromMemory(toolbar_pause_png);
	m_Bitmaps[Toolbar_PluginOptions] = wxGetBitmapFromMemory(toolbar_plugin_options_png);
	m_Bitmaps[Toolbar_PluginGFX]  = wxGetBitmapFromMemory(toolbar_plugin_gfx_png);
	m_Bitmaps[Toolbar_PluginDSP]  = wxGetBitmapFromMemory(toolbar_plugin_dsp_png);
	m_Bitmaps[Toolbar_PluginPAD]  = wxGetBitmapFromMemory(toolbar_plugin_pad_png);
	m_Bitmaps[Toolbar_FullScreen] = wxGetBitmapFromMemory(toolbar_fullscreen_png);
	m_Bitmaps[Toolbar_Help] = wxGetBitmapFromMemory(toolbar_help_png);

	// scale to 24x24 for toolbar
	for (size_t n = Toolbar_FileOpen; n < WXSIZEOF(m_Bitmaps); n++)
	{
		m_Bitmaps[n] = wxBitmap(m_Bitmaps[n].ConvertToImage().Scale(24, 24));
	}
}


void CFrame::OnOpen(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		return;
	wxString path = wxFileSelector(
			_T("Select the file to load"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			wxString::Format
			(
					_T("All GC/Wii files (elf, dol, gcm, iso)|*.elf;*.dol;*.gcm;*.iso;*.gcz|All files (%s)|%s"),
					wxFileSelectorDefaultWildcardStr,
					wxFileSelectorDefaultWildcardStr
			),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);
	if (!path)
	{
		return;
	}
	BootManager::BootCore(std::string(path.ToAscii()));
}


void CFrame::OnQuit(wxCommandEvent& WXUNUSED (event))
{
	Close(true);
}


void CFrame::OnClose(wxCloseEvent& event)
{
	// Don't forget the skip of the window won't be destroyed
	event.Skip();

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		Core::Stop();
		UpdateGUI();
	}
}

void CFrame::OnHelp(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_HELPABOUT:
		{
		AboutDolphin frame(this);
		frame.ShowModal();
		break;
		}
	case IDM_HELPWEBSITE:
		File::Launch("http://www.dolphin-emu.com/");
		break;
	case IDM_HELPGOOGLECODE:
		File::Launch("http://code.google.com/p/dolphin-emu/");
		break;
	}
}


// =======================================================
// Play button
// -------------
void CFrame::OnPlay(wxCommandEvent& WXUNUSED (event))
{
	// shuffle2: wxBusyInfo is meant to be created on the stack
	// and only stay around for the life of the scope it's in.
	// If that is not what we want, find another solution. I don't
	// think such a dialog is needed anyways, so maybe kill it?
	wxBusyInfo bootingDialog(wxString::FromAscii("Booting..."), this);

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Core::GetState() == Core::CORE_RUN)
		{
			Core::SetState(Core::CORE_PAUSE);
		}
		else
		{
			Core::SetState(Core::CORE_RUN);
		}
		UpdateGUI();
	}
	// Start the selected ISO
	else if (m_GameListCtrl->GetSelectedISO() != 0)
	{
		BootManager::BootCore(m_GameListCtrl->GetSelectedISO()->GetFileName());
	}
	/* Start the default ISO, or if we don't have a default ISO, start the last
	   started ISO */
	else if (!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.empty() && 
		wxFileExists(wxString(SConfig::GetInstance().m_LocalCoreStartupParameter.
			m_strDefaultGCM.c_str(), wxConvUTF8)))
	{
		BootManager::BootCore(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM);
	}
	else if (!SConfig::GetInstance().m_LastFilename.empty() && 
	wxFileExists(wxString(SConfig::GetInstance().m_LastFilename.c_str(), wxConvUTF8)))
	{
		BootManager::BootCore(SConfig::GetInstance().m_LastFilename);
	}
}
// =============


void CFrame::OnStop(wxCommandEvent& WXUNUSED (event))
{
	Core::Stop();
	UpdateGUI();
}


void CFrame::OnRefresh(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl)
	{
		m_GameListCtrl->Update();
	}
}


void CFrame::OnConfigMain(wxCommandEvent& WXUNUSED (event))
{
	CConfigMain ConfigMain(this);
	ConfigMain.ShowModal();
	if (ConfigMain.bRefreshList)
		m_GameListCtrl->Update();
}


void CFrame::OnPluginGFX(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoPlugin.c_str()
			);
}


void CFrame::OnPluginDSP(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDSPPlugin.c_str()
			);
}

void CFrame::OnPluginPAD(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strPadPlugin.c_str()
			);
}
void CFrame::OnPluginWiimote(wxCommandEvent& WXUNUSED (event))
{
	CPluginManager::GetInstance().OpenConfig(
			GetHandle(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strWiimotePlugin.c_str()
			);
}

void CFrame::OnBrowse(wxCommandEvent& WXUNUSED (event))
{
	m_GameListCtrl->BrowseForDirectory();
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED (event))
{
	CMemcardManager MemcardManager(this);
	MemcardManager.ShowModal();
}

void CFrame::OnShow_CheatsWindow(wxCommandEvent& WXUNUSED (event))
{
	CheatsWindow = new wxCheatsWindow(this, wxDefaultPosition, wxSize(600, 390));
}

void CFrame::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_UPDATEGUI:
		UpdateGUI();
		break;

	case IDM_UPDATESTATUSBAR:
		if (m_pStatusBar != NULL)
		{
			m_pStatusBar->SetStatusText(event.GetString(), event.GetInt());
		}
		break;
	}
}

void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED (event))
{
	ShowFullScreen(true);
	UpdateGUI();
}

void CFrame::OnToggleDualCore(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore = !SConfig::GetInstance().m_LocalCoreStartupParameter.bUseDualCore;
	SConfig::GetInstance().SaveSettings();
}
void CFrame::OnToggleSkipIdle(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = !SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnLoadState(wxCommandEvent& event)
{
	int id = event.GetId();
	int slot = id - IDM_LOADSLOT1 + 1;
	State_Load(slot);
}

void CFrame::OnResize(wxSizeEvent& event)
{
	DoMoveIcons(); // In FrameWiimote.cpp
	event.Skip();
}

void CFrame::OnSaveState(wxCommandEvent& event)
{
	int id = event.GetId();
	int slot = id - IDM_SAVESLOT1 + 1;
	State_Save(slot);
}

void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
	wxToolBarBase* toolBar = GetToolBar();
	
	if (event.IsChecked())
	{
		CFrame::RecreateToolbar();
	}
	else
	{
		delete toolBar;
		SetToolBar(NULL);
	}

	this->SendSizeEvent();
}

void CFrame::OnToggleStatusbar(wxCommandEvent& event)
{
	if (event.IsChecked())
		m_pStatusBar->Show();
	else
		m_pStatusBar->Hide();

	this->SendSizeEvent();
}

void CFrame::OnKeyDown(wxKeyEvent& event)
{
	// Toggle fullscreen from Alt + Enter or Esc
	if (((event.GetKeyCode() == WXK_RETURN) && (event.GetModifiers() == wxMOD_ALT)) ||
	    (event.GetKeyCode() == WXK_ESCAPE))
	{
		ShowFullScreen(!IsFullScreen());
		UpdateGUI();
	}
#ifdef _WIN32
	else if(event.GetKeyCode() == 'E') // Send this to the video plugin WndProc
	{
		PostMessage((HWND)Core::GetWindowHandle(), WM_KEYDOWN, event.GetKeyCode(), 0);
		event.Skip();
	}
#endif
	else
	{
		event.Skip();
	}
}

void CFrame::UpdateGUI()
{
	bool initialized = Core::GetState() != Core::CORE_UNINITIALIZED;
	bool running = Core::GetState() == Core::CORE_RUN;
	bool paused = Core::GetState() == Core::CORE_PAUSE;

	if (GetToolBar() != NULL)
	{
		GetToolBar()->EnableTool(IDM_CONFIG_MAIN, !initialized);
		GetToolBar()->EnableTool(IDM_STOP, running || paused);
	}
	m_pMenuItemStop->Enable(running || paused);
	m_pMenuItemLoad->Enable(initialized);
	m_pMenuItemSave->Enable(initialized);
	m_pPluginOptions->Enable(!running && !paused);

	if (running)
	{
		if (GetToolBar() != NULL)
		{
			m_pToolPlay->SetNormalBitmap(m_Bitmaps[Toolbar_Pause]);
			m_pToolPlay->SetShortHelp(_("Pause"));
			m_pToolPlay->SetLabel(_("Pause"));
		}
		m_pMenuItemPlay->SetText(_("&Pause"));
	}
	else
	{
		if (GetToolBar() != NULL)
		{
			m_pToolPlay->SetNormalBitmap(m_Bitmaps[Toolbar_Play]);
			m_pToolPlay->SetShortHelp(_("Play"));
			m_pToolPlay->SetLabel(_("Play"));
		}
		m_pMenuItemPlay->SetText(_("&Play"));
	}
	if (GetToolBar() != NULL) GetToolBar()->Realize();


	if (!initialized)
	{
		if (m_GameListCtrl && !m_GameListCtrl->IsShown())
		{
			m_GameListCtrl->Enable();
			m_GameListCtrl->Show();
			sizerPanel->FitInside(m_Panel);
		}
	}
	else
	{
		if (m_GameListCtrl && m_GameListCtrl->IsShown())
		{
			m_GameListCtrl->Disable();
			m_GameListCtrl->Hide();
		}
	}
}

void CFrame::OnSwapDisc(wxCommandEvent& WXUNUSED (event))
{
	PanicAlert("Omega: I opened the lid");
	SetLidOpen(true);
}