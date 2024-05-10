// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the example_sdl2_opengl3/ folder**
// See imgui_impl_sdl2.cpp for details.

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include <string>
#include <vector>

// emulate idStr..
class idStr : public std::string {

public:

	idStr() = default;

	idStr(const char* s) : std::string(s) {}

	int Length() const {
		return (int)length();
	}

	operator const char* (void) const {
		return c_str();
	}

	static idStr		Format( const char* format, ... );
	static idStr		VFormat( const char* format, va_list argptr );
};

idStr idStr::Format( const char* format, ... )
{
	va_list argptr;
	va_start( argptr, format );
	idStr ret = VFormat( format, argptr );
	va_end( argptr );
	return ret;
}

idStr idStr::VFormat( const char* format, va_list argptr )
{
	idStr ret;
	int len;
	va_list argptrcopy;
	char buffer[16000];

	// make a copy of argptr in case we need to call vsnprintf() again after truncation
#ifdef va_copy // IIRC older VS versions didn't have this?
	va_copy( argptrcopy, argptr );
#else
	argptrcopy = argptr;
#endif

	len = vsnprintf( buffer, sizeof(buffer), format, argptr );

	if ( len < sizeof(buffer) ) {
		ret = buffer;
	} else {
		// string was truncated, because buffer wasn't big enough.
		ret.resize( len );
		vsnprintf( &ret[0], len+1, format, argptrcopy );
	}
	va_end( argptrcopy );

	return ret;

}

namespace idMath {
	int ClampInt(int minVal, int maxVal, int val) {
		return (val < minVal) ? minVal : (val > maxVal) ? maxVal : val;
	}
}

