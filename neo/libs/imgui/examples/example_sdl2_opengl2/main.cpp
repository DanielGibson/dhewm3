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
};

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
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
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

struct BindingEntry {
	idStr command; // "_impulse3" or "_forward" or similar - or "" for heading entry
	idStr displayName;
	const char* description = nullptr;
	int selectedColumn = -1;
	// TODO: actual bindings?

	BindingEntry() = default;

	BindingEntry( const char* _displayName ) : displayName(_displayName) {}

	BindingEntry( const char* _command, const char* _displayName, const char* descr = nullptr )
	: command( _command ), displayName( _displayName ), description( descr ) {}

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
		// TODO: get bindings
	}

	bool Draw( int numBindings, bool isSelected )
	{
		if ( IsHeading() ) {
			ImGui::SeparatorText( displayName );
			if(description) {
				AddDescrTooltip(description);
			}
		} else {
			ImGui::PushID(command);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );

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

			// ImGui::SetItemDefaultFocus() for clicked item? is that the same as selecting with cursor keys?

			bool doubleClick = ImGui::IsMouseDoubleClicked( 0 );

			bool selectThis = isSelected && (selectedColumn == 0);
			// TODO: ImGuiSelectableFlags_AllowOverlap ?
			if ( ImGui::Selectable( "##0", selectThis, ImGuiSelectableFlags_AllowDoubleClick ) ) {
				if ( doubleClick ) {
					// TODO: add binding
				} else {
					isSelected = !selectThis;
					selectedColumn = isSelected ? 0 : -1;
				}
			}

			// if the first column (action name, like "Move Left") is hovered, highlight the whole row
			if ( ImGui::IsItemHovered() ) {
				ImU32 highlightRowColor = ImGui::GetColorU32( ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered) );
				ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, highlightRowColor );
			}

			ImGui::SameLine();
			// TODO ImGui::IsMouseDoubleClicked( 0 );
			ImGui::TextUnformatted( displayName );

			AddTooltip( command );

			if ( description ) {
				AddDescrTooltip( description );
			}

			for ( int i=1; i <= numBindings ; ++i ) {
				ImGui::TableSetColumnIndex( i );

				char selID[5];
				snprintf(selID, sizeof(selID), "##%d", i);
				selectThis = isSelected && ((selectedColumn == i) || selectedColumn == 0);
				if ( ImGui::Selectable( selID, selectThis, ImGuiSelectableFlags_AllowDoubleClick ) ) {
					if ( doubleClick ) {
						// TODO: replace this binding (or add one if this cell is empty)
					} else {
						isSelected = !selectThis;
						selectedColumn = isSelected ? i : -1;
					}
				}
				ImGui::SameLine();
				ImGui::Text( "binding %d", i );
			}
			ImGui::PopID();
			return isSelected;
		}
		return false;
	}
};

static BindingEntry bindingEntries[] = {
	{ "", "Move / Look", nullptr },
	{ "_forward",       "Forward"    , nullptr },
	{ "_back",          "Backpedal"  , "walk back" },
	{ "_moveLeft",      "Move Left"  , "strafe left" },
	{ "_moveRight",     "Move Right" , nullptr },
	{ "", "Weapons", nullptr },
	{ "_impulse0",      "Fists",  "the other kind of fisting" },
	{ "_impulse1",      "Pistol",  nullptr },
};

static void DrawBindingsMenu()
{
	ImGui::PushItemWidth(70.0f); // TODO: somehow dependent on font size and/or scale or something
	static int numBindings = 4; // TODO: in real code, save in CVar (in_maxBindingsPerCommand or sth)
	ImGui::InputInt("Number of Binding columns to show", &numBindings);
	numBindings = idMath::ClampInt(1, 10, numBindings);
	ImGui::PopItemWidth();

	ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg;

	// inTable: are we currently adding elements to a table of bindings?
	//  (we're not when adding a heading from bindingEntries: existing tables are ended
	//   before a heading and a new table is started afterwards)
	bool inTable = false;
	// did the last ImGui::BeginTable() call return true (or is it not currently visible)?
	// (init to true so the first heading before any bindings is shown)
	bool lastBeginTable = true;
	int tableNum = 1;
	static int selectedRow = -1;
	for ( int i=0; i < IM_ARRAYSIZE(bindingEntries); ++i ) {
		BindingEntry& be = bindingEntries[i];
		bool isHeading = be.IsHeading();
		if ( !isHeading && !inTable ) {
			// there is no WIP table (that has been started but not ended yet)
			// and the current element is a regular bind entry that's supposed
			// to go in a table, so start a new one
			inTable = true;
			char tableID[10];
			snprintf(tableID, sizeof(tableID), "bindTab%d", tableNum);
			++tableNum;
			lastBeginTable = ImGui::BeginTable(tableID, numBindings+1, tableFlags);
		} else if ( isHeading && inTable ) {
			// we've been adding elements to a table (unless lastBeginTable = false) that hasn't
			// been closed with EndTable() yet, but now we're at a heading so the table is done
			if ( lastBeginTable ) {
				ImGui::EndTable();
			}
			inTable = false;
		}

		if ( lastBeginTable ) { // if ImGui::BeginTable() returned false, don't draw its elements
			if ( be.Draw(numBindings, selectedRow == i) ) {
				selectedRow = i;
			}
		}
	}
	if ( inTable && lastBeginTable ) {
		// end the last binding table, if any
		ImGui::EndTable();
	}
}

static bool show_demo_window = true;

static void myWindow()
{
	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

	//ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
	ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

	DrawBindingsMenu();



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
