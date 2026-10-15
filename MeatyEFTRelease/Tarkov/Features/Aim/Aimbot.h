#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <optional>
#include <vector>

// Forward declarations
enum class boneIndex : uint32_t;

/**
 * @brief Aimbot cache for storing locked target and weapon state
 */
struct AimbotCache
{
	uint64_t lockedPlayerPtr = 0;
	uint64_t handsControllerPtr = 0;
	uint64_t fireportTransformPtr = 0;

	glm::vec3 lastTargetBonePosition{0.0f};
	glm::vec3 weaponMuzzlePosition{0.0f};

	uint32_t selectedBone = 0;  // Head
	bool isDead = false;
};

/**
 * @brief Aimbot feature for targeting and aiming at enemies
 * Implements memory-write based aimbot with configurable targeting options
 */
class Aimbot
{
public:
	static Aimbot& Instance();

	Aimbot(const Aimbot&) = delete;
	Aimbot& operator=(const Aimbot&) = delete;

	// Initialization and lifecycle
	bool Initialize();
	void Shutdown();
	void Update();

	// Configuration getters
	bool IsEnabled() const;
	bool IsSafeLocked() const;
	bool IsAutobone() const;
	bool IsHeadshotAI() const;
	bool IsRandomBone() const;
	bool DisableReLock() const;
	float GetFOV() const;
	float GetMaxDistance() const;  // Get max engagement distance
	bool IsEngaged() const { return engaged_.load(); }

	// Target management
	void TargetPlayer(uint64_t playerPtr);
	void ClearTarget();
	uint64_t GetCurrentTarget() const { return current_target_; }

	// Silent aim control (for macro/hotkey integration)
	void SetSilentAimMode(bool enabled);
	bool IsSilentAimMode() const { return silent_aim_mode_.load(); }

	// Aimbot operation
	void ApplyAim();
	void RestoreAim();

private:
	// Target scanning
	uint64_t ScanForTargets();  // Finds best valid target from registered players
	std::vector<uint64_t> GetValidTargets();  // Gets all valid targets meeting criteria
	Aimbot();
	~Aimbot();

	// Worker thread for background aimbot updates
	void AimbotWorkerThread();

	// Bone selection
	uint32_t SelectTargetBone(uint64_t playerPtr);
	std::vector<uint32_t> GetValidBones() const;

	// Helper functions
	bool ValidateTarget(uint64_t playerPtr);
	bool IsTargetAlive(uint64_t playerPtr);
	bool IsTargetVisible(uint64_t playerPtr);

	glm::vec3 GetTargetBonePosition(uint64_t playerPtr, uint32_t boneIndex);
	glm::vec3 GetWeaponMuzzlePosition();

	bool CalculateAimDirection(const glm::vec3& targetPos, glm::vec3& outDirection);
	bool ApplyAimPatch(const glm::vec3& aimDirection);

	// State
	std::atomic<bool> enabled_{false};
	std::atomic<bool> engaged_{false};
	std::atomic<uint64_t> current_target_{0};
	std::atomic<bool> silent_aim_mode_{false};  // When true, fireport aim is disabled and silent aim is used

	std::thread worker_thread_;
	std::atomic<bool> running_{false};
	mutable std::mutex target_mutex_;
	mutable std::mutex cache_mutex_;

	// Last known aim state for restoration
	glm::vec3 last_aim_direction_{0.0f};
	bool aim_patched_{false};
	bool last_hotkey_state_{false};  // For debouncing hotkey presses
	std::chrono::steady_clock::time_point last_hotkey_toggle_time_;

	// Cache management
	AimbotCache cache_;
	bool first_lock_{false};
};
