#include "Aimbot.h"
#include "Ballistics.h"
#include "ReadOnlyAim.h"
#include "FireportTracker.h"
#include "../../memory/Memory.h"
#include "../../GameWorld/MainGame.h"
#include "../../SDK/EftOffsets.h"
#include "../../GameWorld/Player/LocalPlayer.h"
#include "../../GameWorld/Player/ObservedPlayer.h"
#include "../../GameWorld/RegisteredPlayers.h"
#include "../../UI/globals.h"
#include "../../UI/memWriteTab.h"
#include "../../../Core/Utilities.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <cmath>
#include <iostream>
#include <chrono>
#include <windows.h>

extern Memory mem;

Aimbot& Aimbot::Instance()
{
	static Aimbot instance;
	return instance;
}

Aimbot::Aimbot() : running_(false), aim_patched_(false), last_hotkey_state_(false),
	last_hotkey_toggle_time_(std::chrono::steady_clock::now())
{
}

Aimbot::~Aimbot()
{
	Shutdown();
}

bool Aimbot::Initialize()
{
	if (running_)
		return true;

	running_ = true;
	try {
		worker_thread_ = std::thread(&Aimbot::AimbotWorkerThread, this);
		std::cout << "Aimbot: Initialized successfully\n";
		return true;
	} catch (const std::exception& ex) {
		std::cerr << "Aimbot: Failed to initialize - " << ex.what() << "\n";
		running_ = false;
		return false;
	}
}

void Aimbot::Shutdown()
{
	if (!running_)
		return;

	running_ = false;
	if (worker_thread_.joinable()) {
		worker_thread_.join();
	}
	RestoreAim();
	std::cout << "Aimbot: Shutdown complete\n";
}

bool Aimbot::IsEnabled() const
{
	return memWriteGlobals::config.combat.aimbot || IsSilentAimMode();
}

bool Aimbot::IsSafeLocked() const
{
	return memWriteGlobals::config.combat.aimbotSafeLock;
}

bool Aimbot::IsAutobone() const
{
	return memWriteGlobals::config.combat.aimbotAutoBone;
}

bool Aimbot::IsHeadshotAI() const
{
	return memWriteGlobals::config.combat.aimbotHeadshotAI;
}

bool Aimbot::IsRandomBone() const
{
	return memWriteGlobals::config.combat.aimbotRandomBone;
}

bool Aimbot::DisableReLock() const
{
	return memWriteGlobals::config.combat.aimbotDisableReLock;
}

float Aimbot::GetFOV() const
{
	return memWriteGlobals::config.combat.aimbotFOV;
}

float Aimbot::GetMaxDistance() const
{
	return memWriteGlobals::config.combat.aimbotMaxDistance;
}

void Aimbot::Update()
{
	// Check for silent aim hotkey hold - do this regardless of aimbot enabled
	auto& config = memWriteGlobals::config;
	bool keyPressed = false;
	if (config.combat.silentAimEnabled && config.combat.silentAimHotkey > 0) {
		if (c_keys* keyboard = mem.GetKeyboard(); keyboard && keyboard->IsReady()) {
			keyPressed = keyboard->IsKeyDown(config.combat.silentAimHotkey);
		}

		if (keyPressed != last_hotkey_state_) {
			config.combat.silentAimActive = keyPressed;
			SetSilentAimMode(keyPressed);
		}
		last_hotkey_state_ = keyPressed;
	}
	else if (last_hotkey_state_ || config.combat.silentAimActive || IsSilentAimMode()) {
		last_hotkey_state_ = false;
		config.combat.silentAimActive = false;
		SetSilentAimMode(false);
	}

	if (!running_ || !IsEnabled())
		return;

	// Main thread update - can be called from render loop
}

void Aimbot::TargetPlayer(uint64_t playerPtr)
{
	if (ValidateTarget(playerPtr)) {
		std::lock_guard<std::mutex> lock(target_mutex_);
		current_target_.store(playerPtr);
		engaged_.store(true);
	}
}

void Aimbot::ClearTarget()
{
	{
		std::lock_guard<std::mutex> lock(target_mutex_);
		current_target_.store(0);
	}
	engaged_.store(false);
	RestoreAim();
}

bool Aimbot::ValidateTarget(uint64_t playerPtr)
{
	if (!playerPtr)
		return false;

	// Exclude self
	if (playerPtr == MainGame::localPlayerPtr)
		return false;

	// Check if player is alive
	if (!IsTargetAlive(playerPtr))
		return false;

	// Check visibility if safe lock is enabled
	if (IsSafeLocked() && !IsTargetVisible(playerPtr))
		return false;

	// Safety additional checks could go here:
	// - Team/group filtering if needed
	// - Player type filtering (exclude BTR operators, etc.)
	// - Distance sanity check

	return true;
}