// add tooltip with given text to the previously added widget (visible if that item is hovered)
static void AddTooltip( const char* text )
{
	if ( ImGui::BeginItemTooltip() )
	{
		ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );
		ImGui::TextUnformatted( text );
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

// add a grey "(?)" that can be hovered for a tooltip with a description
// (or similar additional information)
static void AddDescrTooltip( const char* description )
{
	if ( description != nullptr ) {
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		AddTooltip( description );
	}
}

static idStr warningTooltipText;
static double warningTooltipStartTime = -100.0;

static void UpdateWarningTooltip()
{
	double timeNow = ImGui::GetTime();
	if ( timeNow - warningTooltipStartTime > 4.0f ) {
		return;
	}
	// TODO: also hide if a key was pressed or maybe even if the mouse was moved (too much)?

	ImGui::BeginTooltip();
	// TODO: warning icon

	ImGui::TextUnformatted( warningTooltipText.c_str() );
	ImGui::EndTooltip();
}

static void ShowWarningTooltip( const char* text )
{
	warningTooltipText = "!! ";
	warningTooltipText += text;
	warningTooltipStartTime = ImGui::GetTime();
	printf("warning: %s\n", text);
}



static int IsKeyPressed( ImGuiKey key ) {
	return ImGui::IsKeyPressed( key, false );
}

	// by default enter and space both select a cell (when using cursor keys for navigation)
	// => could use only space for select, and enter for "set binding"?
	// ImGuiKey_Enter, ImGuiKey_KeypadEnter (activate binding mode for selected entry, like double-click)
	// ImGuiKey_Escape (cancel binding mode or unselect if not in binding mode)
	// ImGuiKey_Backspace, ImGuiKey_Delete, (delete currently selected binding(s))
	// ImGuiKey_GamepadFaceDown (like enter) ImGuiKey_GamepadFaceRight (like Esc)
	//   could use ImGuiKey_GamepadL2 (left trigger) for delete?
	//   in idUserInterfaceLocal::HandleEvent() we treat left trigger as Enter
	//  but what gamepad button to use for select?
	//   - maybe ImGuiKey_GamepadFaceUp for enter and facedown for select?
	// ImGui::IsKeyPressed(key, false);

static bool IsSelectionKeyPressed() {
	return IsKeyPressed( ImGuiKey_Space ); // TODO: any gamepad button?
}

static bool IsBindNowKeyPressed() {
	return IsKeyPressed( ImGuiKey_Enter ) || IsKeyPressed( ImGuiKey_KeypadEnter ); // TODO: gamepad button?
}

static bool IsClearKeyPressed() {
	return IsKeyPressed( ImGuiKey_Delete ) || IsKeyPressed( ImGuiKey_Backspace ); // TODO: gamepad button?
}

static bool IsCancelKeyPressed() {
	// Note: In Doom3, Escape opens/closes the main menu, so in dhewm3 the gamepad Start button
	//       behaves the same, incl. the specialty that it can't be bound by the user
	return IsKeyPressed( ImGuiKey_Escape ) || IsKeyPressed( ImGuiKey_GamepadStart );
}

static bool IsUnselectKeyPressed() {
	// xbox B is probably suitable for unsetting selection - TODO: OTOH, does selection even make sense with gamepad? isn't the implicit focus enough?
	return IsCancelKeyPressed() || IsKeyPressed( ImGuiKey_GamepadFaceRight );
}

// background color for the first column of the binding table, that contains the name of the command
static ImU32 displayNameBGColor = 0;

enum BindingEntrySelectionState {
	BESS_NotSelected = 0,
	BESS_Selected,
	BESS_WantBind,
	BESS_WantClear,
	BESS_WantRebind // we were in WantBind, but the key is already bound to another command, so show a confirmation popup
};

struct BoundKey {
	int keyNum = -1;
	idStr keyName;
	idStr internalKeyName; // the one used in bind commands in the D3 console and config

	void Set( int _keyNum )
	{
		keyNum = _keyNum;
		keyName = ImGui::GetKeyName( (ImGuiKey)_keyNum ); // TODO: use idKeyInput::KeyNumToString(keyNum, true) instead!
		internalKeyName = "_int_";
		internalKeyName += keyName; // TODO: use idKeyInput::KeyNumToString(keyNum, false) instead!
	}

	BoundKey() = default;

	BoundKey ( int _keyNum ) {
		Set( _keyNum );
	}

};

struct BindingEntry;
static BindingEntry* FindBindingEntryForKey( int keyNum );
static int numBindingColumns = 4; // TODO: in real code, save in CVar (in_maxBindingsPerCommand or sth)

static int rebindKeyNum = -1; // only used for HandleRebindPopup()
static BindingEntry* rebindOtherEntry = nullptr; // ditto

struct BindingEntry {
	idStr command; // "_impulse3" or "_forward" or similar - or "" for heading entry
	idStr displayName;
	const char* description = nullptr;
	std::vector<BoundKey> bindings;
	int selectedColumn = -1;

	BindingEntry() = default;

	BindingEntry( const char* _displayName ) : displayName(_displayName) {}

	BindingEntry( const char* _command, const char* _displayName, const char* descr = nullptr )
	: command( _command ), displayName( _displayName ), description( descr ) {}

	BindingEntry( const char* _command, const char* _displayName, const char* descr, std::initializer_list<BoundKey> boundKeys )
	: command( _command ), displayName( _displayName ), description( descr ), bindings( boundKeys ) {}

#if 0
	BindingEntry( const idStr& _command, const idStr& _displayName, const char* descr = nullptr )
		: command( _command ), displayName( _displayName ), description( descr ) {}

	BindingEntry( const idStr& _command, const char* _displayName, const char* descr = nullptr )
		: command( _command ), displayName( _displayName ), description( descr ) {}

	BindingEntry( const BindingEntryTemplate& bet )
	: command( bet.command ), description( bet.description ) {
		displayName = GetLocalizedString( bet.nameLocStr, bet.name );
		displayName.StripTrailingWhitespace();
	}
#endif

	bool IsHeading() const
	{
		return command.Length() == 0;
	}

	void Init()
	{
		// TODO: get bindings from Doom3
	}

	// also updates this->selectedColumn
	void UpdateSelectionState( int column, /* in+out */ BindingEntrySelectionState& selState )
	{
		// if currently a popup is shown for creating a new binding or clearing one (BESS_WantBind
		// or BESS_WantClear), everything is still rendered, but in a disabled (greyed out) state
		// and shouldn't handle any input
		if ( selState < BESS_WantBind ) {
			if ( ImGui::IsItemFocused() ) {
				// Note: even when using the mouse, clicking a selectable will make it focused,
				//       so it's possible to select a command (or specific binding of a command)
				//       with the mouse and then press Enter to (re)bind it or Delete to clear it
				if ( IsSelectionKeyPressed() ) {
					if ( selState == BESS_Selected && selectedColumn == column ) {
						printf("focus unselect\n");
						selState = BESS_NotSelected;
						selectedColumn = -1;
					} else {
						printf("focus select\n");
						selState = BESS_Selected;
						selectedColumn = column;
					}
				} else if ( IsBindNowKeyPressed() ) {
					printf("focus bind now\n");
					selState = BESS_WantBind;
					selectedColumn = column;
				} else if ( IsClearKeyPressed() ) {
					printf("focus clear now\n");
					bool nothingToClear = false;
					if ( column == 0 ) {
						if ( bindings.size() == 0 ) {
							ShowWarningTooltip( "No keys are bound to this command, so there's nothing to unbind" );
							nothingToClear = true;
						}
					} else if ( (size_t)column > bindings.size() || bindings[column-1].keyNum == -1 ) {
						ShowWarningTooltip( "No bound key selected for unbind" );
						nothingToClear = true;
					}

					selState = nothingToClear ? BESS_Selected : BESS_WantClear;
					selectedColumn = column;
				}
			}

			if ( ImGui::IsItemHovered() ) {
				if ( column == 0 ) {
					// if the first column (command name, like "Move Left") is hovered, highlight the whole row
					// A normal Selectable would use ImGuiCol_HeaderHovered, but I use that as the "selected"
					// color (in Draw()), so use the next brighter thing (ImGuiCol_HeaderActive) here.
					ImU32 highlightRowColor = ImGui::GetColorU32( ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive) );
					ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, highlightRowColor );
				}

				bool doubleClicked = ImGui::IsMouseDoubleClicked( 0 );
				bool singleClicked = !doubleClicked && ImGui::IsMouseClicked( 0 );

				if ( singleClicked ) {
					if ( selState == BESS_Selected && selectedColumn == column ) {
						printf("hover unselect\n");
						selState = BESS_NotSelected;
						selectedColumn = -1;
					} else {
						printf("hover select\n");
						selState = BESS_Selected;
						selectedColumn = column;
					}
				} else if ( doubleClicked ) {
					printf("hover doubleclick\n");
					selState = BESS_WantBind;
					selectedColumn = column;
				}
			}
		}

		// this column is selected => highlight it
		if ( selState != BESS_NotSelected && selectedColumn == column ) {
			// ImGuiCol_Header would be the regular "selected cell/row" color that Selectable would use
			// but ImGuiCol_HeaderHovered is more visible, IMO
			ImU32 highlightRowColor = ImGui::GetColorU32( ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered) );
			ImGui::TableSetBgColor( ImGuiTableBgTarget_CellBg, highlightRowColor );

			if ( column == 0 ) {
				// the displayName column is selected => highlight the whole row
				ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, highlightRowColor );
				// (yes, still set the highlight color for ImGuiTableBgTarget_CellBg above for extra
				//  highlighting of the column 0 cell, otherwise it'd look darker due to displayNameBGColor)
			}
		}
	}

	BindingEntrySelectionState Draw( int numBindings, const BindingEntrySelectionState oldSelState )
	{
		if ( IsHeading() ) {
			ImGui::SeparatorText( displayName );
			if ( description ) {
				AddDescrTooltip( description );
			}
		} else {
			ImGui::PushID( command );

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );

			// the first column (with the display name in it) gets a different background color
			ImGui::TableSetBgColor( ImGuiTableBgTarget_CellBg, displayNameBGColor );

			BindingEntrySelectionState newSelState = oldSelState;

			// not really using the selectable feature, mostly making it selectable
			// so keyboard/gamepad navigation works
			ImGui::Selectable( "##0", false, 0 );

			UpdateSelectionState( 0, newSelState );

			ImGui::SameLine();
			ImGui::TextUnformatted( displayName );

			AddTooltip( command );

			if ( description ) {
				AddDescrTooltip( description );
			}

			for ( int col=1; col <= numBindings ; ++col ) {
				ImGui::TableSetColumnIndex( col );

				bool colHasBinding = ((size_t)col <= bindings.size()) && bindings[col-1].keyNum != -1;
				char selTxt[128];
				if ( colHasBinding ) {
					snprintf( selTxt, sizeof(selTxt), "%s###%d", bindings[col-1].keyName.c_str(), col );
				} else {
					snprintf( selTxt, sizeof(selTxt), "###%d", col );
				}
				ImGui::Selectable( selTxt, false, 0 );
				UpdateSelectionState( col, newSelState );

				if ( colHasBinding ) {
					AddTooltip( bindings[col-1].internalKeyName.c_str() );
				}
			}
			// TODO: additional column with a [...] button or similar if there are even more bindings?

			ImGui::PopID();

			if ( newSelState == BESS_NotSelected ) {
				selectedColumn = -1;
			}
			return newSelState;
		}
		return BESS_NotSelected;
	}

	void Bind( int keyNum ) {
		if ( keyNum >= 0 ) {
			printf( "bind key %d to %s (%s)\n", keyNum, command.c_str(), displayName.c_str() );
			// TODO: actual Doom3 bind implementation!
		}
	}

	void Unbind( int keyNum ) {
		if ( keyNum >= 0 ) {
			printf( "unbind key %d from %s (%s)\n", keyNum, command.c_str(), displayName.c_str() );
			// TODO: actual Doom3 unbind implementation!
		}
	}

	BindingEntrySelectionState HandleClearPopup( const char* popupName )
	{
		BindingEntrySelectionState ret = BESS_WantClear;
		int selectedBinding = selectedColumn - 1;

		if ( ImGui::BeginPopupModal( popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
		{
			idStr popupText;
			if ( selectedColumn == 0 ) {
				popupText = idStr::Format( "Clear all keybindings for %s ?", displayName.c_str() );
			} else {
				popupText = idStr::Format( "Unbind key '%s' from command %s ?",
				                           bindings[selectedBinding].keyName.c_str(), displayName.c_str() );
			}

			ImGui::TextUnformatted( popupText );
			ImGui::NewLine();
			ImGui::TextUnformatted( "Press Enter to confirm or Escape to cancel." );
			ImGui::NewLine();

			// center the Ok and Cancel buttons; 260 = 2*120 (button width) + 20 (space added between them)
			float buttonOffset = (ImGui::GetWindowWidth() - 260.0f) * 0.5f;
			ImGui::SetCursorPosX( buttonOffset );

			bool confirmedByKey = false;
			if ( !ImGui::IsAnyItemFocused() ) {
				// if no item is focused (=> not using keyboard or gamepad navigation to select
				//  [Ok] or [Cancel] button), check if Enter has been pressed to confirm deletion
				// (otherwise, enter can be used to chose the selected button)
				confirmedByKey = IsBindNowKeyPressed();
			}

			if ( ImGui::Button( "Ok", ImVec2(120, 0) ) || confirmedByKey ) {
				if ( selectedColumn == 0 ) {
					for ( BoundKey& bk : bindings ) {
						Unbind( bk.keyNum );
					}
					bindings.clear();
				} else {
					Unbind( bindings[selectedBinding].keyNum );

					bindings[selectedBinding].keyNum = -1;
					bindings[selectedBinding].keyName = "";
				}

				ImGui::CloseCurrentPopup();
				ret = BESS_Selected;
			}
			ImGui::SetItemDefaultFocus();

			ImGui::SameLine( 0.0f, 20.0f );
			if ( ImGui::Button( "Cancel", ImVec2(120, 0) ) || IsCancelKeyPressed() ) {
				ImGui::CloseCurrentPopup();
				ret = BESS_Selected;
			}

			ImGui::EndPopup();
		}

		return ret;
	}


	void AddKeyBinding( int keyNum )
	{
		Bind( keyNum );

		int numBindings = bindings.size();
		if ( selectedColumn == 0 ) {
			for ( int i=0; i < numBindings; ++i ) {
				// if there's an empty column, use that
				if ( bindings[i].keyNum == -1 ) {
					bindings[i].Set( keyNum );
					// select the column this was inserted into
					// (+1 because of the first column with displayName)
					selectedColumn = i + 1;
					return;
				}
			}
			if ( numBindings < numBindingColumns ) {
				bindings.push_back( BoundKey(keyNum) );
				selectedColumn = numBindings + 1;
			} else {
				// insert in last column (but don't remove any elements from bindings!)
				auto it = bindings.cbegin(); // I fucking hate STL.
				it += (numBindingColumns-1);
				bindings.insert( it, BoundKey(keyNum) );
				selectedColumn = numBindingColumns;
			}
		} else {
			int selectedBinding = selectedColumn - 1;
			assert( selectedBinding >= 0 );
			if ( selectedBinding < numBindings ) {
				Unbind( bindings[selectedBinding].keyNum );
				bindings[selectedBinding].Set( keyNum );
			} else  {
				if ( selectedBinding > numBindings ) {
					// apparently a column with other unset columns before it was selected
					// => add enough empty columns
					bindings.resize( selectedBinding );
				}
				bindings.push_back( BoundKey(keyNum) );
			}
		}
	}

	void RemoveKeyBinding( int keyNum )
	{
		int delPos = -1;
		int numBindings = bindings.size();
		for ( int i = 0; i < numBindings; ++i ) {
			if ( bindings[i].keyNum == keyNum ) {
				delPos = i;
				break;
			}
		}
		if ( delPos != -1 ) {
			Unbind( keyNum );
			auto it = bindings.begin() + delPos;
			bindings.erase( it );
		}
	}


	BindingEntrySelectionState HandleBindPopup( const char* popupName, bool firstOpen )
	{
		BindingEntrySelectionState ret = BESS_WantBind;
		int selectedBinding = selectedColumn - 1;

		ImGuiIO& io = ImGui::GetIO();
		// disable keyboard and gamepad input while the bind popup is open
		io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard);

		if ( ImGui::BeginPopupModal( popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
		{
			idStr popupText;
			if ( selectedColumn == 0 || (size_t)selectedBinding >= bindings.size() || bindings[selectedBinding].keyNum == -1 ) {
				// add a binding
				popupText = idStr::Format( "Press a key or button to bind to %s", displayName.c_str() );
			} else {
				// overwrite a binding
				popupText = idStr::Format( "Press a key or button to replace '%s' binding to %s",
				                           bindings[selectedBinding].keyName.c_str(), displayName.c_str() );
			}

			ImGui::TextUnformatted( popupText );
			ImGui::NewLine();
			ImGui::TextUnformatted( "To bind a mouse button, click it in the following field" );

			const float windowWidth = ImGui::GetWindowWidth();
			ImVec2 clickFieldSize( windowWidth * 0.8f, ImGui::GetTextLineHeightWithSpacing() * 4.0f );
			ImGui::SetCursorPosX( windowWidth * 0.1f );

			ImGui::Button( "###clickField", clickFieldSize );
			bool clickFieldHovered = ImGui::IsItemHovered();

			ImGui::NewLine();
			ImGui::TextUnformatted( "... or press Escape to cancel." );

			ImGui::NewLine();
			// center the Cancel button; 120 is the button width
			float buttonOffset = (windowWidth - 120.0f) * 0.5f;
			ImGui::SetCursorPosX( buttonOffset );

			if ( ImGui::Button( "Cancel", ImVec2(120, 0) ) || IsCancelKeyPressed() ) {
				ImGui::CloseCurrentPopup();
				ret = BESS_Selected;
				io.ConfigFlags |= (ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard);
			} else if ( !firstOpen ) {
				// find out if any key is pressed and bind that (except for Esc which can't be
				// bound and is already handled though IsCancelKeyPressed() above)
				// (but don't run this when the popup has just been opened, because then
				//  the key that opened this, likely Enter, is still pressed)
				ImGuiKey pressedKey = ImGuiKey_None;
				for ( int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k ) {
					ImGuiKey key = (ImGuiKey)k;
					if ( key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseMiddle && !clickFieldHovered ) {
						// ignore mouse buttons, unless they clicked the clickField
						continue;
					}

					if ( IsKeyPressed( key ) ) {
						pressedKey = key;
						break;
					}
				}

				if ( pressedKey != ImGuiKey_None ) {
					BindingEntry* oldBE = FindBindingEntryForKey( pressedKey );
					if ( oldBE == nullptr ) {
						// that key isn't bound yet, hooray!
						AddKeyBinding( pressedKey );
						ret = BESS_Selected;
					} else if ( oldBE == this ) {
						// that key is already bound to this command, show warning, otherwise do nothing
						const char* keyName = ImGui::GetKeyName( pressedKey ); // TODO: get from dhewm3 instead
						idStr warning = idStr::Format( "Key '%s' is already bound to this command (%s)!",
						                               keyName, displayName.c_str() );
						ShowWarningTooltip( warning );
						ret = BESS_Selected;
						// TODO: select column with that specific binding?
					} else {
						// that key is already bound to some other command, show confirmation popup :-/
						rebindKeyNum = pressedKey;
						rebindOtherEntry = oldBE;

						ret = BESS_WantRebind;
					}
					ImGui::CloseCurrentPopup();
					io.ConfigFlags |= (ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard);
				}
			}
			ImGui::EndPopup();
		}

		return ret;
	}

	BindingEntrySelectionState HandleRebindPopup( const char* popupName )
	{
		BindingEntrySelectionState ret = BESS_WantRebind;
		int selectedBinding = selectedColumn - 1;

		if ( ImGui::BeginPopupModal( popupName, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
		{
			const char* keyName = ImGui::GetKeyName( (ImGuiKey)rebindKeyNum );

			ImGui::Text( "Key '%s' is already bound to command %s !\nBind to %s instead?",
			             keyName, rebindOtherEntry->displayName.c_str(), displayName.c_str() );
			ImGui::NewLine();
			ImGui::TextUnformatted( "Press Enter to confirm or Escape to cancel." );
			ImGui::NewLine();

			// center the Ok and Cancel buttons; 260 = 2*120 (button width) + 20 (space added between them)
			float buttonOffset = (ImGui::GetWindowWidth() - 260.0f) * 0.5f;
			ImGui::SetCursorPosX( buttonOffset );

			bool confirmedByKey = false;
			if ( !ImGui::IsAnyItemFocused() ) {
				// if no item is focused (=> not using keyboard or gamepad navigation to select
				//  [Ok] or [Cancel] button), check if Enter has been pressed to confirm deletion
				// (otherwise, enter can be used to chose the selected button)
				confirmedByKey = IsBindNowKeyPressed();
			}

			if ( ImGui::Button( "Ok", ImVec2(120, 0) ) || confirmedByKey ) {
				rebindOtherEntry->RemoveKeyBinding( rebindKeyNum );
				AddKeyBinding( rebindKeyNum );

				rebindOtherEntry = nullptr;
				rebindKeyNum = -1;

				ImGui::CloseCurrentPopup();
				ret = BESS_Selected;
			}
			ImGui::SetItemDefaultFocus();

			ImGui::SameLine( 0.0f, 20.0f );
			if ( ImGui::Button( "Cancel", ImVec2(120, 0) ) || IsCancelKeyPressed() ) {
				rebindOtherEntry = nullptr;
				rebindKeyNum = -1;

				ImGui::CloseCurrentPopup();
				ret = BESS_Selected;
			}

			ImGui::EndPopup();
		}

		return ret;
	}

	void HandlePopup( BindingEntrySelectionState& selectionState )
	{
		assert(selectedColumn >= 0);
		const char* popupName = nullptr;

		if ( selectionState == BESS_WantClear ) {
			if ( bindings.size() == 0 || (size_t)selectedColumn > bindings.size() ||  bindings[selectedColumn-1].keyNum == -1 ) {
				// there are no bindings at all for this command, or at least not in the selected column
				// => don't show popup, but keep the cell selected
				selectionState = BESS_Selected;
				return;
			}
			popupName = (selectedColumn == 0) ? "Unbind keys" : "Unbind key";
		} else if ( selectionState == BESS_WantBind ) {
			popupName = "Bind key";
		} else {
			assert( selectionState == BESS_WantRebind );
			popupName = "Confirm rebinding key";
		}

		static bool popupOpened = false;
		bool firstOpen = false;
		if ( !popupOpened ) {
			ImGui::OpenPopup( popupName );
			popupOpened = true;
			firstOpen = true;
		}
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f) );

		BindingEntrySelectionState newSelState = BESS_Selected;
		if ( selectionState == BESS_WantClear ) {
			newSelState = HandleClearPopup( popupName );
		} else if ( selectionState == BESS_WantBind ) {
			newSelState = HandleBindPopup( popupName, firstOpen );
		} else {
			newSelState = HandleRebindPopup( popupName );
		}

		if ( newSelState != selectionState ) {
			popupOpened = false;
			selectionState = newSelState;
		}
	}
};

static BindingEntry bindingEntries[] = {
	{ "", "Move / Look", nullptr },
	{ "_forward",       "Forward"    , nullptr, { ImGuiKey_W, ImGuiKey_GamepadLStickUp } },
	{ "_back",          "Backpedal"  , "walk back" },
	{ "_moveLeft",      "Move Left"  , "strafe left" },
	{ "_moveRight",     "Move Right" , nullptr },
	{ "", "Weapons", nullptr },
	{ "_impulse0",      "Fists",  "the other kind of fisting" },
	{ "_impulse1",      "Pistol",  nullptr },
};

// return NULL if not currently bound to anything
static BindingEntry* FindBindingEntryForKey( int keyNum )
{
	for ( BindingEntry& be : bindingEntries ) {
		for ( const BoundKey& bk : be.bindings ) {
			if ( bk.keyNum == keyNum ) {
				return &be;
			}
		}
	}
	return nullptr;
}

static void DrawBindingsMenu()
{
	ImGui::PushItemWidth(70.0f); // TODO: somehow dependent on font size and/or scale or something
	ImGui::InputInt( "Number of Binding columns to show", &numBindingColumns );
	numBindingColumns = idMath::ClampInt( 1, 10, numBindingColumns );
	ImGui::PopItemWidth();

	{
		// calculate the background color for the first column of the key binding tables
		// (it contains the command, while the other columns contain the keys bound to that command)
		ImVec4 bgColor = ImGui::GetStyleColorVec4( ImGuiCol_TableHeaderBg );
		bgColor.w = 0.5f;
		displayNameBGColor = ImGui::GetColorU32( bgColor );
	}

	static int selectedRow = -1;
	static BindingEntrySelectionState selectionState = BESS_NotSelected;

	// make the key column entries in the bindings table center-aligned instead of left-aligned
	ImGui::PushStyleVar( ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.0f) );

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg;
	// inTable: are we currently adding elements to a table of bindings?
	//  (we're not when adding a heading from bindingEntries: existing tables are ended
	//   before a heading and a new table is started afterwards)
	bool inTable = false;
	// did the last ImGui::BeginTable() call return true (or is it not currently visible)?
	// (init to true so the first heading before any bindings is shown)
	bool lastBeginTable = true;
	int tableNum = 1;
	const int numBindingEntries = IM_ARRAYSIZE(bindingEntries);
	for ( int i=0; i < numBindingEntries; ++i ) {
		BindingEntry& be = bindingEntries[i];
		bool isHeading = be.IsHeading();
		if ( !isHeading && !inTable ) {
			// there is no WIP table (that has been started but not ended yet)
			// and the current element is a regular bind entry that's supposed
			// to go in a table, so start a new one
			inTable = true;
			char tableID[10];
			snprintf( tableID, sizeof(tableID), "bindTab%d", tableNum++ );
			lastBeginTable = ImGui::BeginTable( tableID, numBindingColumns+1, tableFlags );
			if ( lastBeginTable ) {
				ImGui::TableSetupScrollFreeze(1, 0);
				const float commandColumnWidth = 100.0f; // TODO: set sensible value, depending on font size/scale
				ImGui::TableSetupColumn( "Command", ImGuiTableColumnFlags_WidthFixed, commandColumnWidth );
			}
		} else if ( isHeading && inTable ) {
			// we've been adding elements to a table (unless lastBeginTable = false) that hasn't
			// been closed with EndTable() yet, but now we're at a heading so the table is done
			if ( lastBeginTable ) {
				ImGui::EndTable();
			}
			inTable = false;
		}

		if ( lastBeginTable ) { // if ImGui::BeginTable() returned false, don't draw its elements
			BindingEntrySelectionState bess = (selectedRow == i) ? selectionState : BESS_NotSelected;
			bess = be.Draw(numBindingColumns, bess);
			if ( bess != BESS_NotSelected ) {
				selectedRow = i;
				selectionState = bess;
			} else if ( selectedRow == i ) {
				// this row was selected, but be.Draw() returned BESS_NotSelected, so unselect it
				selectedRow = -1;
			}
		}
	}
	if ( inTable && lastBeginTable ) {
		// end the last binding table, if any
		ImGui::EndTable();
	}

	ImGui::PopStyleVar(); // ImGuiStyleVar_SelectableTextAlign

	// WantBind or WantClear or WantRebind => show a popup
	if ( selectionState > BESS_Selected ) {
		assert(selectedRow >= 0 && selectedRow < numBindingEntries);
		bindingEntries[selectedRow].HandlePopup( selectionState );
	}
}

static bool show_demo_window = true;

static void myWindow()
{
	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

	//ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
	ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

	DrawBindingsMenu();
	UpdateWarningTooltip();



#if 0
	static float f = 0.0f;
	static int counter = 0;
	ImGui::Checkbox("Another Window", &show_another_window);

	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
	ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

	if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
#endif // 0
	ImGui::End();
}

// Main code
int main(int, char**)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1680, 1050, window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state

    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            myWindow();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context where shaders may be bound
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
