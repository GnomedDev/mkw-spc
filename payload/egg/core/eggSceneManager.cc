#include "eggSceneManager.hh"

extern "C" {
#include <revolution.h>
}
#include <game/host_system/SystemManager.hh>
#include <game/system/RaceConfig.hh>
#include <game/system/ResourceManager.hh>
#include <game/system/SaveManager.hh>
#include <sp/IOSDolphin.hh>
#include <sp/cs/RoomManager.hh>

namespace EGG {

bool SceneManager::s_dolphinIsUnavailable = false;
u32 SceneManager::s_dolphinSpeedStack[8];
s32 SceneManager::s_dolphinSpeedStackSize;

bool SceneManager::InitDolphinSpeed() {
    if (s_dolphinIsUnavailable) {
        return false;
    }

    if (SP::IOSDolphin::IsOpen()) {
        return true;
    }

    s_dolphinIsUnavailable = !SP::IOSDolphin::Open();
    return !s_dolphinIsUnavailable;
}

bool SceneManager::SetDolphinSpeed(u32 percent) {
    if (SP::IOSDolphin::Open()) {
        SP_LOG("Set Dolphin speed to %u", percent);
        return SP::IOSDolphin::SetSpeedLimit(percent) == IPC_OK;
    }
    return false;
}

u32 SceneManager::GetDolphinSpeedLimit() {
    if (!SP::IOSDolphin::Open()) {
        return 0;
    }

    auto q = SP::IOSDolphin::GetSpeedLimit();
    if (!q.has_value()) {
        return 0;
    }

    return *q;
}

void SceneManager::PushDolphinSpeed(u32 percent) {
    if (s_dolphinSpeedStackSize == 8) {
        SP_LOG("Max Dolphin speed stack depth reached");
        return;
    }

    const u32 oldLimit = GetDolphinSpeedLimit();
    if (oldLimit == 0) {
        SP_LOG("Failed to acquire current Dolphin speed");
        return;
    }

    if (SetDolphinSpeed(percent)) {
        s_dolphinSpeedStack[s_dolphinSpeedStackSize++] = oldLimit;
    } else {
        SP_LOG("Failed to set Dolphin speed");
    }
}

void SceneManager::PopDolphinSpeed() {
    if (s_dolphinSpeedStackSize > 0) {
        SetDolphinSpeed(s_dolphinSpeedStack[--s_dolphinSpeedStackSize]);
    }
}

void SceneManager::reinitCurrentScene() {
    SP_LOG("SceneManager::reinitCurrentScene");
    if (InitDolphinSpeed()) {
        PushDolphinSpeed(800);
    }

    auto *rc = System::RaceConfig::Instance();
    auto *saveManager = System::SaveManager::Instance();
    auto setting = saveManager->getSetting<SP::ClientSettings::Setting::TAMirror>();
    // This is a hack to get mirror TTs working. Restarting a race from mirror to non mirror causes
    // graphical bugs. Opted to reload the entire track as a simple fix.
    if (rc->raceScenario().mirror && (setting == SP::ClientSettings::TAMirror::Disable)) {
        Scene *parent = m_currScene->getParent();
        u32 sceneID = m_currScene->getSceneID();
        destroyScene(m_currScene);
        createScene(sceneID, parent);
    } else {
        REPLACED(reinitCurrentScene)();
    }

    if (InitDolphinSpeed()) {
        PopDolphinSpeed();
    }
}

void SceneManager::createScene(s32 sceneId, Scene *parent) {
    SP_LOG("SceneManager::createScene(%d)", sceneId);
    if (InitDolphinSpeed()) {
        PushDolphinSpeed(800);
    }
    System::ResourceManager::OnCreateScene(static_cast<System::RKSceneID>(sceneId));
    SP::RoomManager::OnCreateScene();
    REPLACED(createScene)(sceneId, parent);
    if (InitDolphinSpeed()) {
        PopDolphinSpeed();
    }
    SP_LOG("SceneManager::createScene(%d) done", sceneId);
}

void SceneManager::destroyScene(Scene *scene) {
    SP_LOG("SceneManager::destroyScene");
    if (InitDolphinSpeed()) {
        PushDolphinSpeed(800);
    }
    REPLACED(destroyScene)(scene);
    SP::RoomManager::OnDestroyScene();
    if (InitDolphinSpeed()) {
        PopDolphinSpeed();
    }
}

} // namespace EGG