bool Aimbot::IsTargetAlive(uint64_t playerPtr)
{
	if (!playerPtr)
		return false;

	try {
		// For now, assume all valid pointers represent alive players
		// A complete implementation would:
		// 1. Read Player.Health or PlayerHealthController
		// 2. Check for IsAlive flag
		// 3. Verify player state is not "dead" or "unconscious"
		// 
		// This is deferred because health offset varies by game update
		// Using read validation of player position as implicit alive check
		glm::vec3 pos(0.0f);
		bool canReadPos = mem.Read(playerPtr + 0x1A0, &pos, sizeof(glm::vec3));
		return canReadPos && glm::length(pos) > 0.01f;
	}
	catch (...) {
		return false;
	}
}

bool Aimbot::IsTargetVisible(uint64_t playerPtr)
{
	// Use raycast or existing visibility check
	// This would use the game's line-of-sight checks
	return true;
}

uint32_t Aimbot::SelectTargetBone(uint64_t playerPtr)
{
	auto validBones = GetValidBones();
	if (validBones.empty())
		return 0;  // Default to head

	// Random bone selection override
	if (IsRandomBone()) {
		return validBones[rand() % validBones.size()];
	}

	// Headshot priority for AI
	if (IsHeadshotAI()) {
		return 0;  // Head bone (HumanHead = 133, but using index 0)
	}

	// Auto-bone selection based on visibility and target position
	if (IsAutobone()) {
		// Try to select visible bone - prefer head, fall back to chest
		// In reality would check which bone is most visible from camera
		glm::vec3 headPos = GetTargetBonePosition(playerPtr, 0);
		if (headPos != glm::vec3(0.0f)) {
			return 0;  // Head is visible
		}
		return 1;  // Fall back to chest
	}

	// Default: Head
	return 0;
}

std::vector<uint32_t> Aimbot::GetValidBones() const
{
	// Return only head (0) and chest (1) bones as requested
	return {0, 1};
}

glm::vec3 Aimbot::GetTargetBonePosition(uint64_t playerPtr, uint32_t boneIndex)
{
	if (!playerPtr)
		return glm::vec3(0.0f);

	try {
		uint64_t playerBonesPtr = 0;
		if (!mem.Read(playerPtr + sdk::Player::PlayerBones, &playerBonesPtr, sizeof(uint64_t)))
			return glm::vec3(0.0f);

		if (!playerBonesPtr)
			return glm::vec3(0.0f);

		// Bones are stored as an array/dictionary
		// We need to read the bone transform at index
		// Head = 0, Chest/Ribcage = 2
		// Each bone is a UnityTransform pointer + transform data

		// Simplified: for head, use offset 0x0, for chest use next offset
		// In practice, bones would be indexed through a collection
		uint64_t boneTransformPtr = 0;

		if (boneIndex == 0) {  // Head
			// Offset to head bone transform - would be in PlayerBones structure
			boneTransformPtr = playerBonesPtr;  // Placeholder
		} else if (boneIndex == 1) {  // Chest/Ribcage
			if (!mem.Read(playerBonesPtr + sdk::PlayerBones::Ribcage, &boneTransformPtr, sizeof(uint64_t)))
				return glm::vec3(0.0f);
		}

		if (!boneTransformPtr)
			return glm::vec3(0.0f);

		// Read transform position
		// Unity Transform world position would be at specific offset
		glm::vec3 position(0.0f);
		if (!mem.Read(boneTransformPtr + 0x18, &position, sizeof(glm::vec3)))
			return glm::vec3(0.0f);

		return position;
	}
	catch (...) {
		return glm::vec3(0.0f);
	}
}

glm::vec3 Aimbot::GetWeaponMuzzlePosition()
{
	// Get local player and their weapon fireport position
	if (!MainGame::localPlayerPtr)
		return glm::vec3(0.0f);

	try {
		uint64_t handsControllerPtr = 0;
		if (!mem.Read(MainGame::localPlayerPtr + sdk::Player::_handsController, &handsControllerPtr, sizeof(uint64_t)))
			return glm::vec3(0.0f);

		if (!handsControllerPtr)
			return glm::vec3(0.0f);

		uint64_t fireportPtr = 0;
		if (!mem.Read(handsControllerPtr + sdk::FirearmController::Fireport, &fireportPtr, sizeof(uint64_t)))
			return glm::vec3(0.0f);

		if (!fireportPtr)
			return glm::vec3(0.0f);

		// Read fireport transform position
		glm::vec3 muzzlePos(0.0f);
		if (!mem.Read(fireportPtr + 0x18, &muzzlePos, sizeof(glm::vec3)))
			return glm::vec3(0.0f);

		return muzzlePos;
	}
	catch (...) {
		return glm::vec3(0.0f);
	}
}

