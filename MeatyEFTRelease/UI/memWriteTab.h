#pragma once
#include <cstdint>
#include <string>

namespace memWriteGlobals
{
	// ===== COMBAT FEATURES - AIMBOT ONLY =====
	struct CombatSettings
	{
		bool aimbot = false;
		bool aimbotSafeLock = false;
		bool aimbotDisableReLock = false;
		bool aimbotAutoBone = false;
		bool aimbotHeadshotAI = false;
		bool aimbotRandomBone = false;
		float aimbotFOV = 45.0f;
		float aimbotMaxDistance = 300.0f;  // Max engagement distance in meters
		uint32_t aimbotBone = 0;  // Target bone: 0=head, 1=chest

		// Silent aim hotkey binding
		bool silentAimEnabled = false;
		uint32_t silentAimHotkey = 0;  // Virtual key code (VK_F key for example)
		bool silentAimActive = false;  // Current toggle state when hotkey pressed
		float silentAimFOV = 45.0f;  // Independent FOV for silent aim
		uint32_t silentAimBone = 0;  // Independent bone for silent aim
		bool silentAimShowFovRing = true;  // Whether to show FOV ring on Fuser overlay
	};

	// Master configuration struct (AIM-only)
	struct MemWriteConfig
	{
		bool enabled = false;  // Global enable/disable for aimbot

		CombatSettings combat;
	};

	// Global instance
	inline MemWriteConfig config;
}
