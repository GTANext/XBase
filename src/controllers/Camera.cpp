#include <XBase/Camera.h>
#include <XBase/Hooks.h>
#include <XBase/Input.h>

#include "plugin.h"
#include "extensions/ScriptCommands.h"
#include "CCamera.h"
#include "CPlayerPed.h"
#include "CPad.h"
#include "CPools.h"
#include "CVehicle.h"
#include "CWorld.h"

#include <algorithm>
#include <cmath>

namespace {

using XBase::Camera::Mode;
using XBase::Camera::Settings;

Mode s_mode = Mode::Disabled;
Settings s_settings;

CPed* s_freecamPed = nullptr;
int s_freecamPedHandle = -1;
float s_yaw = 0.0f;
float s_pitch = 0.0f;
unsigned char s_hudState = 1;
unsigned char s_radarState = 0;
float s_previousFov = 70.0f;
bool s_freecamSnapshotOwned = false;

float s_previousGenerationDistance = 1.0f;
float s_previousLodDistance = 1.0f;
float s_topDownOffset = 40.0f;
bool s_topDownSnapshotOwned = false;

constexpr float Pi = 3.14159265358979323846f;

void RestoreCamera() {
    plugin::Command<plugin::Commands::CAMERA_PERSIST_FOV>(false);
    plugin::Command<plugin::Commands::RESTORE_CAMERA_JUMPCUT>();
}

void DisableFreecam() {
    plugin::Command<plugin::Commands::SET_EVERYONE_IGNORE_PLAYER>(0, false);

    if (s_freecamPedHandle >= 0) {
        plugin::Command<plugin::Commands::DELETE_CHAR>(s_freecamPedHandle);
    }
    s_freecamPed = nullptr;
    s_freecamPedHandle = -1;

    if (s_freecamSnapshotOwned) {
        plugin::patch::Set<unsigned char>(0xBA6769, s_hudState);
        plugin::patch::Set<unsigned char>(0xBA676C, s_radarState);
        TheCamera.LerpFOV(TheCamera.FindCamFOV(), s_previousFov, 250, true);
        RestoreCamera();
        s_freecamSnapshotOwned = false;
    }
}

bool EnableFreecam() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) return false;

    s_hudState = plugin::patch::Get<unsigned char>(0xBA6769);
    s_radarState = plugin::patch::Get<unsigned char>(0xBA676C);
    s_previousFov = TheCamera.FindCamFOV();
    s_freecamSnapshotOwned = true;

    plugin::Command<plugin::Commands::SET_EVERYONE_IGNORE_PLAYER>(0, true);
    plugin::patch::Set<unsigned char>(0xBA6769, 0);
    plugin::patch::Set<unsigned char>(0xBA676C, 2);

    CVector position = player->GetPosition();
    plugin::Command<plugin::Commands::CREATE_RANDOM_CHAR>(
        position.x, position.y, position.z, &s_freecamPedHandle);
    s_freecamPed = CPools::GetPed(s_freecamPedHandle);
    if (!s_freecamPed) {
        DisableFreecam();
        return false;
    }

    s_freecamPed->bIsVisible = false;
    s_yaw = player->GetHeading() + 89.6f;
    s_pitch = 0.0f;
    position.z -= 20.0f;
    s_freecamPed->SetPosn(position);

    plugin::Command<plugin::Commands::FREEZE_CHAR_POSITION_AND_DONT_LOAD_COLLISION>(
        s_freecamPedHandle, true);
    plugin::Command<plugin::Commands::SET_LOAD_COLLISION_FOR_CHAR_FLAG>(s_freecamPedHandle, false);
    plugin::Command<plugin::Commands::SET_CHAR_COLLISION>(s_freecamPedHandle, false);
    TheCamera.LerpFOV(TheCamera.FindCamFOV(), s_settings.freecamFov, 250, true);
    plugin::Command<plugin::Commands::CAMERA_PERSIST_FOV>(true);
    return true;
}