bool Aimbot::CalculateAimDirection(const glm::vec3& targetPos, glm::vec3& outDirection)
{
	glm::vec3 muzzlePos = GetWeaponMuzzlePosition();
	if (glm::length(muzzlePos) == 0.0f)
		return false;

	glm::vec3 dir = glm::normalize(targetPos - muzzlePos);

	// Apply FOV limiting
	float fovLimit = GetFOV();
	if (fovLimit > 0.0f && fovLimit < 180.0f) {
		// For FOV limiting, we need to check angle from player's view direction
		// Simplified: For now, we accept targets that are aimed at
		// A proper implementation would:
		// 1. Read camera forward vector from game state
		// 2. Calculate angle between camera forward and target direction
		// 3. Reject if angle > FOV limit

		// For now, FOV limit just ensures we have a reasonable aiming cone
		// The actual FOV check would be more complex with camera view matrix
		// This is a placeholder that always accepts targets in valid range
		if (fovLimit <= 0.0f || fovLimit > 180.0f) {
			// Invalid FOV, reject
			return false;
		}
	}

	outDirection = dir;
	return true;
}

bool Aimbot::ApplyAimPatch(const glm::vec3& aimDirection)
{
	if (aim_patched_)
		return true;

	uint64_t target = current_target_.load();
	if (!target || !MainGame::localPlayerPtr)
		return false;

	try {
		uint64_t handsControllerPtr = 0;
		if (!mem.Read(MainGame::localPlayerPtr + sdk::Player::_handsController, &handsControllerPtr, sizeof(uint64_t)))
			return false;

		if (!handsControllerPtr)
			return false;

		uint64_t fireportPtr = 0;
		if (!mem.Read(handsControllerPtr + sdk::FirearmController::Fireport, &fireportPtr, sizeof(uint64_t)))
			return false;

		if (!fireportPtr)
			return false;

		// Write the aim direction to fireport transform rotation
		// This writes directly to the weapon's forward direction
		// Using WriteValue template for type safety
		if (mem.WriteValue<glm::vec3>(fireportPtr + 0x18, aimDirection)) {
			last_aim_direction_ = aimDirection;
			aim_patched_ = true;
			return true;
		}

		return false;
	}
	catch (...) {
		return false;
	}
}

void Aimbot::ApplyAim()
{
	if (aim_patched_)
		return;

	uint64_t target = current_target_.load();
	if (!target || !MainGame::localPlayerPtr)
		return;

	// Select target bone
	uint32_t targetBone = SelectTargetBone(target);

	glm::vec3 targetPos = GetTargetBonePosition(target, targetBone);
	if (glm::length(targetPos) == 0.0f)
		return;

	glm::vec3 aimDir;
	if (CalculateAimDirection(targetPos, aimDir)) {
		ApplyAimPatch(aimDir);
	}
}

void Aimbot::RestoreAim()
{
	if (!aim_patched_)
		return;

	if (!MainGame::localPlayerPtr)
	{
		aim_patched_ = false;
		return;
	}

	try {
		uint64_t handsControllerPtr = 0;
		if (!mem.Read(MainGame::localPlayerPtr + sdk::Player::_handsController, &handsControllerPtr, sizeof(uint64_t)))
		{
			aim_patched_ = false;
			return;
		}

		uint64_t fireportPtr = 0;
		if (!mem.Read(handsControllerPtr + sdk::FirearmController::Fireport, &fireportPtr, sizeof(uint64_t)))
		{
			aim_patched_ = false;
			return;
		}

		if (!fireportPtr)
		{
			aim_patched_ = false;
			return;
		}

		// Restore by writing zero vector to disable aim override
		glm::vec3 restored(0.0f);
		mem.WriteValue<glm::vec3>(fireportPtr + 0x18, restored);
		aim_patched_ = false;
	}
	catch (...) {
		aim_patched_ = false;
	}
}

