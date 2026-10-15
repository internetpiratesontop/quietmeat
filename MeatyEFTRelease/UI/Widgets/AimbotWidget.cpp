#include "AimbotWidget.h"

#include "../includes.h"
#include "../config.h"
#include "../globals.h"
#include "../menuLayout.h"
#include "../../Tarkov/Features/Aim/Aimbot.h"

#include <string>
#include <windows.h>

namespace uiWidgets
{
	static const char* boneNames[] = { "Head", "Chest" };

	static std::string KeyCodeToString(uint32_t keyCode)
	{
		if (keyCode == 0)
			return "Not Set";

		// Common virtual key codes
		if (keyCode >= VK_F1 && keyCode <= VK_F12)
			return std::string("F") + std::to_string(keyCode - VK_F1 + 1);

		if (keyCode == VK_LSHIFT) return "Left Shift";
		if (keyCode == VK_RSHIFT) return "Right Shift";
		if (keyCode == VK_LCONTROL) return "Left Ctrl";
		if (keyCode == VK_RCONTROL) return "Right Ctrl";
		if (keyCode == VK_LMENU) return "Left Alt";
		if (keyCode == VK_RMENU) return "Right Alt";
		if (keyCode == VK_SPACE) return "Space";
		if (keyCode == VK_TAB) return "Tab";
		if (keyCode == VK_ESCAPE) return "Escape";
		if (keyCode == VK_RETURN) return "Enter";
		if (keyCode == VK_BACK) return "Backspace";
		if (keyCode == VK_DELETE) return "Delete";
		if (keyCode == VK_INSERT) return "Insert";
		if (keyCode == VK_HOME) return "Home";
		if (keyCode == VK_END) return "End";
		if (keyCode == VK_PRIOR) return "Page Up";
		if (keyCode == VK_NEXT) return "Page Down";
		if (keyCode >= 'A' && keyCode <= 'Z')
			return std::string(1, static_cast<char>(keyCode));
		if (keyCode >= '0' && keyCode <= '9')
			return std::string(1, static_cast<char>(keyCode));

		return "Unknown";
	}

	void renderAimbotWindow()
	{
		if (!appMenu::widgetAimbot)
			return;

		if (ImGui::Begin("Aimbot Widget", &appMenu::widgetAimbot, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::SeparatorText("Main Aimbot");

			// Master enable
			ImGui::Checkbox("##aimbotEnable", &memWriteGlobals::config.combat.aimbot);
			ImGui::SameLine();
			ImGui::TextUnformatted("Enable Aimbot");

			if (memWriteGlobals::config.combat.aimbot)
			{
				ImGui::Indent();

				// Settings
				ImGui::Checkbox("##aimbotSafeLock", &memWriteGlobals::config.combat.aimbotSafeLock);
				ImGui::SameLine();
				ImGui::TextUnformatted("Safe Lock");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Only lock onto visible/safe targets");

				ImGui::Checkbox("##aimbotDisableReLock", &memWriteGlobals::config.combat.aimbotDisableReLock);
				ImGui::SameLine();
				ImGui::TextUnformatted("Disable Re-Lock");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Prevent re-locking after target loss");

				ImGui::Checkbox("##aimbotAutoBone", &memWriteGlobals::config.combat.aimbotAutoBone);
				ImGui::SameLine();
				ImGui::TextUnformatted("Auto Bone");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Automatically select optimal bone");

				ImGui::Checkbox("##aimbotHeadshotAI", &memWriteGlobals::config.combat.aimbotHeadshotAI);
				ImGui::SameLine();
				ImGui::TextUnformatted("Headshot Priority");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Prioritize headshots on AI players");

				ImGui::Checkbox("##aimbotRandomBone", &memWriteGlobals::config.combat.aimbotRandomBone);
				ImGui::SameLine();
				ImGui::TextUnformatted("Random Bone");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Randomize target bone selection");

				// Sliders
				ImGui::SliderFloat("##aimbotFOV", &memWriteGlobals::config.combat.aimbotFOV, 10.0f, 180.0f, "FOV: %.1f°");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Aiming field of view in degrees");

				ImGui::SliderFloat("##aimbotMaxDistance", &memWriteGlobals::config.combat.aimbotMaxDistance, 10.0f, 500.0f, "Distance: %.1f m");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Maximum engagement distance in meters");

				// Bone selection
				if (ImGui::Combo("##aimbotBone", reinterpret_cast<int*>(&memWriteGlobals::config.combat.aimbotBone), boneNames, IM_ARRAYSIZE(boneNames)))
				{
					// Bone selection changed
				}

				// Status
				ImGui::Separator();
				ImGui::TextUnformatted("Status:");
				ImGui::SameLine();
				if (Aimbot::Instance().IsEngaged())
				{
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ENGAGED");
				}
				else
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "IDLE");
				}

				ImGui::Unindent();
			}

			ImGui::SeparatorText("Silent Aim");

				// Enable silent aim hotkey system
				ImGui::Checkbox("##silentAimEnabled", &memWriteGlobals::config.combat.silentAimEnabled);
				ImGui::SameLine();
				ImGui::TextUnformatted("Enable Silent Aim");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Toggle silent aim on/off with a hotkey");

				if (memWriteGlobals::config.combat.silentAimEnabled)
			{
				ImGui::Indent();

				// Display current hotkey
				ImGui::TextUnformatted("Hotkey:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "%s", KeyCodeToString(memWriteGlobals::config.combat.silentAimHotkey).c_str());
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Press a key to bind\nEsc to unbind");

				// Key input for binding
				ImGui::PushItemWidth(150.0f);
				static char keyBindBuffer[32] = "Press a key...";
				ImGui::InputText("##silentAimKeyBind", keyBindBuffer, IM_ARRAYSIZE(keyBindBuffer), ImGuiInputTextFlags_ReadOnly);

				if (ImGui::IsItemActive())
				{
					// Listen for key presses
					for (int i = 0; i < 256; i++)
					{
						if (GetAsyncKeyState(i) & 0x8000)
						{
							if (i == VK_ESCAPE)
							{
								memWriteGlobals::config.combat.silentAimHotkey = 0;
							}
							else if (i != VK_LSHIFT && i != VK_RSHIFT && i != VK_LCONTROL && i != VK_RCONTROL)
							{
								memWriteGlobals::config.combat.silentAimHotkey = i;
							}
							break;
						}
					}
				}
				ImGui::PopItemWidth();

				// Status
				ImGui::TextUnformatted("Silent Aim Status:");
				ImGui::SameLine();
				if (memWriteGlobals::config.combat.silentAimActive)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "ACTIVE");
				}
				else
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "INACTIVE");
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Press the bound hotkey to toggle");

				// Silent Aim specific settings
				ImGui::Separator();
				ImGui::SliderFloat("##silentAimFOV", &memWriteGlobals::config.combat.silentAimFOV, 10.0f, 180.0f, "Silent FOV: %.1f°");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Field of view when silent aim is active");

				if (ImGui::Combo("##silentAimBone", reinterpret_cast<int*>(&memWriteGlobals::config.combat.silentAimBone), boneNames, IM_ARRAYSIZE(boneNames)))
				{
					// Silent aim bone changed
				}

				ImGui::Unindent();
			}

			ImGui::SeparatorText("Info");
			ImGui::TextDisabled("Aimbot auto-engages when:\n• Aimbot is enabled\n• Valid target found\n• Target passes validation\n\nSilent Aim temporarily disables \nfireport aim when toggled on.");

			ImGui::End();
		}
	}
}