void DisableTopDown() {
    if (!s_topDownSnapshotOwned) return;
    TheCamera.m_fGenerationDistMultiplier = s_previousGenerationDistance;
    TheCamera.m_fLODDistMultiplier = s_previousLodDistance;
    RestoreCamera();
    s_topDownSnapshotOwned = false;
}

bool EnableTopDown() {
    if (!FindPlayerPed()) return false;
    s_previousGenerationDistance = TheCamera.m_fGenerationDistMultiplier;
    s_previousLodDistance = TheCamera.m_fLODDistMultiplier;
    s_topDownOffset = static_cast<float>(s_settings.topDownZoom);
    s_topDownSnapshotOwned = true;
    return true;
}

void ProcessFreecam() {
    if (!s_freecamPed) {
        DisableFreecam();
        s_mode = Mode::Disabled;
        return;
    }

    const float deltaSeconds = std::clamp(XBase::Hooks::GetFrameDeltaSeconds(), 0.001f, 0.1f);
    float speed = static_cast<float>(s_settings.freecamSpeed) * 30.0f * deltaSeconds;
    CVector position = s_freecamPed->GetPosition();

    s_yaw -= CPad::NewMouseControllerState.x / 250.0f;
    s_pitch += CPad::NewMouseControllerState.y / 3.0f;
    s_yaw = std::clamp(s_yaw, -150.0f, 150.0f);
    s_pitch = std::clamp(s_pitch, -270.0f, 270.0f);

    if (XBase::Input::WasPressed(XBase::Input::Key::Enter)) {
        CPlayerPed* player = FindPlayerPed();
        if (player) {
            CEntity* playerEntity = player;
            CVector destination = position;
            destination.z = CWorld::FindGroundZFor3DCoord(
                destination.x, destination.y, 1000.0f, nullptr, &playerEntity) + 0.5f;
            plugin::Command<plugin::Commands::SET_CHAR_COORDINATES>(
                CPools::GetPedRef(player), destination.x, destination.y, destination.z);
        }
    }

    if (XBase::Input::IsModifierDown(XBase::Input::Modifier::Alt)
        && s_settings.freecamSpeed > 1) {
        speed *= 0.5f;
    }
    if (XBase::Input::IsModifierDown(XBase::Input::Modifier::Shift)) {
        speed *= 2.0f;
    }

    const bool forward = XBase::Input::IsDown(XBase::Input::Key::W);
    const bool backward = XBase::Input::IsDown(XBase::Input::Key::S);
    if (forward || backward) {
        const float direction = backward ? -1.0f : 1.0f;
        float heading = 0.0f;
        plugin::Command<plugin::Commands::GET_CHAR_HEADING>(s_freecamPedHandle, &heading);
        if (XBase::Input::IsModifierDown(XBase::Input::Modifier::Ctrl)) {
            position.z += speed * direction;
        } else {
            const float headingRadians = heading * Pi / 180.0f;
            position.x += direction * speed * std::cos(headingRadians);
            position.y += direction * speed * std::sin(headingRadians);
            if (!XBase::Input::IsDown(XBase::Input::Key::Space)) {
                position.z += direction * speed * 2.0f * std::sin(s_pitch / 3.0f * Pi / 180.0f);
            }
        }
    }

    const bool left = XBase::Input::IsDown(XBase::Input::Key::A);
    const bool right = XBase::Input::IsDown(XBase::Input::Key::D);
    if (left || right) {
        float heading = 0.0f;
        plugin::Command<plugin::Commands::GET_CHAR_HEADING>(s_freecamPedHandle, &heading);
        const float direction = left ? -1.0f : 1.0f;
        const float strafeRadians = (heading - 90.0f) * Pi / 180.0f;
        position.x += direction * speed * std::cos(strafeRadians);
        position.y += direction * speed * std::sin(strafeRadians);
    }

    const float wheel = XBase::Hooks::ConsumeWheelDelta();
    if (wheel != 0.0f) {
        if (XBase::Input::IsModifierDown(XBase::Input::Modifier::Ctrl)) {
            s_settings.freecamFov = std::clamp(
                s_settings.freecamFov - (wheel > 0.0f ? 2.0f : -2.0f), 10.0f, 115.0f);
            TheCamera.LerpFOV(TheCamera.FindCamFOV(), s_settings.freecamFov, 250, true);
            plugin::Command<plugin::Commands::CAMERA_PERSIST_FOV>(true);
        } else {
            s_settings.freecamSpeed = std::clamp(
                s_settings.freecamSpeed + (wheel > 0.0f ? 1 : -1), 1, 10);
        }
    }

    s_freecamPed->SetHeading(s_yaw);
    plugin::Command<plugin::Commands::ATTACH_CAMERA_TO_CHAR>(
        s_freecamPedHandle, 0.0, 0.0, 20.0, 90.0, 180, s_pitch, 0.0, 2);
    s_freecamPed->SetPosn(position);
    plugin::Call<0x4045B0>(&position);
}