void Aimbot::AimbotWorkerThread()
{
	std::cout << "Aimbot worker thread started\n";

	while (running_) {
		try {
			// Check if we're in a raid
			if (!MainGame::localPlayerPtr || MainGame::localPlayerPtr == 0) {
				engaged_.store(false);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			if (IsEnabled()) {
				uint64_t target = current_target_.load();

				if (target && ValidateTarget(target)) {
					ApplyAim();
					engaged_.store(true);
				} else if (!DisableReLock()) {
					// Scan for new target
					uint64_t newTarget = ScanForTargets();
					if (newTarget) {
						TargetPlayer(newTarget);
						ApplyAim();
						engaged_.store(true);
					} else {
						engaged_.store(false);
					}
				} else {
					engaged_.store(false);
				}
			} else {
				RestoreAim();
				engaged_.store(false);
			}
		} catch (const std::exception& ex) {
			std::cerr << "Aimbot worker error: " << ex.what() << "\n";
			engaged_.store(false);
		}

			std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
			}

			std::cout << "Aimbot worker thread shutting down\n";
		}

		void Aimbot::SetSilentAimMode(bool enabled)
		{
			silent_aim_mode_.store(enabled);

			// If disabling silent aim, restore normal fireport aim
			if (!enabled && aim_patched_) {
				RestoreAim();
			}
		}


uint64_t Aimbot::ScanForTargets()
{
	if (!MainGame::localPlayerPtr)
		return 0;

	// Get the local player location and view direction
	glm::vec3 localPos = MainGame::localLocation;
	glm::vec2 localRotation = MainGame::localRotation;
	float maxDistance = GetMaxDistance();
	float fovLimit = GetFOV();

	// Use ReadOnlyAim to find best target if available
	// ReadOnlyAim applies its own sophisticated targeting logic
	auto bestTarget = readOnlyAim.getLiveTarget();
	if (bestTarget.has_value()) {
		uint64_t instance = bestTarget.value().player.instance;
		// Verify it's still valid before returning
		if (instance && ValidateTarget(instance)) {
			return instance;
		}
	}

	// Fallback: scan valid targets and prefer the closest one to the screen center.
	uint64_t bestTargetPtr = 0;
	float bestScore = std::numeric_limits<float>::max();
	const bool silentAimActive = IsSilentAimMode();
	const glm::vec3 muzzlePos = silentAimActive ? GetWeaponMuzzlePosition() : glm::vec3(0.0f);
	const glm::vec3 viewDir = glm::normalize(glm::vec3(
		std::cos(glm::radians(localRotation.y)) * std::cos(glm::radians(localRotation.x)),
		std::sin(glm::radians(localRotation.x)),
		std::sin(glm::radians(localRotation.y)) * std::cos(glm::radians(localRotation.x))));

	// Scan through player buffer in MainGame
	for (int i = 0; i < MainGame::registeredPlayersCount; i++) {
		uint64_t playerPtr = MainGame::player_buffer[i];
		if (!playerPtr || playerPtr == MainGame::localPlayerPtr)
			continue;

		glm::vec3 targetPos(0.0f);
		if (!mem.Read(playerPtr + 0x1A0, &targetPos, sizeof(glm::vec3)))
			continue;

		const float dist = glm::distance(localPos, targetPos);
		if (dist > maxDistance || dist < 0.1f)
			continue;

		if (!ValidateTarget(playerPtr))
			continue;

		float score = dist;
		if (silentAimActive) {
			if (glm::length(muzzlePos) == 0.0f)
				continue;

			const glm::vec3 toTarget = glm::normalize(targetPos - muzzlePos);
			const float angle = glm::degrees(std::acos(glm::clamp(glm::dot(viewDir, toTarget), -1.0f, 1.0f)));
			if (angle > memWriteGlobals::config.combat.silentAimFOV)
				continue;

			score = angle;
		}

		if (score < bestScore) {
			bestScore = score;
			bestTargetPtr = playerPtr;
		}
	}

	return bestTargetPtr;
}

std::vector<uint64_t> Aimbot::GetValidTargets()
{
	std::vector<uint64_t> validTargets;

	if (!MainGame::localPlayerPtr)
		return validTargets;

	glm::vec3 localPos = MainGame::localLocation;
	float maxDistance = GetMaxDistance();

	// Iterate through registered players
	for (int i = 0; i < MainGame::registeredPlayersCount; i++) {
		uint64_t playerPtr = MainGame::player_buffer[i];
		if (!playerPtr || playerPtr == MainGame::localPlayerPtr)
			continue;

		// Read player position
		glm::vec3 targetPos(0.0f);
		if (!mem.Read(playerPtr + 0x1A0, &targetPos, sizeof(glm::vec3)))
			continue;

		// Check distance
		float dist = glm::distance(localPos, targetPos);
		if (dist <= maxDistance && dist > 0.1f) {
			// Validate target (alive, visibility if safe lock enabled)
			if (ValidateTarget(playerPtr)) {
				validTargets.push_back(playerPtr);
			}
		}
	}

	return validTargets;
}