void ProcessTopDown() {
    CPlayerPed* player = FindPlayerPed();
    if (!player) {
        DisableTopDown();
        s_mode = Mode::Disabled;
        return;
    }

    CVector position = player->GetPosition();
    float targetOffset = static_cast<float>(s_settings.topDownZoom);
    plugin::Command<plugin::Commands::SET_PLAYER_DRUNKENNESS>(0, 0);

    if (CVehicle* vehicle = FindPlayerVehicle(-1, false)) {
        targetOffset += std::min(vehicle->m_vecMoveSpeed.Magnitude() * 35.0f, 40.0f);
    }
    s_topDownOffset += (targetOffset - s_topDownOffset) * 0.05f;

    const CVector playerOffset(position.x, position.y, position.z + 2.0f);
    const CVector cameraPosition(playerOffset.x, playerOffset.y, playerOffset.z + s_topDownOffset);
    CColPoint collisionPoint;
    CEntity* collisionEntity = nullptr;
    const CVector fixedPosition = CWorld::ProcessLineOfSight(
        playerOffset, cameraPosition, collisionPoint, collisionEntity,
        true, true, true, true, true, true, true, true)
        ? collisionPoint.m_vecPoint
        : cameraPosition;

    plugin::Command<plugin::Commands::SET_FIXED_CAMERA_POSITION>(
        fixedPosition.x, fixedPosition.y, fixedPosition.z, 0.0f, 0.0f, 0.0f);
    plugin::Command<plugin::Commands::POINT_CAMERA_AT_POINT>(position.x, position.y, position.z, 2);
    TheCamera.m_fGenerationDistMultiplier = 10.0f;
    TheCamera.m_fLODDistMultiplier = 10.0f;
}

} // namespace

namespace XBase::Camera {

bool SetMode(Mode mode) {
    if (mode == s_mode) return true;

    if (s_mode == Mode::Freecam) DisableFreecam();
    if (s_mode == Mode::TopDown) DisableTopDown();
    s_mode = Mode::Disabled;

    bool enabled = true;
    if (mode == Mode::Freecam) enabled = EnableFreecam();
    if (mode == Mode::TopDown) enabled = EnableTopDown();
    if (enabled) s_mode = mode;
    return enabled;
}

Mode GetMode() {
    return s_mode;
}

bool IsActive() {
    return s_mode != Mode::Disabled;
}

void SetSettings(const Settings& settings) {
    s_settings.freecamFov = std::clamp(settings.freecamFov, 10.0f, 115.0f);
    s_settings.freecamSpeed = std::clamp(settings.freecamSpeed, 1, 10);
    s_settings.topDownZoom = std::clamp(settings.topDownZoom, 10, 100);
    if (s_mode == Mode::Freecam) {
        TheCamera.LerpFOV(TheCamera.FindCamFOV(), s_settings.freecamFov, 250, true);
        plugin::Command<plugin::Commands::CAMERA_PERSIST_FOV>(true);
    }
}

Settings GetSettings() {
    return s_settings;
}

void NotifyGameInit() {
    SetMode(Mode::Disabled);
}

void Process() {
    if (s_mode == Mode::Freecam) ProcessFreecam();
    if (s_mode == Mode::TopDown) ProcessTopDown();
}

void Shutdown() {
    SetMode(Mode::Disabled);
}

} // namespace XBase::Camera